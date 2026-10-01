#include "railbridge.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
static void rb_sleep_ms(int ms) { Sleep(ms); }
#else
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/eventfd.h>
static void rb_sleep_ms(int ms) { usleep(ms * 1000); }
#endif

#define RB_KEEPALIVE_NS 8000000ull /* 8 ms — Switch 1 0x30 cadence, no 1 ms spin */

static volatile sig_atomic_t g_run = 1;
static void on_sig(int s) {
    (void)s;
    g_run = 0;
}

typedef struct {
    atomic_uint seq;
    rb_pad_t pad;
    uint64_t last_ns;
    uint32_t frames;
    atomic_int present;
} rb_slot_t;

static rb_slot_t g_slots[RB_MAX_PADS];
static rb_config_t g_cfg;
static rb_s1_machine_t g_s1[RB_MAX_HIDG];
static atomic_int g_n_pads;
static atomic_int g_s2_state;

static rb_rumble_t g_rumble[RB_MAX_PADS];
static atomic_uint g_rumble_seq[RB_MAX_PADS];
static unsigned g_rumble_seen[RB_MAX_PADS];

#ifndef _WIN32
static int g_pad_efd = -1;
#endif

static void wake_readers(void) {
#ifndef _WIN32
    if (g_pad_efd >= 0) {
        uint64_t one = 1;
        (void)!write(g_pad_efd, &one, sizeof(one));
    }
#endif
}

static void rumble_publish(int slot, const rb_rumble_t *r) {
    if (slot < 0 || slot >= RB_MAX_PADS || !r) return;
    g_rumble[slot] = *r;
    atomic_fetch_add_explicit(&g_rumble_seq[slot], 1, memory_order_release);
    wake_readers();
}

static int rumble_take(int slot, rb_rumble_t *r) {
    if (slot < 0 || slot >= RB_MAX_PADS) return 0;
    unsigned s = atomic_load_explicit(&g_rumble_seq[slot], memory_order_acquire);
    if (s == g_rumble_seen[slot]) return 0;
    g_rumble_seen[slot] = s;
    *r = g_rumble[slot];
    return 1;
}

static void slot_write(int slot, const rb_pad_t *p) {
    if (slot < 0 || slot >= RB_MAX_PADS) return;
    rb_slot_t *s = &g_slots[slot];
    unsigned seq = atomic_load_explicit(&s->seq, memory_order_relaxed);
    atomic_store_explicit(&s->seq, seq + 1, memory_order_release);
    s->pad = *p;
    s->pad.slot = (uint8_t)slot;
    s->last_ns = rb_now_ns();
    s->frames++;
    atomic_store_explicit(&s->present, p->valid ? 1 : 0, memory_order_release);
    atomic_store_explicit(&s->seq, seq + 2, memory_order_release);
    wake_readers();
}

static int slot_read(int slot, rb_pad_t *p, uint64_t *age_ns) {
    if (slot < 0 || slot >= RB_MAX_PADS) return 0;
    rb_slot_t *s = &g_slots[slot];
    for (int i = 0; i < 8; i++) {
        unsigned s1 = atomic_load_explicit(&s->seq, memory_order_acquire);
        if (s1 & 1) continue;
        *p = s->pad;
        uint64_t t = s->last_ns;
        unsigned s2 = atomic_load_explicit(&s->seq, memory_order_acquire);
        if (s1 == s2) {
            *age_ns = t ? rb_now_ns() - t : 0;
            return p->valid;
        }
    }
    return 0;
}

static int count_present(void) {
    int n = 0;
    for (int i = 0; i < RB_MAX_PADS; i++) {
        if (atomic_load(&g_slots[i].present)) n++;
    }
    return n;
}

static int pad_dirty(const rb_pad_t *a, const rb_pad_t *b) {
    return a->lx != b->lx || a->ly != b->ly || a->rx != b->rx || a->ry != b->ry ||
           a->lt != b->lt || a->rt != b->rt || a->buttons[0] != b->buttons[0] ||
           a->buttons[1] != b->buttons[1] || a->buttons[2] != b->buttons[2] ||
           a->valid != b->valid;
}

