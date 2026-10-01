#include "railbridge.h"

#include <string.h>

rb_controller_kind_t rb_kind_from_pid(uint16_t pid) {
    return rb_kind_from_vid_pid(RB_VID_NINTENDO, pid);
}

rb_controller_kind_t rb_kind_from_vid_pid(uint16_t vid, uint16_t pid) {
    if (vid == RB_VID_NINTENDO) {
        switch (pid) {
            case RB_PID_S2_PRO: return RB_KIND_S2_PRO;
            case RB_PID_S2_GC_A:
            case RB_PID_S2_GC_B:
            case RB_PID_S2_GC_C: return RB_KIND_S2_GC;
            case RB_PID_JC2_L: return RB_KIND_JC2_L;
            case RB_PID_JC2_R: return RB_KIND_JC2_R;
            case RB_PID_S1_PRO: return RB_KIND_S1_PRO;
            case RB_PID_S1_JC_L: return RB_KIND_S1_JC_L;
            case RB_PID_S1_JC_R: return RB_KIND_S1_JC_R;
            case RB_PID_GC_ADAPTER: return RB_KIND_GC_ADAPTER;
            default: return RB_KIND_UNKNOWN;
        }
    }
    if (vid == RB_VID_MICROSOFT) {
        switch (pid) {
            case RB_PID_XBOX360:
            case RB_PID_XBOX360_W: return RB_KIND_XBOX360;
            case RB_PID_XBOX_SERIES:
            case RB_PID_XBOX_SERIES2: return RB_KIND_XBOXSERIES;
            case RB_PID_XBOXONE:
            case RB_PID_XBOXONE_S:
            case RB_PID_XBOXONE_S2:
            case RB_PID_XBOXONE_ELITE:
            case RB_PID_XBOXONE_ELITE2:
            case 0x02DD:
            case 0x02E0:
            case 0x02FF:
            case 0x0B05:
            case 0x0B20:
            case 0x0B22: return RB_KIND_XBOXONE;
            default:
                if ((pid & 0xFF00) == 0x0200 || (pid & 0xFF00) == 0x0B00)
                    return RB_KIND_XBOXONE;
                return RB_KIND_UNKNOWN;
        }
    }
    if (vid == RB_VID_SONY) {
        switch (pid) {
            case RB_PID_DS4:
            case RB_PID_DS4_V2:
            case RB_PID_DS4_DONGLE: return RB_KIND_DS4;
            case RB_PID_DUALSENSE:
            case RB_PID_DUALSENSE_EDGE: return RB_KIND_DUALSENSE;
            default: return RB_KIND_UNKNOWN;
        }
    }
    return RB_KIND_UNKNOWN;
}

rb_family_t rb_kind_family(rb_controller_kind_t kind) {
    switch (kind) {
        case RB_KIND_S2_PRO:
        case RB_KIND_S2_GC:
        case RB_KIND_JC2_L:
        case RB_KIND_JC2_R: return RB_FAMILY_NINTENDO_S2;
        case RB_KIND_S1_PRO:
        case RB_KIND_S1_JC_L:
        case RB_KIND_S1_JC_R: return RB_FAMILY_NINTENDO_S1;
        case RB_KIND_XBOX360:
        case RB_KIND_XBOXONE:
        case RB_KIND_XBOXSERIES: return RB_FAMILY_XBOX;
        case RB_KIND_DS4:
        case RB_KIND_DUALSENSE: return RB_FAMILY_SONY;
        case RB_KIND_GC_ADAPTER: return RB_FAMILY_GC_ADAPTER;
        default: return RB_FAMILY_UNKNOWN;
    }
}

const char *rb_kind_name(rb_controller_kind_t kind) {
    switch (kind) {
        case RB_KIND_S2_PRO: return "Switch 2 Pro Controller";
        case RB_KIND_S2_GC: return "NSO GameCube Controller";
        case RB_KIND_JC2_L: return "Joy-Con 2 (L)";
        case RB_KIND_JC2_R: return "Joy-Con 2 (R)";
        case RB_KIND_S1_PRO: return "Switch 1 Pro Controller";
        case RB_KIND_S1_JC_L: return "Joy-Con (L)";
        case RB_KIND_S1_JC_R: return "Joy-Con (R)";
        case RB_KIND_XBOX360: return "Xbox 360 Controller";
        case RB_KIND_XBOXONE: return "Xbox One Controller";
        case RB_KIND_XBOXSERIES: return "Xbox Series Controller";
        case RB_KIND_DS4: return "DualShock 4";
        case RB_KIND_DUALSENSE: return "DualSense";
        case RB_KIND_GC_ADAPTER: return "GameCube Adapter";
        default: return "Unknown controller";
    }
}

