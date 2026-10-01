#include "internal.h"

#ifndef RB_NO_USB

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#define rb_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define rb_sleep_ms(ms) usleep((ms) * 1000)
#endif

static int already_open(const rb_host_t *already, int n, uint8_t bus, uint8_t addr) {
    for (int i = 0; i < n; i++) {
        if (already[i].h && already[i].bus == bus && already[i].addr == addr) return 1;
    }
    return 0;
}

static int find_endpoints(libusb_device *dev, rb_host_t *h) {
    struct libusb_config_descriptor *cfg = NULL;
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0) return -1;
    h->iface_hid = -1;
    h->iface_vendor = -1;
    h->bulk_out = 0;
    h->bulk_in = 0;
    h->int_in = 0;
    h->int_out = 0;
    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        const struct libusb_interface *itf = &cfg->interface[i];
        if (itf->num_altsetting < 1) continue;
        const struct libusb_interface_descriptor *d = &itf->altsetting[0];
        int is_hid = d->bInterfaceClass == 3;
        int is_vendor = d->bInterfaceClass == 0xFF;
        for (int e = 0; e < d->bNumEndpoints; e++) {
            const struct libusb_endpoint_descriptor *ep = &d->endpoint[e];
            uint8_t type = ep->bmAttributes & 0x03;
            int inn = (ep->bEndpointAddress & 0x80) != 0;
            if (type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                if (is_hid && h->iface_hid < 0) h->iface_hid = d->bInterfaceNumber;
                if (inn && !h->int_in) h->int_in = ep->bEndpointAddress;
                if (!inn && !h->int_out) h->int_out = ep->bEndpointAddress;
            }
            if (type == LIBUSB_TRANSFER_TYPE_BULK) {
                if (is_vendor && h->iface_vendor < 0) h->iface_vendor = d->bInterfaceNumber;
                if (inn && !h->bulk_in) h->bulk_in = ep->bEndpointAddress;
                else if (!inn && !h->bulk_out) h->bulk_out = ep->bEndpointAddress;
            }
            if (is_vendor && h->iface_vendor < 0) h->iface_vendor = d->bInterfaceNumber;
            if (is_hid && h->iface_hid < 0) h->iface_hid = d->bInterfaceNumber;
        }
    }
    libusb_free_config_descriptor(cfg);
    if (!h->int_in) h->int_in = 0x81;
    if (!h->int_out) h->int_out = 0x01;
    if (!h->bulk_out) h->bulk_out = 0x02;
    if (!h->bulk_in) h->bulk_in = 0x82;
    if (h->iface_hid < 0) h->iface_hid = 0;
    if (h->iface_vendor < 0) h->iface_vendor = (h->kind == RB_KIND_S2_PRO || h->kind == RB_KIND_S2_GC ||
                                                 h->kind == RB_KIND_JC2_L || h->kind == RB_KIND_JC2_R)
                                                    ? 1
                                                    : 0;
    return 0;
}

static int claim(libusb_device_handle *h, int iface) {
    if (iface < 0) return 0;
    libusb_detach_kernel_driver(h, iface);
    return libusb_claim_interface(h, iface);
}

static int open_one(libusb_device *dev, uint16_t vid, uint16_t pid, rb_host_t *out) {
    memset(out, 0, sizeof(*out));
    out->vid = vid;
    out->pid = pid;
    out->kind = rb_kind_from_vid_pid(vid, pid);
    out->bus = libusb_get_bus_number(dev);
    out->addr = libusb_get_device_address(dev);
    out->gc_ports = (out->kind == RB_KIND_GC_ADAPTER) ? RB_GC_PORTS : 1;
    find_endpoints(dev, out);
    int rc = libusb_open(dev, &out->h);
    if (rc != 0) {
        fprintf(stderr, "open %04x:%04x failed: %s\n", vid, pid, libusb_strerror(rc));
        return -1;
    }
    claim(out->h, out->iface_hid);
    if (out->iface_vendor != out->iface_hid) claim(out->h, out->iface_vendor);
    return 0;
}

int rb_host_open_all(libusb_context *ctx, rb_host_t *out, int cap, const rb_host_t *already, int nalready) {
    libusb_device **list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    if (n < 0) return -1;
    int got = 0;
    for (ssize_t i = 0; i < n && got < cap; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
        rb_controller_kind_t k = rb_kind_from_vid_pid(desc.idVendor, desc.idProduct);
        if (k == RB_KIND_UNKNOWN) continue;
        uint8_t bus = libusb_get_bus_number(list[i]);
        uint8_t addr = libusb_get_device_address(list[i]);
        if (already_open(already, nalready, bus, addr)) continue;
        if (open_one(list[i], desc.idVendor, desc.idProduct, &out[got]) == 0) {
            got++;
        }
    }
    libusb_free_device_list(list, 1);
    return got;
}

void rb_host_close(rb_host_t *h) {
    if (!h || !h->h) return;
    if (h->iface_hid >= 0) libusb_release_interface(h->h, h->iface_hid);
    if (h->iface_vendor >= 0 && h->iface_vendor != h->iface_hid)
        libusb_release_interface(h->h, h->iface_vendor);
    libusb_close(h->h);
    h->h = NULL;
}