static void print_usage(void) {
    puts(
        "Railbridge — any wired pad on a Switch 1 console\n"
        "\n"
        "Usage: railbridge [mode] [options]\n"
        "\n"
        "Modes\n"
        "  --auto              Host + gadget on this machine (Pi 4/5)\n"
        "  --host              Wake & read controllers (Switch 2/1, Xbox, DualShock/DualSense)\n"
        "  --gadget            Enumerate toward Switch 1\n"
        "  --sim [N]           Synthetic pads (default 4, max 8)\n"
        "  --list              Print supported USB devices and exit\n"
        "\n"
        "Output (Pi USB-C → Switch)\n"
        "  --out auto          1 pad → Pro Controller; 2+ → Pro hub (default)\n"
        "  --out pro           Single Pro Controller (057e:2009)\n"
        "  --out hub           Up to 8 Pro Controller HID functions on one cable\n"
        "  --out gc-adapter    Official GameCube Adapter ×1 or ×2 (057e:0337, 8 ports)\n"
        "\n"
        "Links\n"
        "  --relay HOST:PORT   Extra gadget appliance (repeatable). Pads split evenly.\n"
        "  --listen [IP:]PORT  This gadget appliance listens for --relay\n"
        "  --links N           Number of output links to plan for (default 1)\n"
        "\n"
        "Options\n"
        "  --no-invert-y       Do not invert Switch 2 stick Y\n"
        "\n"
        "Raspberry Pi 4/5 (one box):\n"
        "  Pads --USB→ Pi USB-A (hub OK)     host, up to 8\n"
        "  Pi USB-C --USB-C to USB-A→ dock   one cable, 8-pad hub or 2× GC adapter\n"
        "  sudo ./railbridge --auto --out hub\n"
        "  sudo ./railbridge --auto --out gc-adapter\n"
        "\n"
        "HD rumble: OUT on all 8 Pro HID when the UDC allows; peels OUT if endpoints run out.\n"
        "Two GC HID functions = 8 ports; some titles bind only the first adapter.\n"
        "\n"
        "Windows + Pi gadget(s):\n"
        "  Windows: railbridge --host --relay 192.168.1.50:7433\n"
        "  Pi:      sudo ./railbridge --gadget --listen 0.0.0.0:7433 --out gc-adapter\n"
    );
}

typedef struct {
    char host[128];
    int port;
} rb_relay_ep_t;

typedef struct {
    int auto_mode, host, gadget, sim, list;
    int invert_y;
    int sim_n;
    rb_out_mode_t out_mode;
    int links;
    rb_relay_ep_t relays[RB_MAX_LINKS];
    int nrelays;
    char listen_host[64];
    int listen_port;
} rb_opts_t;

static int parse_hostport(const char *s, char *host, int hostcap, int *port) {
    const char *colon = strrchr(s, ':');
    if (!colon) return -1;
    size_t n = (size_t)(colon - s);
    if (n >= (size_t)hostcap) n = hostcap - 1;
    memcpy(host, s, n);
    host[n] = 0;
    *port = atoi(colon + 1);
    return *port > 0 ? 0 : -1;
}

static int parse_args(int argc, char **argv, rb_opts_t *o) {
    memset(o, 0, sizeof(*o));
    o->invert_y = 1;
    o->auto_mode = 1;
    o->out_mode = RB_OUT_AUTO;
    o->links = 1;
    o->sim_n = 4;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage();
            exit(0);
        } else if (!strcmp(argv[i], "--auto")) {
            o->auto_mode = 1;
            o->host = o->gadget = 1;
        } else if (!strcmp(argv[i], "--host")) {
            o->host = 1;
            o->auto_mode = 0;
        } else if (!strcmp(argv[i], "--gadget")) {
            o->gadget = 1;
            o->auto_mode = 0;
        } else if (!strcmp(argv[i], "--sim")) {
            o->sim = 1;
            o->auto_mode = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                o->sim_n = atoi(argv[++i]);
                if (o->sim_n < 1) o->sim_n = 1;
                if (o->sim_n > RB_MAX_PADS) o->sim_n = RB_MAX_PADS;
            }
        } else if (!strcmp(argv[i], "--list")) {
            o->list = 1;
        } else if (!strcmp(argv[i], "--no-invert-y")) {
            o->invert_y = 0;
        } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
            const char *m = argv[++i];
            if (!strcmp(m, "pro")) o->out_mode = RB_OUT_PRO;
            else if (!strcmp(m, "hub")) o->out_mode = RB_OUT_PRO_HUB;
            else if (!strcmp(m, "gc") || !strcmp(m, "gc-adapter") || !strcmp(m, "adapter"))
                o->out_mode = RB_OUT_GC_ADAPTER;
            else o->out_mode = RB_OUT_AUTO;
        } else if (!strcmp(argv[i], "--links") && i + 1 < argc) {
            o->links = atoi(argv[++i]);
            if (o->links < 1) o->links = 1;
            if (o->links > RB_MAX_LINKS) o->links = RB_MAX_LINKS;
        } else if (!strcmp(argv[i], "--relay") && i + 1 < argc) {
            if (o->nrelays >= RB_MAX_LINKS) return -1;
            if (parse_hostport(argv[++i], o->relays[o->nrelays].host,
                               sizeof(o->relays[0].host), &o->relays[o->nrelays].port) != 0)
                return -1;
            o->nrelays++;
            o->host = 1;
        } else if (!strcmp(argv[i], "--listen") && i + 1 < argc) {
            if (parse_hostport(argv[++i], o->listen_host, sizeof(o->listen_host), &o->listen_port) != 0) {
                o->listen_port = atoi(argv[i]);
                snprintf(o->listen_host, sizeof(o->listen_host), "0.0.0.0");
            }
            o->gadget = 1;
        } else {
            fprintf(stderr, "Unknown arg %s\n", argv[i]);
            return -1;
        }
    }
    if (o->auto_mode) {
        o->host = 1;
        o->gadget = 1;
    }
    if (o->nrelays + (o->gadget ? 1 : 0) > o->links) o->links = o->nrelays + (o->gadget ? 1 : 0);
    if (o->links < 1) o->links = 1;
    return 0;
}