const char *rb_family_name(rb_family_t f) {
    switch (f) {
        case RB_FAMILY_NINTENDO_S2: return "Switch 2";
        case RB_FAMILY_NINTENDO_S1: return "Switch 1";
        case RB_FAMILY_XBOX: return "Xbox";
        case RB_FAMILY_SONY: return "PlayStation";
        case RB_FAMILY_GC_ADAPTER: return "GameCube Adapter";
        default: return "Unknown";
    }
}

const char *rb_out_mode_name(rb_out_mode_t m) {
    switch (m) {
        case RB_OUT_PRO: return "Pro Controller";
        case RB_OUT_PRO_HUB: return "Pro Controller hub";
        case RB_OUT_GC_ADAPTER: return "GameCube Adapter";
        default: return "auto";
    }
}

int rb_out_capacity(rb_out_mode_t m) {
    switch (m) {
        case RB_OUT_PRO: return 1;
        case RB_OUT_PRO_HUB: return RB_MAX_HIDG;
        case RB_OUT_GC_ADAPTER: return RB_GC_PORTS * RB_MAX_GC_HID;
        default: return RB_MAX_HIDG;
    }
}

int rb_gadget_functions(rb_out_mode_t m, int npads) {
    m = rb_out_resolve(m, npads);
    if (m == RB_OUT_PRO) return 1;
    if (m == RB_OUT_GC_ADAPTER) return npads > RB_GC_PORTS ? RB_MAX_GC_HID : 1;
    if (npads < 1) npads = 1;
    if (npads > RB_MAX_HIDG) npads = RB_MAX_HIDG;
    return npads;
}

rb_out_mode_t rb_out_resolve(rb_out_mode_t requested, int npads) {
    if (requested != RB_OUT_AUTO) return requested;
    if (npads <= 1) return RB_OUT_PRO;
    return RB_OUT_PRO_HUB;
}

uint16_t rb_axis_u8_to12(uint8_t v) {
    return (uint16_t)(((uint32_t)v * RB_STICK_MAX) / 255);
}

uint16_t rb_axis_s16_to12(int16_t v) {
    uint16_t u = (uint16_t)((int32_t)v + 32768);
    return (uint16_t)(u >> 4);
}

uint8_t rb_axis_12_to_u8(uint16_t v) {
    return (uint8_t)(((uint32_t)(v & 0xFFF) * 255) / RB_STICK_MAX);
}

static void face_xbox_to_s1(uint8_t a, uint8_t b, uint8_t x, uint8_t y, rb_pad_t *out) {
    /* Position map: Xbox A (south) → Nintendo B, B (east) → A, X (west) → Y, Y (north) → X. */
    if (a) out->buttons[0] |= 0x04;
    if (b) out->buttons[0] |= 0x08;
    if (x) out->buttons[0] |= 0x01;
    if (y) out->buttons[0] |= 0x02;
}

static void hat_to_dpad(uint8_t hat, rb_pad_t *out) {
    static const uint8_t map[8] = {0x02, 0x06, 0x04, 0x05, 0x01, 0x09, 0x08, 0x0A};
    if (hat < 8) out->buttons[2] |= map[hat];
}

/* Report 0x05 buttons already match Switch 1 0x30 button bytes 0-2. */
static void parse_buttons_05(const uint8_t b[4], rb_pad_t *out) {
    out->buttons[0] = b[0];
    out->buttons[1] = (uint8_t)(b[1] & 0x3F);
    out->buttons[2] = b[2];
    out->extra = b[3];
}

