#include "railbridge.h"

#include <string.h>

static const uint8_t k_idle[4] = {0x00, 0x01, 0x40, 0x40};

bool rb_kind_hd_rumble(rb_controller_kind_t kind) {
    switch (kind) {
        case RB_KIND_S2_PRO:
        case RB_KIND_JC2_L:
        case RB_KIND_JC2_R:
        case RB_KIND_S1_PRO:
        case RB_KIND_S1_JC_L:
        case RB_KIND_S1_JC_R:
        case RB_KIND_S2_GC:
            return true;
        default:
            return false;
    }
}

void rb_rumble_clear(rb_rumble_t *r) {
    memset(r, 0, sizeof(*r));
    memcpy(r->raw, k_idle, 4);
    memcpy(r->raw + 4, k_idle, 4);
}

static int hd_idle(const uint8_t b[4]) {
    if (!b[0] && !b[1] && !b[2] && !b[3]) return 1;
    return b[0] == 0x00 && b[1] == 0x01 && b[2] == 0x40 && b[3] == 0x40;
}

/* Nintendo HD rumble 4-byte block → 0–255 amplitude.
 * High band amp in byte 1 (0x80 flag + encoded), low band in byte 3 (0x40 idle). */
static uint8_t hd_amp(const uint8_t b[4]) {
    if (hd_idle(b)) return 0;
    uint8_t hf = 0, lf = 0;
    if (b[1] > 1) {
        if (b[1] & 0x80)
            hf = (uint8_t)(((uint16_t)(b[1] & 0x7F) * 255) / 100);
        else
            hf = (uint8_t)(((uint16_t)b[1] * 255) / 192);
    }
    if (b[3] > 0x40)
        lf = (uint8_t)(((uint16_t)(b[3] - 0x40) * 255) / 112);
    else if (b[3] && b[3] < 0x40)
        lf = (uint8_t)(((uint16_t)b[3] * 255) / 64);
    return hf > lf ? hf : lf;
}

void rb_rumble_encode_hd(uint8_t dst4[4], uint8_t amp) {
    if (amp == 0) {
        memcpy(dst4, k_idle, 4);
        return;
    }
    /* ~320 Hz high band, ~160 Hz low band — a solid default tick. */
    dst4[0] = 0x74;
    dst4[1] = (uint8_t)(0x80 | (amp / 2));
    if (dst4[1] < 0x81) dst4[1] = 0x81;
    dst4[2] = 0x60;
    dst4[3] = (uint8_t)(0x40 + (amp / 4));
    if (dst4[3] < 0x41) dst4[3] = 0x41;
}

bool rb_rumble_parse_s1(const uint8_t *report, size_t len, rb_rumble_t *out) {
    if (!report || !out || len < 10) return false;
    uint8_t id = report[0];
    if (id != 0x10 && id != 0x01) return false;
    rb_rumble_clear(out);
    memcpy(out->raw, report + 2, 8);
    out->hf_l = report[2];
    out->lf_l = report[4];
    out->hf_r = report[6];
    out->lf_r = report[8];
    out->amp_l = hd_amp(report + 2);
    out->amp_r = hd_amp(report + 6);
    out->active = out->amp_l > 0 || out->amp_r > 0;
    out->hd = out->active;
    return true;
}

bool rb_rumble_parse_gc(const uint8_t *report, size_t len, rb_rumble_t out[RB_GC_PORTS]) {
    if (!report || !out || len < 2) return false;
    if (report[0] != 0x11) return false;
    for (int i = 0; i < RB_GC_PORTS; i++) {
        rb_rumble_clear(&out[i]);
        uint8_t on = (i + 1 < (int)len) ? report[i + 1] : 0;
        if (on) {
            out[i].amp_l = 255;
            out[i].amp_r = 255;
            out[i].active = true;
            rb_rumble_encode_hd(out[i].raw, 255);
            rb_rumble_encode_hd(out[i].raw + 4, 255);
        }
        out[i].hd = false;
    }
    return true;
}

int rb_rumble_encode_gc_ports(const rb_rumble_t ports[RB_GC_PORTS], uint8_t *out, int cap) {
    if (!ports || !out || cap < 5) return 0;
    out[0] = 0x11;
    for (int i = 0; i < RB_GC_PORTS; i++) {
        out[i + 1] = (ports[i].active || ports[i].amp_l > 20 || ports[i].amp_r > 20) ? 1 : 0;
    }
    return 5;
}

int rb_rumble_encode_for(rb_controller_kind_t kind, const rb_rumble_t *r, uint8_t *out, int cap) {
    if (!r || !out || cap < 5) return 0;
    memset(out, 0, (size_t)cap);
    uint8_t l = r->amp_l, rr = r->amp_r;

    if (rb_kind_hd_rumble(kind) && rb_kind_family(kind) == RB_FAMILY_NINTENDO_S1) {
        if (cap < 10) return 0;
        out[0] = 0x10;
        out[1] = 0;
        memcpy(out + 2, r->raw, 8);
        return 10;
    }

    if (kind == RB_KIND_S2_PRO || kind == RB_KIND_JC2_L || kind == RB_KIND_JC2_R) {
        if (cap < 16) return 0;
        out[0] = 0x02;
        memcpy(out + 1, r->raw, 8);
        return 16;
    }

    if (kind == RB_KIND_S2_GC) {
        if (cap < 16) return 0;
        out[0] = 0x03;
        memcpy(out + 1, r->raw, 8);
        return 16;
    }

    if (kind == RB_KIND_XBOX360) {
        if (cap < 8) return 0;
        out[0] = 0x00;
        out[1] = 0x08;
        out[3] = l;
        out[4] = rr;
        return 8;
    }

    if (kind == RB_KIND_XBOXONE || kind == RB_KIND_XBOXSERIES) {
        if (cap < 13) return 0;
        out[0] = 0x09;
        out[1] = 0x00;
        out[2] = 0x00;
        out[3] = 0x09;
        out[4] = 0x00;
        out[5] = 0x0F;
        out[6] = l;
        out[7] = l;
        out[8] = rr;
        out[9] = rr;
        out[10] = 0xFF;
        out[11] = 0x00;
        out[12] = 0x00;
        return 13;
    }

    if (kind == RB_KIND_DS4) {
        if (cap < 32) return 0;
        out[0] = 0x05;
        out[1] = 0xFF;
        out[4] = rr;
        out[5] = l;
        return 32;
    }

    if (kind == RB_KIND_DUALSENSE) {
        if (cap < 48) return 0;
        out[0] = 0x02;
        out[1] = 0x01 | 0x02; /* valid flag: rumble */
        out[3] = rr;
        out[4] = l;
        return 48;
    }

    if (kind == RB_KIND_GC_ADAPTER) {
        rb_rumble_t ports[RB_GC_PORTS];
        for (int i = 0; i < RB_GC_PORTS; i++) rb_rumble_clear(&ports[i]);
        ports[0] = *r;
        ports[1] = *r;
        ports[1].amp_l = r->amp_r;
        ports[1].amp_r = r->amp_r;
        return rb_rumble_encode_gc_ports(ports, out, cap);
    }

    return 0;
}