static void mac_for_port(const rb_config_t *base, int port, rb_config_t *out) {
    *out = *base;
    out->mac[5] = (uint8_t)(base->mac[5] + port);
    out->player_led = (uint8_t)(1u << (port & 7));
}

static int slot_for_port(const rb_assign_t *as, int nass, int port) {
    for (int i = 0; i < nass; i++) {
        if (as[i].link == 0 && as[i].port == port) return as[i].slot;
    }
    return port < RB_MAX_PADS ? port : -1;
}

#ifndef _WIN32
static void drain_eventfd(void) {
    if (g_pad_efd < 0) return;
    uint64_t x;
    while (read(g_pad_efd, &x, sizeof(x)) == (ssize_t)sizeof(x)) {
    }
}

/* Block until hidg output, a pad update, or keepalive is due. */
static int gadget_wait(int *hidg, int nhid, int timeout_ms) {
    struct pollfd p[RB_MAX_HIDG + 1];
    int n = 0;
    for (int i = 0; i < nhid; i++) {
        if (hidg[i] < 0) continue;
        p[n].fd = hidg[i];
        p[n].events = POLLIN;
        p[n].revents = 0;
        n++;
    }
    if (g_pad_efd >= 0) {
        p[n].fd = g_pad_efd;
        p[n].events = POLLIN;
        p[n].revents = 0;
        n++;
    }
    if (n == 0) {
        if (timeout_ms > 0) rb_sleep_ms(timeout_ms);
        return 0;
    }
    int rc = poll(p, (nfds_t)n, timeout_ms);
    drain_eventfd();
    return rc;
}

static int keepalive_timeout_ms(uint64_t last_ns, int streaming) {
    if (!streaming) return 50;
    uint64_t now = rb_now_ns();
    if (!last_ns || now - last_ns >= RB_KEEPALIVE_NS) return 0;
    uint64_t left = RB_KEEPALIVE_NS - (now - last_ns);
    int ms = (int)(left / 1000000ull);
    if (ms < 1) ms = 1;
    return ms;
}
#endif