static void parse_buttons_09(const uint8_t b[3], rb_pad_t *out) {
    uint8_t s0 = 0, s1 = 0, s2 = 0;
    if (b[0] & 0x01) s0 |= 0x04;
    if (b[0] & 0x02) s0 |= 0x08;
    if (b[0] & 0x04) s0 |= 0x01;
    if (b[0] & 0x08) s0 |= 0x02;
    if (b[0] & 0x10) s0 |= 0x40;
    if (b[0] & 0x20) s0 |= 0x80;
    if (b[0] & 0x40) s1 |= 0x02;
    if (b[0] & 0x80) s1 |= 0x04;
    if (b[1] & 0x01) s2 |= 0x01;
    if (b[1] & 0x02) s2 |= 0x04;
    if (b[1] & 0x04) s2 |= 0x08;
    if (b[1] & 0x08) s2 |= 0x02;
    if (b[1] & 0x10) s2 |= 0x40;
    if (b[1] & 0x20) s2 |= 0x80;
    if (b[1] & 0x40) s1 |= 0x01;
    if (b[1] & 0x80) s1 |= 0x08;
    if (b[2] & 0x01) s1 |= 0x10;
    if (b[2] & 0x02) s1 |= 0x20;
    out->buttons[0] = s0;
    out->buttons[1] = s1;
    out->buttons[2] = s2;
    out->extra = b[2];
}

static void parse_buttons_0a(const uint8_t b[3], rb_pad_t *out) {
    uint8_t s0 = 0, s1 = 0, s2 = 0;
    if (b[0] & 0x01) s0 |= 0x04;
    if (b[0] & 0x02) s0 |= 0x08;
    if (b[0] & 0x04) s0 |= 0x01;
    if (b[0] & 0x08) s0 |= 0x02;
    if (b[0] & 0x10) s0 |= 0x80;
    if (b[0] & 0x20) s0 |= 0x40;
    if (b[0] & 0x40) s1 |= 0x02;
    if (b[0] & 0x80) s1 |= 0x04;
    if (b[1] & 0x01) s2 |= 0x01;
    if (b[1] & 0x02) s2 |= 0x04;
    if (b[1] & 0x04) s2 |= 0x08;
    if (b[1] & 0x08) s2 |= 0x02;
    if (b[1] & 0x10) s2 |= 0x80;
    if (b[1] & 0x20) s2 |= 0x40;
    if (b[1] & 0x40) s1 |= 0x01;
    if (b[1] & 0x80) s1 |= 0x08;
    if (b[2] & 0x01) s1 |= 0x10;
    if (b[2] & 0x02) s1 |= 0x20;
    out->buttons[0] = s0;
    out->buttons[1] = s1;
    out->buttons[2] = s2;
    out->extra = b[2];
}

bool rb_s2_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out) {
    if (!buf || !out || len < 2) return false;
    rb_pad_neutral(out);
    out->kind = kind;

    uint8_t id = buf[0];
    const uint8_t *p = buf + 1;
    size_t n = len - 1;

    if (id != 0x05 && id != 0x09 && id != 0x0A && id != 0x07 && id != 0x08) {
        if (len >= 16) {
            p = buf;
            n = len;
            id = 0x05;
        } else {
            return false;
        }
    }

    out->report_id = id;

    if (id == 0x05) {
        if (n < 16) return false;
        memcpy(&out->counter, p + 0, 4);
        parse_buttons_05(p + 4, out);
        rb_unpack12(p + 0x0A, &out->lx, &out->ly);
        rb_unpack12(p + 0x0D, &out->rx, &out->ry);
        if (n > 0x3D) {
            out->lt = p[0x3C];
            out->rt = p[0x3D];
        }
        if (n >= 0x2A + 18) {
            memcpy(&out->ax, p + 0x2A + 6, 2);
            memcpy(&out->ay, p + 0x2A + 8, 2);
            memcpy(&out->az, p + 0x2A + 10, 2);
            memcpy(&out->gx, p + 0x2A + 12, 2);
            memcpy(&out->gy, p + 0x2A + 14, 2);
            memcpy(&out->gz, p + 0x2A + 16, 2);
        }
        out->valid = true;
        return true;
    }

    if (id == 0x09) {
        if (n < 0x0B) return false;
        out->counter = p[0];
        parse_buttons_09(p + 2, out);
        rb_unpack12(p + 5, &out->lx, &out->ly);
        rb_unpack12(p + 8, &out->rx, &out->ry);
        out->valid = true;
        return true;
    }

    if (id == 0x0A) {
        if (n < 0x0E) return false;
        out->counter = p[0];
        parse_buttons_0a(p + 2, out);
        rb_unpack12(p + 5, &out->lx, &out->ly);
        rb_unpack12(p + 8, &out->rx, &out->ry);
        out->lt = p[0x0C];
        out->rt = p[0x0D];
        out->valid = true;
        return true;
    }

    return false;
}

