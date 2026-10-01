#include "railbridge.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static uint8_t g_spi[0x10000];
static int g_spi_ready = 0;

const char *rb_s1_state_name(rb_s1_state_t s) {
    switch (s) {
        case RB_S1_IDLE: return "idle";
        case RB_S1_ATTACHED: return "attached";
        case RB_S1_CONN_STATUS: return "conn-status";
        case RB_S1_HANDSHAKE: return "handshake";
        case RB_S1_BAUD: return "baud";
        case RB_S1_USB_ONLY: return "usb-only";
        case RB_S1_SUBCMD: return "subcommands";
        case RB_S1_STREAMING: return "streaming";
        default: return "error";
    }
}

const char *rb_s2_state_name(rb_s2_state_t s) {
    switch (s) {
        case RB_S2_IDLE: return "idle";
        case RB_S2_ENUMERATED: return "enumerated";
        case RB_S2_WAKE_USB: return "wake-usb";
        case RB_S2_ENABLE_HID: return "enable-hid";
        case RB_S2_SELECT_REPORT: return "select-report";
        case RB_S2_STREAMING: return "streaming";
        default: return "error";
    }
}

void rb_config_default(rb_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->mac[0] = 0x98;
    cfg->mac[1] = 0xB6;
    cfg->mac[2] = 0xE9;
    cfg->mac[3] = 0x12;
    cfg->mac[4] = 0x34;
    cfg->mac[5] = 0x56;
    cfg->player_led = 0x01;
    cfg->invert_y = true;
    cfg->gc_analog_shoulders = true;
    cfg->gc_trigger_threshold = 40;
    cfg->out_mode = RB_OUT_AUTO;
    cfg->links = 1;
    cfg->gc_adapters = 2;
}

void rb_pad_neutral(rb_pad_t *pad) {
    memset(pad, 0, sizeof(*pad));
    pad->lx = RB_STICK_CENTER;
    pad->ly = RB_STICK_CENTER;
    pad->rx = RB_STICK_CENTER;
    pad->ry = RB_STICK_CENTER;
}

void rb_pack12(uint8_t out[3], uint16_t x, uint16_t y) {
    x &= 0xFFF;
    y &= 0xFFF;
    out[0] = (uint8_t)(x & 0xFF);
    out[1] = (uint8_t)(((x >> 8) & 0x0F) | ((y & 0x0F) << 4));
    out[2] = (uint8_t)((y >> 4) & 0xFF);
}

void rb_unpack12(const uint8_t in[3], uint16_t *x, uint16_t *y) {
    *x = (uint16_t)(in[0] | ((in[1] & 0x0F) << 8));
    *y = (uint16_t)((in[1] >> 4) | (in[2] << 4));
}

static uint16_t clamp12(int v) {
    if (v < 0) return 0;
    if (v > 0xFFF) return 0xFFF;
    return (uint16_t)v;
}

static uint16_t maybe_invert_y(uint16_t y, bool invert) {
    return invert ? (uint16_t)(0xFFF - y) : y;
}

void rb_translate(const rb_pad_t *in, const rb_config_t *cfg, rb_pad_t *out) {
    *out = *in;
    rb_family_t fam = rb_kind_family(in->kind);
    bool inv = cfg->invert_y && fam == RB_FAMILY_NINTENDO_S2;
    out->ly = maybe_invert_y(in->ly, inv);
    out->ry = maybe_invert_y(in->ry, inv);
    if ((in->kind == RB_KIND_S2_GC || in->kind == RB_KIND_GC_ADAPTER) && cfg->gc_analog_shoulders) {
        if (in->lt > cfg->gc_trigger_threshold) out->buttons[2] |= 0x80;
        if (in->rt > cfg->gc_trigger_threshold) out->buttons[0] |= 0x80;
        if (in->lt > 180) out->buttons[2] |= 0x40;
        if (in->rt > 180) out->buttons[0] |= 0x40;
    }
    out->lx = clamp12(out->lx);
    out->ly = clamp12(out->ly);
    out->rx = clamp12(out->rx);
    out->ry = clamp12(out->ry);
}

void rb_s1_encode_input(const rb_pad_t *pad, const rb_config_t *cfg, uint8_t timer, uint8_t *out64) {
    memset(out64, 0, RB_REPORT_LEN);
    out64[0] = 0x30;
    out64[1] = timer;
    out64[2] = 0x91;
    out64[3] = pad->buttons[0];
    out64[4] = (uint8_t)(pad->buttons[1] | 0x80);
    out64[5] = pad->buttons[2];
    rb_pack12(out64 + 6, pad->lx, pad->ly);
    rb_pack12(out64 + 9, pad->rx, pad->ry);
    out64[12] = 0x00;
    for (int s = 0; s < 3; s++) {
        uint8_t *d = out64 + 13 + s * 12;
        memcpy(d + 0, &pad->ax, 2);
        memcpy(d + 2, &pad->ay, 2);
        memcpy(d + 4, &pad->az, 2);
        memcpy(d + 6, &pad->gx, 2);
        memcpy(d + 8, &pad->gy, 2);
        memcpy(d + 10, &pad->gz, 2);
    }
    (void)cfg;
}