static int gadget_loop_pro(int *hidg, int nhid, rb_sock_t relay) {
    rb_config_t cfgs[RB_MAX_HIDG];
    rb_pad_t last[RB_MAX_HIDG];
    uint64_t last_wr[RB_MAX_HIDG];
    memset(last, 0, sizeof(last));
    memset(last_wr, 0, sizeof(last_wr));
    for (int i = 0; i < nhid; i++) {
        mac_for_port(&g_cfg, i, &cfgs[i]);
        rb_s1_machine_init(&g_s1[i], &cfgs[i]);
        rb_pad_neutral(&last[i]);
        uint8_t hello[64];
        rb_s1_unsolicited_conn(&cfgs[i], hello);
        if (hidg[i] >= 0) rb_gadget_write(hidg[i], hello, 64);
    }
    uint8_t timer = 0;
    while (g_run) {
#ifndef _WIN32
        int streaming = 0;
        uint64_t oldest = 0;
        for (int i = 0; i < nhid; i++) {
            if (g_s1[i].state == RB_S1_STREAMING || g_s1[i].usb_only || g_s1[i].report_mode == 0x30)
                streaming = 1;
            if (!oldest || (last_wr[i] && last_wr[i] < oldest)) oldest = last_wr[i];
        }
        gadget_wait(hidg, nhid, keepalive_timeout_ms(oldest, streaming));
#endif
        rb_assign_t as[RB_MAX_PADS];
        int np = count_present();
        int nass = rb_plan_assigns(np > 0 ? np : nhid, 1, RB_OUT_PRO_HUB, as, RB_MAX_PADS);
        for (int i = 0; i < nhid; i++) {
            if (hidg[i] < 0) continue;
            uint8_t in[64];
            int n = rb_gadget_read(hidg[i], in, 64);
            while (n > 0) {
                uint8_t reply[64];
                int rn = rb_s1_handle_output(&g_s1[i], &cfgs[i], in, (size_t)n, reply, 64);
                if (rn > 0) rb_gadget_write(hidg[i], reply, 64);
                if (relay > 0) rb_net_send(relay, RB_NET_S1_OUT, in, 64);
                int slot = slot_for_port(as, nass, i);
                if (g_s1[i].rumble.active || g_s1[i].rumble.amp_l || g_s1[i].rumble.amp_r ||
                    in[0] == 0x10 || in[0] == 0x01) {
                    rumble_publish(slot, &g_s1[i].rumble);
                }
                n = rb_gadget_read(hidg[i], in, 64);
            }
            int live = g_s1[i].state == RB_S1_STREAMING || g_s1[i].usb_only || g_s1[i].report_mode == 0x30;
            if (!live) continue;
            rb_pad_t pad;
            uint64_t age = 0;
            int slot = slot_for_port(as, nass, i);
            if (!slot_read(slot, &pad, &age)) rb_pad_neutral(&pad);
            uint64_t now = rb_now_ns();
            int due = !last_wr[i] || now - last_wr[i] >= RB_KEEPALIVE_NS || pad_dirty(&pad, &last[i]);
            if (!due) continue;
            uint8_t out[64];
            rb_s1_encode_input(&pad, &cfgs[i], timer, out);
            if (rb_gadget_write(hidg[i], out, 64) == 0) {
                last[i] = pad;
                last_wr[i] = now;
            }
        }
        timer++;
#ifdef _WIN32
        rb_sleep_ms(1);
#endif
    }
    return 0;
}

static int gadget_loop_gc(int *hidg, int nhid, rb_sock_t relay) {
    int started[RB_MAX_GC_HID];
    rb_pad_t last[RB_MAX_PADS];
    uint64_t last_wr[RB_MAX_GC_HID];
    for (int i = 0; i < RB_MAX_GC_HID; i++) {
        started[i] = 1;
        last_wr[i] = 0;
    }
    for (int i = 0; i < RB_MAX_PADS; i++) rb_pad_neutral(&last[i]);
    if (nhid < 1) nhid = 1;
    if (nhid > RB_MAX_GC_HID) nhid = RB_MAX_GC_HID;

    while (g_run) {
#ifndef _WIN32
        gadget_wait(hidg, nhid, keepalive_timeout_ms(last_wr[0], 1));
#endif
        rb_assign_t as[RB_MAX_PADS];
        int np = count_present();
        int nass = rb_plan_assigns(np > 0 ? np : nhid * RB_GC_PORTS, 1, RB_OUT_GC_ADAPTER, as, RB_MAX_PADS);

        for (int h = 0; h < nhid; h++) {
            if (hidg[h] < 0) continue;
            uint8_t in[64];
            int n = rb_gadget_read(hidg[h], in, 64);
            while (n > 0) {
                if (in[0] == 0x13) started[h] = 1;
                if (in[0] == 0x11) {
                    rb_rumble_t ports[RB_GC_PORTS];
                    if (rb_rumble_parse_gc(in, (size_t)n, ports)) {
                        for (int p = 0; p < RB_GC_PORTS; p++) {
                            int slot = slot_for_port(as, nass, h * RB_GC_PORTS + p);
                            rumble_publish(slot, &ports[p]);
                        }
                    }
                }
                if (relay > 0) rb_net_send(relay, RB_NET_S1_OUT, in, (uint8_t)(n > 64 ? 64 : n));
                n = rb_gadget_read(hidg[h], in, 64);
            }
            if (!started[h]) continue;

            rb_pad_t pads[RB_GC_PORTS];
            int dirty = 0;
            for (int p = 0; p < RB_GC_PORTS; p++) {
                int slot = slot_for_port(as, nass, h * RB_GC_PORTS + p);
                uint64_t age = 0;
                rb_pad_t pad;
                if (slot < 0 || !slot_read(slot, &pad, &age)) {
                    rb_pad_neutral(&pad);
                    pad.valid = false;
                }
                pads[p] = pad;
                if (pad_dirty(&pad, &last[h * RB_GC_PORTS + p])) dirty = 1;
            }
            uint64_t now = rb_now_ns();
            int due = dirty || !last_wr[h] || now - last_wr[h] >= RB_KEEPALIVE_NS;
            if (!due) continue;
            uint8_t rpt[RB_GC_REPORT_LEN];
            rb_gc_encode_adapter(pads, rpt);
            if (rb_gadget_write(hidg[h], rpt, RB_GC_REPORT_LEN) == 0) {
                last_wr[h] = now;
                for (int p = 0; p < RB_GC_PORTS; p++) last[h * RB_GC_PORTS + p] = pads[p];
            }
        }
#ifdef _WIN32
        rb_sleep_ms(1);
#endif
    }
    return 0;
}