bool rb_s1_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out) {
    if (!buf || !out || len < 2) return false;
    rb_pad_neutral(out);
    out->kind = kind;
    uint8_t id = buf[0];
    out->report_id = id;

    if (id == 0x30 && len >= 12) {
        out->counter = buf[1];
        out->buttons[0] = buf[3];
        out->buttons[1] = (uint8_t)(buf[4] & 0x3F);
        out->buttons[2] = buf[5];
        rb_unpack12(buf + 6, &out->lx, &out->ly);
        rb_unpack12(buf + 9, &out->rx, &out->ry);
        if (len >= 49) {
            memcpy(&out->ax, buf + 13, 2);
            memcpy(&out->ay, buf + 15, 2);
            memcpy(&out->az, buf + 17, 2);
            memcpy(&out->gx, buf + 19, 2);
            memcpy(&out->gy, buf + 21, 2);
            memcpy(&out->gz, buf + 23, 2);
        }
        out->valid = true;
        return true;
    }

    if (id == 0x3F && len >= 8) {
        uint8_t b0 = buf[1], b1 = buf[2], hat = buf[3];
        if (b0 & 0x01) out->buttons[0] |= 0x01; /* Y */
        if (b0 & 0x02) out->buttons[0] |= 0x02; /* X */
        if (b0 & 0x04) out->buttons[0] |= 0x04; /* B */
        if (b0 & 0x08) out->buttons[0] |= 0x08; /* A */
        if (b0 & 0x10) out->buttons[0] |= 0x40; /* R */
        if (b0 & 0x20) out->buttons[0] |= 0x80; /* ZR */
        if (b1 & 0x01) out->buttons[1] |= 0x01; /* Minus */
        if (b1 & 0x02) out->buttons[1] |= 0x02; /* Plus */
        if (b1 & 0x04) out->buttons[1] |= 0x08; /* L stick */
        if (b1 & 0x08) out->buttons[1] |= 0x04; /* R stick */
        if (b1 & 0x10) out->buttons[1] |= 0x10; /* Home */
        if (b1 & 0x20) out->buttons[1] |= 0x20; /* Capture */
        if (b1 & 0x40) out->buttons[2] |= 0x40; /* L */
        if (b1 & 0x80) out->buttons[2] |= 0x80; /* ZL */
        hat_to_dpad(hat, out);
        if (len >= 12) {
            uint16_t lx = (uint16_t)(buf[4] | (buf[5] << 8));
            uint16_t ly = (uint16_t)(buf[6] | (buf[7] << 8));
            uint16_t rx = (uint16_t)(buf[8] | (buf[9] << 8));
            uint16_t ry = (uint16_t)(buf[10] | (buf[11] << 8));
            out->lx = (uint16_t)((lx * RB_STICK_MAX) / 65535);
            out->ly = (uint16_t)((ly * RB_STICK_MAX) / 65535);
            out->rx = (uint16_t)((rx * RB_STICK_MAX) / 65535);
            out->ry = (uint16_t)((ry * RB_STICK_MAX) / 65535);
        } else {
            out->lx = rb_axis_u8_to12(buf[4]);
            out->ly = rb_axis_u8_to12(buf[5]);
            out->rx = rb_axis_u8_to12(buf[6]);
            out->ry = rb_axis_u8_to12(buf[7]);
        }
        out->valid = true;
        return true;
    }

    return false;
}