static void pack_cal_pair(uint8_t *d, uint16_t x, uint16_t y) {
    rb_pack12(d, x, y);
}

void rb_fill_factory_spi(uint8_t *spi, size_t len) {
    memset(spi, 0xFF, len);
    uint16_t c = RB_STICK_CENTER;
    uint16_t mx = 0xE20, mn = 0x1E0;
    uint8_t *p = spi + 0x603D;
    pack_cal_pair(p + 0, mx, mx);
    pack_cal_pair(p + 3, c, c);
    pack_cal_pair(p + 6, mn, mn);
    pack_cal_pair(p + 9, c, c);
    pack_cal_pair(p + 12, mn, mn);
    pack_cal_pair(p + 15, mx, mx);
    memset(spi + 0x6020, 0x00, 24);
    spi[0x6050] = 0x32;
    spi[0x6051] = 0x32;
    spi[0x6052] = 0x32;
    spi[0x6053] = 0xFF;
    spi[0x6054] = 0x1E;
    spi[0x6055] = 0x1E;
    spi[0x6056] = 0x1E;
    spi[0x6057] = 0xFF;
    memcpy(spi + 0x6000, "XKW00000000001", 14);
}

int rb_spi_read(uint32_t addr, uint8_t size, uint8_t *dst) {
    if (!g_spi_ready) {
        rb_fill_factory_spi(g_spi, sizeof(g_spi));
        g_spi_ready = 1;
    }
    if (size > 0x1D) size = 0x1D;
    if (addr >= sizeof(g_spi)) {
        memset(dst, 0xFF, size);
        return size;
    }
    uint32_t n = size;
    if (addr + n > sizeof(g_spi)) n = (uint32_t)(sizeof(g_spi) - addr);
    memcpy(dst, g_spi + addr, n);
    if (n < size) memset(dst + n, 0xFF, size - n);
    return size;
}

void rb_s1_unsolicited_conn(const rb_config_t *cfg, uint8_t *out64) {
    memset(out64, 0, RB_REPORT_LEN);
    out64[0] = 0x81;
    out64[1] = 0x01;
    out64[2] = 0x00;
    out64[3] = 0x03;
    memcpy(out64 + 4, cfg->mac, 6);
}

static void fill_input_prefix(uint8_t *r, uint8_t report_id, uint8_t timer) {
    memset(r, 0, RB_REPORT_LEN);
    r[0] = report_id;
    r[1] = timer;
    r[2] = 0x91;
    r[3] = 0x00;
    r[4] = 0x80;
    r[5] = 0x00;
    rb_pack12(r + 6, RB_STICK_CENTER, RB_STICK_CENTER);
    rb_pack12(r + 9, RB_STICK_CENTER, RB_STICK_CENTER);
}

static int reply_subcmd(rb_s1_machine_t *m, const rb_config_t *cfg, uint8_t sub,
                        const uint8_t *arg, size_t argn, uint8_t *reply) {
    fill_input_prefix(reply, 0x21, m->timer++);
    reply[13] = (uint8_t)(0x80 | sub);
    reply[14] = sub;

    switch (sub) {
        case 0x02: {
            reply[15] = 0x03;
            reply[16] = 0x48;
            reply[17] = 0x03;
            reply[18] = 0x02;
            memcpy(reply + 19, cfg->mac, 6);
            reply[25] = 0x01;
            reply[26] = 0x01;
            m->state = RB_S1_SUBCMD;
            return RB_REPORT_LEN;
        }
        case 0x10: {
            uint32_t addr = 0;
            uint8_t sz = 0;
            if (argn >= 5) {
                addr = (uint32_t)arg[0] | ((uint32_t)arg[1] << 8) |
                       ((uint32_t)arg[2] << 16) | ((uint32_t)arg[3] << 24);
                sz = arg[4];
            }
            reply[13] = 0x90;
            reply[14] = 0x10;
            reply[15] = (uint8_t)(addr & 0xFF);
            reply[16] = (uint8_t)((addr >> 8) & 0xFF);
            reply[17] = (uint8_t)((addr >> 16) & 0xFF);
            reply[18] = (uint8_t)((addr >> 24) & 0xFF);
            reply[19] = sz;
            rb_spi_read(addr, sz, reply + 20);
            return RB_REPORT_LEN;
        }
        case 0x03:
            if (argn >= 1) m->report_mode = arg[0];
            if (m->report_mode == 0x30) m->state = RB_S1_STREAMING;
            return RB_REPORT_LEN;
        case 0x30:
            if (argn >= 1) m->player_led = arg[0];
            return RB_REPORT_LEN;
        case 0x40:
            m->imu_enabled = argn && arg[0];
            return RB_REPORT_LEN;
        case 0x48:
            m->rumble_enabled = argn && arg[0];
            return RB_REPORT_LEN;
        default:
            return RB_REPORT_LEN;
    }
}