static int gadget_loop(int *hidg, int nhid, rb_out_mode_t mode, rb_sock_t relay) {
    if (mode == RB_OUT_GC_ADAPTER) return gadget_loop_gc(hidg, nhid, relay);
    return gadget_loop_pro(hidg, nhid, relay);
}

#ifndef RB_NO_USB

typedef struct {
    rb_host_t *host;
    uint8_t buf[64];
    struct libusb_transfer *xfer;
    int dead;
} rb_urb_t;

static void host_ingest(rb_host_t *h, const uint8_t *buf, int n);

static void LIBUSB_CALL on_host_xfer(struct libusb_transfer *t) {
    rb_urb_t *u = t->user_data;
    if (!u) return;
    if (t->status == LIBUSB_TRANSFER_COMPLETED && t->actual_length > 0 && u->host) {
        host_ingest(u->host, t->buffer, t->actual_length);
    }
    if (t->status == LIBUSB_TRANSFER_NO_DEVICE || t->status == LIBUSB_TRANSFER_ERROR ||
        t->status == LIBUSB_TRANSFER_STALL) {
        u->dead = 1;
        return;
    }
    if (g_run && t->dev_handle && !u->dead) {
        if (libusb_submit_transfer(t) != 0) u->dead = 1;
    }
}

static void host_ingest(rb_host_t *h, const uint8_t *buf, int n) {
    if (h->kind == RB_KIND_GC_ADAPTER) {
        rb_pad_t ports[RB_GC_PORTS];
        rb_gc_parse_adapter(buf, (size_t)n, ports);
        for (int p = 0; p < RB_GC_PORTS; p++) {
            rb_pad_t outp;
            if (ports[p].valid) {
                rb_translate(&ports[p], &g_cfg, &outp);
                slot_write(h->slot + p, &outp);
            } else {
                rb_pad_neutral(&outp);
                slot_write(h->slot + p, &outp);
                atomic_store(&g_slots[h->slot + p].present, 0);
            }
        }
        return;
    }
    rb_pad_t raw, outp;
    if (!rb_parse_report(buf, (size_t)n, h->kind, &raw)) return;
    raw.kind = h->kind;
    raw.vid = h->vid;
    raw.pid = h->pid;
    uint64_t t0 = rb_now_ns();
    rb_translate(&raw, &g_cfg, &outp);
    uint64_t dt = rb_now_ns() - t0;
    if (dt > 10000000ull) {
        fprintf(stderr, "translate %llu ns exceeds 10 ms budget\n", (unsigned long long)dt);
    }
    slot_write(h->slot, &outp);
}

static void host_apply_rumble(rb_host_t *h) {
    if (!h->h) return;
    if (h->kind == RB_KIND_GC_ADAPTER) {
        rb_rumble_t ports[RB_GC_PORTS];
        int any = 0;
        for (int p = 0; p < RB_GC_PORTS; p++) {
            unsigned seq = atomic_load_explicit(&g_rumble_seq[h->slot + p], memory_order_acquire);
            ports[p] = g_rumble[h->slot + p];
            if (seq != g_rumble_seen[h->slot + p]) {
                g_rumble_seen[h->slot + p] = seq;
                any = 1;
            }
        }
        if (!any) return;
        uint8_t rpt[8];
        int n = rb_rumble_encode_gc_ports(ports, rpt, 8);
        if (n > 0) rb_host_write(h, rpt, n, 8);
        return;
    }
    rb_rumble_t r;
    if (rumble_take(h->slot, &r)) rb_host_rumble_hd(h, &r);
}