bool rb_xbox360_parse(const uint8_t *buf, size_t len, rb_pad_t *out) {
    if (!buf || !out || len < 14) return false;
    const uint8_t *d = buf;
    /* Canonical wired XInput: 00 14 [buttons lo] [buttons hi] LT RT LX LY RX RY */
    if (len >= 18 && buf[0] == 0x00 && buf[1] == 0x14) d = buf;
    else if (len >= 17 && buf[0] == 0x14) d = buf - 1;
    rb_pad_neutral(out);
    out->kind = RB_KIND_XBOX360;
    out->report_id = 0;
    uint8_t b0 = d[2];
    uint8_t b1 = d[3];
    if (b0 & 0x01) out->buttons[2] |= 0x02; /* up */
    if (b0 & 0x02) out->buttons[2] |= 0x01; /* down */
    if (b0 & 0x04) out->buttons[2] |= 0x08; /* left */
    if (b0 & 0x08) out->buttons[2] |= 0x04; /* right */
    if (b0 & 0x10) out->buttons[1] |= 0x02; /* start → plus */
    if (b0 & 0x20) out->buttons[1] |= 0x01; /* back → minus */
    if (b0 & 0x40) out->buttons[1] |= 0x08; /* LS */
    if (b0 & 0x80) out->buttons[1] |= 0x04; /* RS */
    if (b1 & 0x01) out->buttons[2] |= 0x40; /* LB → L */
    if (b1 & 0x02) out->buttons[0] |= 0x40; /* RB → R */
    if (b1 & 0x04) out->buttons[1] |= 0x10; /* guide → home */
    face_xbox_to_s1((uint8_t)(b1 & 0x10), (uint8_t)(b1 & 0x20),
                    (uint8_t)(b1 & 0x40), (uint8_t)(b1 & 0x80), out);
    out->lt = d[4];
    out->rt = d[5];
    int16_t lx, ly, rx, ry;
    memcpy(&lx, d + 6, 2);
    memcpy(&ly, d + 8, 2);
    memcpy(&rx, d + 10, 2);
    memcpy(&ry, d + 12, 2);
    out->lx = rb_axis_s16_to12(lx);
    out->ly = rb_axis_s16_to12((int16_t)(-ly));
    out->rx = rb_axis_s16_to12(rx);
    out->ry = rb_axis_s16_to12((int16_t)(-ry));
    if (out->lt > 30) out->buttons[2] |= 0x80;
    if (out->rt > 30) out->buttons[0] |= 0x80;
    out->valid = true;
    return true;
}

bool rb_xboxone_parse(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out) {
    if (!buf || !out || len < 18) return false;
    if (buf[0] != 0x20) return false; /* GIP input */
    rb_pad_neutral(out);
    out->kind = kind ? kind : RB_KIND_XBOXONE;
    out->report_id = 0x20;
    uint8_t b0 = buf[4];
    uint8_t b1 = buf[5];
    if (b0 & 0x04) out->buttons[1] |= 0x02; /* menu → plus */
    if (b0 & 0x08) out->buttons[1] |= 0x01; /* view → minus */
    face_xbox_to_s1((uint8_t)(b0 & 0x10), (uint8_t)(b0 & 0x20),
                    (uint8_t)(b0 & 0x40), (uint8_t)(b0 & 0x80), out);
    if (b1 & 0x01) out->buttons[2] |= 0x02; /* up */
    if (b1 & 0x02) out->buttons[2] |= 0x01; /* down */
    if (b1 & 0x04) out->buttons[2] |= 0x08; /* left */
    if (b1 & 0x08) out->buttons[2] |= 0x04; /* right */
    if (b1 & 0x10) out->buttons[2] |= 0x40; /* LB */
    if (b1 & 0x20) out->buttons[0] |= 0x40; /* RB */
    if (b1 & 0x40) out->buttons[1] |= 0x08; /* LS */
    if (b1 & 0x80) out->buttons[1] |= 0x04; /* RS */
    uint16_t lt, rt;
    int16_t lx, ly, rx, ry;
    memcpy(&lt, buf + 6, 2);
    memcpy(&rt, buf + 8, 2);
    memcpy(&lx, buf + 10, 2);
    memcpy(&ly, buf + 12, 2);
    memcpy(&rx, buf + 14, 2);
    memcpy(&ry, buf + 16, 2);
    out->lt = (uint8_t)(lt > 1020 ? 255 : (lt >> 2));
    out->rt = (uint8_t)(rt > 1020 ? 255 : (rt >> 2));
    out->lx = rb_axis_s16_to12(lx);
    out->ly = rb_axis_s16_to12((int16_t)(-ly));
    out->rx = rb_axis_s16_to12(rx);
    out->ry = rb_axis_s16_to12((int16_t)(-ry));
    if (out->lt > 30) out->buttons[2] |= 0x80;
    if (out->rt > 30) out->buttons[0] |= 0x80;
    if (len > 18 && (buf[18] & 0x01)) out->buttons[1] |= 0x20; /* share → capture */
    out->buttons[1] |= 0x00;
    /* Xbox guide is a separate GIP packet; ignore here. */
    out->valid = true;
    return true;
}