void rb_s1_machine_init(rb_s1_machine_t *m, const rb_config_t *cfg) {
    memset(m, 0, sizeof(*m));
    m->state = RB_S1_ATTACHED;
    m->report_mode = 0x3F;
    m->player_led = cfg->player_led;
    rb_rumble_clear(&m->rumble);
}

int rb_s1_handle_output(rb_s1_machine_t *m, const rb_config_t *cfg, const uint8_t *in, size_t len,
                        uint8_t *reply, size_t reply_cap) {
    if (!in || len < 1 || reply_cap < RB_REPORT_LEN) return 0;
    memcpy(m->last_cmd, in, len < RB_REPORT_LEN ? len : RB_REPORT_LEN);

    uint8_t id = in[0];

    if (id == 0x80) {
        uint8_t cmd = len > 1 ? in[1] : 0;
        memset(reply, 0, RB_REPORT_LEN);
        reply[0] = 0x81;
        reply[1] = cmd;
        switch (cmd) {
            case 0x01:
                reply[2] = 0x00;
                reply[3] = 0x03;
                memcpy(reply + 4, cfg->mac, 6);
                m->state = RB_S1_CONN_STATUS;
                memcpy(m->last_reply, reply, RB_REPORT_LEN);
                return RB_REPORT_LEN;
            case 0x02:
                m->state = RB_S1_HANDSHAKE;
                memcpy(m->last_reply, reply, RB_REPORT_LEN);
                return RB_REPORT_LEN;
            case 0x03:
                m->state = RB_S1_BAUD;
                memcpy(m->last_reply, reply, RB_REPORT_LEN);
                return RB_REPORT_LEN;
            case 0x04:
                m->usb_only = true;
                m->state = RB_S1_USB_ONLY;
                return 0;
            case 0x05:
                m->usb_only = false;
                return 0;
            default:
                memcpy(m->last_reply, reply, RB_REPORT_LEN);
                return RB_REPORT_LEN;
        }
    }

    if (id == 0x01 && len >= 11) {
        rb_rumble_parse_s1(in, len, &m->rumble);
        m->rumble_counter = in[1];
        uint8_t sub = in[10];
        const uint8_t *arg = in + 11;
        size_t argn = len > 11 ? len - 11 : 0;
        int n = reply_subcmd(m, cfg, sub, arg, argn, reply);
        memcpy(m->last_reply, reply, RB_REPORT_LEN);
        return n;
    }

    if (id == 0x10) {
        rb_rumble_parse_s1(in, len, &m->rumble);
        if (len > 1) m->rumble_counter = in[1];
        return 0;
    }

    return 0;
}

int rb_split_even(int npads, int nlinks, int cap_per, int *counts) {
    if (nlinks < 1) nlinks = 1;
    if (nlinks > RB_MAX_LINKS) nlinks = RB_MAX_LINKS;
    if (cap_per < 1) cap_per = 1;
    if (npads < 0) npads = 0;
    int total_cap = nlinks * cap_per;
    int n = npads > total_cap ? total_cap : npads;
    int base = n / nlinks;
    int rem = n % nlinks;
    for (int i = 0; i < nlinks; i++) {
        counts[i] = base + (i < rem ? 1 : 0);
        if (counts[i] > cap_per) counts[i] = cap_per;
    }
    return n;
}

int rb_plan_assigns(int npads, int nlinks, rb_out_mode_t mode, rb_assign_t *out, int cap) {
    mode = rb_out_resolve(mode, npads);
    int per = rb_out_capacity(mode);
    if (nlinks < 1) nlinks = 1;
    if (nlinks > RB_MAX_LINKS) nlinks = RB_MAX_LINKS;
    int counts[RB_MAX_LINKS];
    int placed = rb_split_even(npads, nlinks, per, counts);
    int slot = 0, n = 0;
    for (int link = 0; link < nlinks && n < cap; link++) {
        for (int port = 0; port < counts[link] && n < cap; port++) {
            out[n].slot = slot++;
            out[n].link = link;
            out[n].port = port;
            n++;
        }
    }
    (void)placed;
    return n;
}

uint64_t rb_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