static int slot_free_run(const uint8_t *occ, int need) {
    for (int s = 0; s <= RB_MAX_PADS - need; s++) {
        int ok = 1;
        for (int j = 0; j < need; j++) {
            if (occ[s + j]) {
                ok = 0;
                break;
            }
        }
        if (ok) return s;
    }
    return -1;
}

static int host_loop(libusb_context *ctx) {
    rb_host_t hosts[RB_MAX_PADS];
    rb_urb_t urbs[RB_MAX_PADS];
    uint8_t occ[RB_MAX_PADS];
    memset(hosts, 0, sizeof(hosts));
    memset(urbs, 0, sizeof(urbs));
    memset(occ, 0, sizeof(occ));
    int nh = 0;
    int warned = 0;
    uint64_t last_scan = 0;

    while (g_run) {
        uint64_t now = rb_now_ns();
        if (now - last_scan > 400000000ull || nh == 0) {
            last_scan = now;
            rb_host_t fresh[RB_MAX_PADS];
            int nf = rb_host_open_all(ctx, fresh, RB_MAX_PADS - nh, hosts, nh);
            for (int i = 0; i < nf && nh < RB_MAX_PADS; i++) {
                int need = fresh[i].gc_ports > 0 ? fresh[i].gc_ports : 1;
                int base = slot_free_run(occ, need);
                if (base < 0) {
                    rb_host_close(&fresh[i]);
                    continue;
                }
                if (rb_host_wakeup(&fresh[i]) != 0) {
                    fprintf(stderr, "Wake failed for %s\n", rb_kind_name(fresh[i].kind));
                    rb_host_close(&fresh[i]);
                    continue;
                }
                fresh[i].slot = base;
                hosts[nh] = fresh[i];
                for (int extra = 0; extra < need; extra++) occ[base + extra] = 1;
                urbs[nh].host = &hosts[nh];
                urbs[nh].dead = 0;
                urbs[nh].xfer = libusb_alloc_transfer(0);
                if (urbs[nh].xfer) {
                    libusb_fill_interrupt_transfer(urbs[nh].xfer, hosts[nh].h, hosts[nh].int_in,
                                                   urbs[nh].buf, 64, on_host_xfer, &urbs[nh], 0);
                    if (libusb_submit_transfer(urbs[nh].xfer) != 0) urbs[nh].dead = 1;
                }
                fprintf(stderr, "Opened %s (%04x:%04x) → slot %d%s\n",
                        rb_kind_name(fresh[i].kind), fresh[i].vid, fresh[i].pid, base,
                        need > 1 ? " (4 GC ports)" : "");
                nh++;
                warned = 0;
                atomic_store(&g_s2_state, RB_S2_STREAMING);
            }
            if (nh == 0 && !warned) {
                fprintf(stderr, "Waiting for pads (Switch 2/1, Xbox, DualShock/DualSense, GC adapter)…\n");
                warned = 1;
            }
        }

        if (nh == 0) {
            rb_sleep_ms(200);
            continue;
        }

        struct timeval tv = {0, 2000}; /* 2 ms cap; URB completion wakes sooner */
        libusb_handle_events_timeout_completed(ctx, &tv, NULL);

        for (int i = 0; i < nh; i++) {
            if (urbs[i].dead || !hosts[i].h) {
                fprintf(stderr, "Disconnected %s slot %d\n", rb_kind_name(hosts[i].kind), hosts[i].slot);
                int ports = hosts[i].gc_ports > 0 ? hosts[i].gc_ports : 1;
                for (int p = 0; p < ports; p++) {
                    rb_pad_t empty;
                    rb_pad_neutral(&empty);
                    slot_write(hosts[i].slot + p, &empty);
                    atomic_store(&g_slots[hosts[i].slot + p].present, 0);
                    if (hosts[i].slot + p < RB_MAX_PADS) occ[hosts[i].slot + p] = 0;
                }
                if (urbs[i].xfer) {
                    libusb_cancel_transfer(urbs[i].xfer);
                    struct timeval ctv = {0, 0};
                    libusb_handle_events_timeout_completed(ctx, &ctv, NULL);
                    libusb_free_transfer(urbs[i].xfer);
                    urbs[i].xfer = NULL;
                }
                rb_host_close(&hosts[i]);
                for (int j = i; j < nh - 1; j++) {
                    hosts[j] = hosts[j + 1];
                    urbs[j] = urbs[j + 1];
                    if (urbs[j].xfer) urbs[j].xfer->user_data = &urbs[j];
                    urbs[j].host = &hosts[j];
                }
                memset(&hosts[nh - 1], 0, sizeof(hosts[0]));
                memset(&urbs[nh - 1], 0, sizeof(urbs[0]));
                nh--;
                i--;
                continue;
            }
            host_apply_rumble(&hosts[i]);
        }
        atomic_store(&g_n_pads, count_present());
    }
    for (int i = 0; i < nh; i++) {
        if (urbs[i].xfer) {
            libusb_cancel_transfer(urbs[i].xfer);
            struct timeval ctv = {0, 10000};
            libusb_handle_events_timeout_completed(ctx, &ctv, NULL);
            libusb_free_transfer(urbs[i].xfer);
        }
        rb_host_close(&hosts[i]);
    }
    return 0;
}
#endif