static bool parse_sony_common(const uint8_t *b, rb_pad_t *out, int analog_l2, int analog_r2) {
    uint8_t b0 = b[0], b1 = b[1], b2 = b[2];
    uint8_t hat = b0 & 0x0F;
    hat_to_dpad(hat, out);
    /* Position map: Cross (south) → B, Circle (east) → A, Square (west) → Y, Triangle (north) → X. */
    if (b0 & 0x10) out->buttons[0] |= 0x01; /* square → Y */
    if (b0 & 0x20) out->buttons[0] |= 0x04; /* cross → B */
    if (b0 & 0x40) out->buttons[0] |= 0x08; /* circle → A */
    if (b0 & 0x80) out->buttons[0] |= 0x02; /* triangle → X */
    if (b1 & 0x01) out->buttons[2] |= 0x40; /* L1 → L */
    if (b1 & 0x02) out->buttons[0] |= 0x40; /* R1 → R */
    if (b1 & 0x04) out->buttons[2] |= 0x80; /* L2 digital → ZL */
    if (b1 & 0x08) out->buttons[0] |= 0x80; /* R2 digital → ZR */
    if (b1 & 0x10) out->buttons[1] |= 0x01; /* create/share → minus */
    if (b1 & 0x20) out->buttons[1] |= 0x02; /* options → plus */
    if (b1 & 0x40) out->buttons[1] |= 0x08; /* L3 */
    if (b1 & 0x80) out->buttons[1] |= 0x04; /* R3 */
    if (b2 & 0x01) out->buttons[1] |= 0x10; /* PS → home */
    if (b2 & 0x02) out->buttons[1] |= 0x20; /* touchpad → capture */
    out->lt = (uint8_t)analog_l2;
    out->rt = (uint8_t)analog_r2;
    if (out->lt > 30) out->buttons[2] |= 0x80;
    if (out->rt > 30) out->buttons[0] |= 0x80;
    return true;
}

bool rb_ds4_parse(const uint8_t *buf, size_t len, rb_pad_t *out) {
    if (!buf || !out || len < 10) return false;
    int off = 0;
    if (buf[0] == 0x01) off = 1;
    else if (buf[0] == 0x11 && len >= 32) off = 3; /* BT */
    else if (buf[0] != 0x01 && len >= 9) off = 0;
    else return false;
    if ((size_t)off + 9 > len) return false;
    rb_pad_neutral(out);
    out->kind = RB_KIND_DS4;
    out->report_id = buf[0];
    out->lx = rb_axis_u8_to12(buf[off + 0]);
    out->ly = rb_axis_u8_to12(buf[off + 1]);
    out->rx = rb_axis_u8_to12(buf[off + 2]);
    out->ry = rb_axis_u8_to12(buf[off + 3]);
    parse_sony_common(buf + off + 4, out, buf[off + 7], buf[off + 8]);
    out->valid = true;
    return true;
}

bool rb_dualsense_parse(const uint8_t *buf, size_t len, rb_pad_t *out) {
    if (!buf || !out || len < 12) return false;
    int off = 0;
    if (buf[0] == 0x01) off = 1;
    else if (buf[0] == 0x31 && len >= 32) off = 2; /* BT */
    else if (len >= 11) off = 0;
    else return false;
    if ((size_t)off + 10 > len) return false;
    rb_pad_neutral(out);
    out->kind = RB_KIND_DUALSENSE;
    out->report_id = buf[0];
    out->lx = rb_axis_u8_to12(buf[off + 0]);
    out->ly = rb_axis_u8_to12(buf[off + 1]);
    out->rx = rb_axis_u8_to12(buf[off + 2]);
    out->ry = rb_axis_u8_to12(buf[off + 3]);
    uint8_t l2 = buf[off + 4];
    uint8_t r2 = buf[off + 5];
    parse_sony_common(buf + off + 7, out, l2, r2);
    out->valid = true;
    return true;
}