static int bulk_cmd(rb_host_t *h, const uint8_t *cmd, int len) {
    int xfer = 0;
    int rc = libusb_bulk_transfer(h->h, h->bulk_out, (unsigned char *)cmd, len, &xfer, 1000);
    if (rc != 0 && rc != LIBUSB_ERROR_TIMEOUT) {
        fprintf(stderr, "bulk out: %s\n", libusb_strerror(rc));
        return -1;
    }
    uint8_t ack[64];
    int rx = 0;
    libusb_bulk_transfer(h->h, h->bulk_in, ack, 64, &rx, 120);
    return 0;
}

int rb_host_write(rb_host_t *h, const uint8_t *buf, int len, int timeout_ms) {
    if (!h->h || !h->int_out) return -1;
    int xfer = 0;
    int rc = libusb_interrupt_transfer(h->h, h->int_out, (unsigned char *)buf, len, &xfer, timeout_ms);
    if (rc == LIBUSB_ERROR_TIMEOUT) return 0;
    return rc == 0 ? xfer : -1;
}

int rb_host_wakeup(rb_host_t *h) {
    rb_family_t fam = rb_kind_family(h->kind);
    if (fam == RB_FAMILY_NINTENDO_S2) {
        rb_s2_cmd_t cmds[16];
        int n = 0;
        rb_s2_wakeup_commands(h->kind, cmds, &n);
        for (int i = 0; i < n; i++) {
            if (bulk_cmd(h, cmds[i].payload, cmds[i].payload_len) != 0) return -1;
            rb_sleep_ms(40);
        }
        return 0;
    }
    if (fam == RB_FAMILY_NINTENDO_S1) {
        uint8_t cmds[8][64];
        int lens[8];
        int n = rb_s1_host_wakeup_commands(cmds, lens, 8);
        for (int i = 0; i < n; i++) {
            rb_host_write(h, cmds[i], lens[i], 200);
            uint8_t ack[64];
            rb_host_read_report(h, ack, 200);
            rb_sleep_ms(20);
        }
        return 0;
    }
    if (fam == RB_FAMILY_XBOX && h->kind != RB_KIND_XBOX360) {
        uint8_t blob[64];
        int n = rb_xbox_wakeup_commands(h->kind, blob, 64);
        int i = 0;
        while (i < n && blob[i]) {
            int len = blob[i++];
            rb_host_write(h, blob + i, len, 200);
            i += len;
            rb_sleep_ms(20);
        }
        return 0;
    }
    if (h->kind == RB_KIND_GC_ADAPTER) {
        uint8_t start = 0x13;
        rb_host_write(h, &start, 1, 200);
        return 0;
    }
    return 0;
}

int rb_host_read_report(rb_host_t *h, uint8_t *buf, int timeout_ms) {
    int xfer = 0;
    int rc = libusb_interrupt_transfer(h->h, h->int_in, buf, 64, &xfer, timeout_ms);
    if (rc == LIBUSB_ERROR_TIMEOUT) return 0;
    if (rc != 0) return -1;
    return xfer;
}

int rb_host_rumble_hd(rb_host_t *h, const rb_rumble_t *r) {
    uint8_t rpt[64];
    int n = rb_rumble_encode_for(h->kind, r, rpt, 64);
    if (n <= 0 || !h->int_out) return 0;
    int xfer = 0;
    return libusb_interrupt_transfer(h->h, h->int_out, rpt, n, &xfer, 8);
}

int rb_host_rumble(rb_host_t *h, uint8_t l, uint8_t r) {
    rb_rumble_t rum;
    rb_rumble_clear(&rum);
    rb_rumble_encode_hd(rum.raw, l);
    rb_rumble_encode_hd(rum.raw + 4, r);
    rum.amp_l = l;
    rum.amp_r = r;
    rum.active = l || r;
    rum.hd = rb_kind_hd_rumble(h->kind) && rum.active;
    return rb_host_rumble_hd(h, &rum);
}

int rb_host_list(libusb_context *ctx) {
    libusb_device **list = NULL;
    ssize_t n = libusb_get_device_list(ctx, &list);
    int found = 0;
    for (ssize_t i = 0; i < n; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
        rb_controller_kind_t k = rb_kind_from_vid_pid(desc.idVendor, desc.idProduct);
        if (k == RB_KIND_UNKNOWN) continue;
        printf("%04x:%04x  %s  bus %u addr %u\n",
               desc.idVendor, desc.idProduct, rb_kind_name(k),
               libusb_get_bus_number(list[i]), libusb_get_device_address(list[i]));
        found++;
    }
    libusb_free_device_list(list, 1);
    if (!found) puts("No supported controllers on USB.");
    return found;
}

#else

int rb_host_list(void *ctx) {
    (void)ctx;
    fprintf(stderr, "USB host support was not built (libusb missing).\n");
    return -1;
}

#endif