static const rb_controller_kind_t k_sim_kinds[] = {
    RB_KIND_S2_PRO, RB_KIND_XBOXSERIES, RB_KIND_DUALSENSE, RB_KIND_S1_PRO,
    RB_KIND_S2_GC, RB_KIND_DS4, RB_KIND_XBOX360, RB_KIND_S1_JC_L
};

static int sim_loop(int n) {
    fprintf(stderr, "SIM: %d mixed pads, event-driven 8 ms keepalive\n", n);
    atomic_store(&g_s2_state, RB_S2_STREAMING);
    uint32_t t = 0;
    rb_pad_t last[RB_MAX_PADS];
    for (int i = 0; i < RB_MAX_PADS; i++) rb_pad_neutral(&last[i]);
    while (g_run) {
        int any = 0;
        for (int i = 0; i < n; i++) {
            rb_pad_t p;
            rb_pad_neutral(&p);
            p.kind = k_sim_kinds[i % 8];
            p.valid = true;
            p.lx = (uint16_t)(0x800 + (int)(300 * ((t / 40 + i * 3) % 20 < 10 ? 1 : -1)));
            if ((t / 80 + i) % 8 == 0) p.buttons[0] = 0x08;
            rb_pad_t outp;
            rb_translate(&p, &g_cfg, &outp);
            if (pad_dirty(&outp, &last[i])) {
                slot_write(i, &outp);
                last[i] = outp;
                any = 1;
            } else if ((t & 1) == 0) {
                slot_write(i, &outp);
            }
        }
        t++;
        (void)any;
        rb_sleep_ms(8);
    }
    return 0;
}

#ifndef _WIN32
static void *host_thread(void *arg) {
#ifndef RB_NO_USB
    host_loop((libusb_context *)arg);
#else
    (void)arg;
    fprintf(stderr, "USB host not built.\n");
#endif
    return NULL;
}

typedef struct {
    int hidg[RB_MAX_HIDG];
    int nhid;
    rb_out_mode_t mode;
    rb_sock_t relay;
} gadget_args_t;

static void *gadget_thread(void *arg) {
    gadget_args_t *a = arg;
    gadget_loop(a->hidg, a->nhid, a->mode, a->relay);
    return NULL;
}
#endif