static void gc_port_to_pad(const uint8_t *p, rb_pad_t *out) {
    rb_pad_neutral(out);
    out->kind = RB_KIND_S2_GC;
    if (!(p[0] & 0x10) && p[0] != 0x14 && p[0] != 0x22) {
        out->valid = false;
        return;
    }
    uint8_t b1 = p[1], b2 = p[2];
    if (b1 & 0x01) out->buttons[0] |= 0x08; /* A */
    if (b1 & 0x02) out->buttons[0] |= 0x04; /* B */
    if (b1 & 0x04) out->buttons[0] |= 0x02; /* X */
    if (b1 & 0x08) out->buttons[0] |= 0x01; /* Y */
    if (b1 & 0x10) out->buttons[2] |= 0x08; /* left */
    if (b1 & 0x20) out->buttons[2] |= 0x04; /* right */
    if (b1 & 0x40) out->buttons[2] |= 0x01; /* down */
    if (b1 & 0x80) out->buttons[2] |= 0x02; /* up */
    if (b2 & 0x01) out->buttons[1] |= 0x02; /* start → plus */
    if (b2 & 0x02) out->buttons[0] |= 0x80; /* Z → ZR */
    if (b2 & 0x04) out->buttons[0] |= 0x40; /* R */
    if (b2 & 0x08) out->buttons[2] |= 0x40; /* L */
    out->lx = rb_axis_u8_to12(p[3]);
    out->ly = rb_axis_u8_to12(p[4]);
    out->rx = rb_axis_u8_to12(p[5]);
    out->ry = rb_axis_u8_to12(p[6]);
    out->lt = p[7];
    out->rt = p[8];
    if (out->lt > 40) out->buttons[2] |= 0x80;
    if (out->rt > 40) out->buttons[0] |= 0x80;
    out->valid = true;
}

int rb_gc_parse_adapter(const uint8_t *buf, size_t len, rb_pad_t out[RB_GC_PORTS]) {
    if (!buf || !out || len < RB_GC_REPORT_LEN) return 0;
    const uint8_t *p = buf;
    if (buf[0] == 0x21) p = buf;
    else return 0;
    int n = 0;
    for (int i = 0; i < RB_GC_PORTS; i++) {
        gc_port_to_pad(p + 1 + i * 9, &out[i]);
        out[i].slot = (uint8_t)i;
        if (out[i].valid) n++;
    }
    return n;
}

void rb_gc_encode_port(const rb_pad_t *pad, uint8_t out9[9]) {
    memset(out9, 0, 9);
    if (!pad || !pad->valid) return;
    out9[0] = 0x14;
    uint8_t b1 = 0, b2 = 0;
    if (pad->buttons[0] & 0x08) b1 |= 0x01; /* A */
    if (pad->buttons[0] & 0x04) b1 |= 0x02; /* B */
    if (pad->buttons[0] & 0x02) b1 |= 0x04; /* X */
    if (pad->buttons[0] & 0x01) b1 |= 0x08; /* Y */
    if (pad->buttons[2] & 0x08) b1 |= 0x10;
    if (pad->buttons[2] & 0x04) b1 |= 0x20;
    if (pad->buttons[2] & 0x01) b1 |= 0x40;
    if (pad->buttons[2] & 0x02) b1 |= 0x80;
    if (pad->buttons[1] & 0x02) b2 |= 0x01; /* start */
    if (pad->buttons[0] & 0x80) b2 |= 0x02; /* Z from ZR */
    if ((pad->buttons[0] & 0x40) || pad->rt > 180) b2 |= 0x04;
    if ((pad->buttons[2] & 0x40) || pad->lt > 180) b2 |= 0x08;
    out9[1] = b1;
    out9[2] = b2;
    out9[3] = rb_axis_12_to_u8(pad->lx);
    out9[4] = rb_axis_12_to_u8(pad->ly);
    out9[5] = rb_axis_12_to_u8(pad->rx);
    out9[6] = rb_axis_12_to_u8(pad->ry);
    out9[7] = pad->lt;
    out9[8] = pad->rt;
}

void rb_gc_encode_adapter(const rb_pad_t pads[RB_GC_PORTS], uint8_t out37[RB_GC_REPORT_LEN]) {
    memset(out37, 0, RB_GC_REPORT_LEN);
    out37[0] = 0x21;
    for (int i = 0; i < RB_GC_PORTS; i++) {
        rb_gc_encode_port(&pads[i], out37 + 1 + i * 9);
    }
}

bool rb_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out) {
    switch (rb_kind_family(kind)) {
        case RB_FAMILY_NINTENDO_S2:
            return rb_s2_parse_report(buf, len, kind, out);
        case RB_FAMILY_NINTENDO_S1:
            return rb_s1_parse_report(buf, len, kind, out);
        case RB_FAMILY_XBOX:
            if (kind == RB_KIND_XBOX360) return rb_xbox360_parse(buf, len, out);
            return rb_xboxone_parse(buf, len, kind, out);
        case RB_FAMILY_SONY:
            if (kind == RB_KIND_DUALSENSE) return rb_dualsense_parse(buf, len, out);
            return rb_ds4_parse(buf, len, out);
        default:
            if (kind == RB_KIND_GC_ADAPTER) return false;
            /* Try families in order for unknown. */
            if (rb_s2_parse_report(buf, len, kind, out)) return true;
            if (rb_s1_parse_report(buf, len, kind, out)) return true;
            if (rb_xboxone_parse(buf, len, kind, out)) return true;
            if (rb_ds4_parse(buf, len, out)) return true;
            return rb_xbox360_parse(buf, len, out);
    }
}

uint8_t rb_s2_preferred_report_id(rb_controller_kind_t kind) {
    switch (kind) {
        case RB_KIND_S2_PRO: return 0x09;
        case RB_KIND_S2_GC: return 0x0A;
        case RB_KIND_JC2_L: return 0x07;
        case RB_KIND_JC2_R: return 0x08;
        default: return 0x05;
    }
}

void rb_s2_wakeup_commands(rb_controller_kind_t kind, rb_s2_cmd_t *out, int *count) {
    static const uint8_t seq[][32] = {
        {16, 0x03, 0x91, 0x00, 0x0D, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        {8,  0x07, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
        {8,  0x16, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00},
        {12, 0x03, 0x91, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00},
        {12, 0x0C, 0x91, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00},
        {0}
    };
    int n = 0;
    for (int i = 0; seq[i][0] && n < 16; i++) {
        out[n].cmd = seq[i][1];
        out[n].sub = seq[i][4];
        out[n].payload_len = seq[i][0];
        memcpy(out[n].payload, &seq[i][1], seq[i][0]);
        n++;
    }
    uint8_t report = rb_s2_preferred_report_id(kind);
    out[n].cmd = 0x03;
    out[n].sub = 0x0A;
    out[n].payload_len = 12;
    uint8_t sel[12] = {0x03, 0x91, 0x00, 0x0A, 0x00, 0x04, 0x00, 0x00, report, 0x00, 0x00, 0x00};
    memcpy(out[n].payload, sel, 12);
    n++;
    *count = n;
}

int rb_xbox_wakeup_commands(rb_controller_kind_t kind, uint8_t *out, int cap) {
    if (kind == RB_KIND_XBOX360) return 0;
    /* GIP power-on (Xbox One / Series). */
    static const uint8_t pwr[] = {0x05, 0x20, 0x00, 0x01, 0x00};
    static const uint8_t sinit[] = {0x05, 0x20, 0x00, 0x0f, 0x06};
    if (cap < (int)(sizeof(pwr) + sizeof(sinit) + 2)) return 0;
    int n = 0;
    out[n++] = (uint8_t)sizeof(pwr);
    memcpy(out + n, pwr, sizeof(pwr));
    n += (int)sizeof(pwr);
    out[n++] = (uint8_t)sizeof(sinit);
    memcpy(out + n, sinit, sizeof(sinit));
    n += (int)sizeof(sinit);
    out[n++] = 0;
    return n;
}

int rb_s1_host_wakeup_commands(uint8_t cmds[][64], int *lens, int cap) {
    if (cap < 5) return 0;
    memset(cmds[0], 0, 64); cmds[0][0] = 0x80; cmds[0][1] = 0x01; lens[0] = 2;
    memset(cmds[1], 0, 64); cmds[1][0] = 0x80; cmds[1][1] = 0x02; lens[1] = 2;
    memset(cmds[2], 0, 64); cmds[2][0] = 0x80; cmds[2][1] = 0x03; lens[2] = 2;
    memset(cmds[3], 0, 64); cmds[3][0] = 0x80; cmds[3][1] = 0x04; lens[3] = 2;
    memset(cmds[4], 0, 64); cmds[4][0] = 0x01; cmds[4][10] = 0x03; cmds[4][11] = 0x30; lens[4] = 12;
    return 5;
}