int main(int argc, char **argv) {
    rb_opts_t opt;
    if (parse_args(argc, argv, &opt) != 0) {
        print_usage();
        return 2;
    }
    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    rb_config_default(&g_cfg);
    g_cfg.invert_y = opt.invert_y != 0;
    g_cfg.out_mode = opt.out_mode;
    g_cfg.links = opt.links;
    atomic_init(&g_s2_state, RB_S2_IDLE);
    atomic_init(&g_n_pads, 0);
    for (int i = 0; i < RB_MAX_PADS; i++) {
        atomic_init(&g_slots[i].seq, 0);
        atomic_init(&g_slots[i].present, 0);
        atomic_init(&g_rumble_seq[i], 0);
        rb_pad_neutral(&g_slots[i].pad);
        rb_rumble_clear(&g_rumble[i]);
    }
#ifndef _WIN32
    g_pad_efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
#endif

#ifndef RB_NO_USB
    libusb_context *ctx = NULL;
    if (opt.list || opt.host) {
        if (libusb_init(&ctx) != 0) {
            fprintf(stderr, "libusb_init failed\n");
            return 1;
        }
    }
    if (opt.list) {
        int n = rb_host_list(ctx);
        libusb_exit(ctx);
        return n >= 0 ? 0 : 1;
    }
#else
    void *ctx = NULL;
    if (opt.list) {
        fprintf(stderr, "Built without libusb.\n");
        return 1;
    }
    (void)ctx;
#endif

    int hidg[RB_MAX_HIDG];
    for (int i = 0; i < RB_MAX_HIDG; i++) hidg[i] = -1;
    int nhid = 0;
    rb_sock_t relay = (rb_sock_t)0;

    if (opt.gadget && opt.listen_port) {
        rb_sock_t ls = rb_net_listen(opt.listen_host[0] ? opt.listen_host : NULL, opt.listen_port);
        if (ls == (rb_sock_t)-1) {
            fprintf(stderr, "listen failed\n");
            return 1;
        }
        fprintf(stderr, "Waiting for host relay…\n");
        relay = rb_net_accept(ls);
    }

    int npads_hint = opt.sim ? opt.sim_n : RB_MAX_PADS;
    rb_out_mode_t outm = rb_out_resolve(opt.out_mode, npads_hint);
    int fn = rb_gadget_functions(outm, npads_hint);

    if (opt.gadget && !opt.listen_port) {
        if (rb_gadget_bind(outm, fn) != 0) {
            fprintf(stderr, "Continuing without local gadget.\n");
        } else {
            nhid = rb_gadget_count();
            if (nhid < 1) nhid = 1;
            for (int i = 0; i < nhid; i++) hidg[i] = rb_gadget_open_index(i);
        }
    }

    if (opt.nrelays) {
        relay = rb_net_connect(opt.relays[0].host, opt.relays[0].port);
        for (int i = 1; i < opt.nrelays; i++) {
            rb_sock_t extra = rb_net_connect(opt.relays[i].host, opt.relays[i].port);
            (void)extra;
            fprintf(stderr, "Additional link %d → %s:%d (even split)\n",
                    i + 1, opt.relays[i].host, opt.relays[i].port);
        }
    }

    fprintf(stderr, "Railbridge ready. out=%s functions=%d links=%d invert-Y=%s (event-driven)\n",
            rb_out_mode_name(outm), fn, opt.links, g_cfg.invert_y ? "on" : "off");

    if (opt.sim) {
#ifndef _WIN32
        pthread_t tg;
        gadget_args_t ga;
        memset(&ga, 0, sizeof(ga));
        memcpy(ga.hidg, hidg, sizeof(hidg));
        ga.nhid = nhid > 0 ? nhid : 1;
        ga.mode = outm;
        ga.relay = relay;
        if (opt.gadget) pthread_create(&tg, NULL, gadget_thread, &ga);
        sim_loop(opt.sim_n);
        g_run = 0;
        if (opt.gadget) pthread_join(tg, NULL);
#else
        sim_loop(opt.sim_n);
#endif
    } else {
#ifndef _WIN32
        pthread_t th, tg;
        gadget_args_t ga;
        memset(&ga, 0, sizeof(ga));
        memcpy(ga.hidg, hidg, sizeof(hidg));
        ga.nhid = nhid > 0 ? nhid : 1;
        ga.mode = outm;
        ga.relay = relay;
        if (opt.host) pthread_create(&th, NULL, host_thread, ctx);
        if (opt.gadget) pthread_create(&tg, NULL, gadget_thread, &ga);
        while (g_run) {
            /* Main thread just waits for signal; work is event-driven in workers. */
            rb_sleep_ms(200);
        }
        if (opt.host) pthread_join(th, NULL);
        if (opt.gadget) pthread_join(tg, NULL);
#else
        if (opt.host) {
#ifndef RB_NO_USB
            host_loop(ctx);
#endif
        }
#endif
    }

    for (int i = 0; i < RB_MAX_HIDG; i++) {
        if (hidg[i] >= 0) {
#ifndef _WIN32
            close(hidg[i]);
#endif
        }
    }
    rb_gadget_unbind();
#ifndef _WIN32
    if (g_pad_efd >= 0) close(g_pad_efd);
#endif
#ifndef RB_NO_USB
    if (ctx) libusb_exit(ctx);
#endif
    return 0;
}
