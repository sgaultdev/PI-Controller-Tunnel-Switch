#include "railbridge.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, msg); g_fail++; } \
} while (0)

static void test_pack_roundtrip(void) {
    uint8_t b[3];
    uint16_t x, y;
    rb_pack12(b, 0x800, 0x123);
    rb_unpack12(b, &x, &y);
    EXPECT(x == 0x800, "center X");
    EXPECT(y == 0x123, "Y");
    rb_pack12(b, 0xFFF, 0x000);
    rb_unpack12(b, &x, &y);
    EXPECT(x == 0xFFF && y == 0x000, "extrema");
}

static void test_s2_05_buttons(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x05;
    rpt[1 + 4] = 0x08;
    rpt[1 + 4 + 1] = 0x02;
    rpt[1 + 4 + 2] = 0x02;
    uint8_t ls[3], rs[3];
    rb_pack12(ls, 0x900, 0x700);
    rb_pack12(rs, 0x800, 0x800);
    memcpy(rpt + 1 + 0x0A, ls, 3);
    memcpy(rpt + 1 + 0x0D, rs, 3);

    rb_pad_t pad;
    EXPECT(rb_s2_parse_report(rpt, 64, RB_KIND_S2_PRO, &pad), "parse 05");
    EXPECT(pad.buttons[0] == 0x08, "A bit");
    EXPECT(pad.buttons[1] == 0x02, "Plus");
    EXPECT(pad.buttons[2] == 0x02, "Up");
    EXPECT(pad.lx == 0x900, "lx");
    EXPECT(pad.ly == 0x700, "ly");
}

static void test_s2_09_remap(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x09;
    rpt[1 + 2] = 0x02;
    rb_pad_t pad;
    EXPECT(rb_s2_parse_report(rpt, 64, RB_KIND_S2_PRO, &pad), "parse 09");
    EXPECT(pad.buttons[0] == 0x08, "A remapped to S1");
}

static void test_gc_z_to_zr(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x0A;
    rpt[1 + 2] = 0x10;
    rpt[1 + 0x0C] = 200;
    rpt[1 + 0x0D] = 10;
    rb_pad_t pad;
    EXPECT(rb_s2_parse_report(rpt, 64, RB_KIND_S2_GC, &pad), "parse 0A");
    EXPECT(pad.buttons[0] & 0x80, "Z→ZR");
    rb_config_t cfg;
    rb_config_default(&cfg);
    rb_pad_t out;
    rb_translate(&pad, &cfg, &out);
    EXPECT(out.buttons[2] & 0x80, "L analog → ZL");
    EXPECT(out.buttons[2] & 0x40, "L analog high → L");
}

static void test_s1_handshake(void) {
    rb_config_t cfg;
    rb_config_default(&cfg);
    rb_s1_machine_t m;
    rb_s1_machine_init(&m, &cfg);
    uint8_t cmd[64] = {0}, reply[64];

    cmd[0] = 0x80; cmd[1] = 0x01;
    int n = rb_s1_handle_output(&m, &cfg, cmd, 2, reply, 64);
    EXPECT(n == 64, "conn reply");
    EXPECT(reply[0] == 0x81 && reply[1] == 0x01 && reply[3] == 0x03, "pro type");

    cmd[1] = 0x02;
    n = rb_s1_handle_output(&m, &cfg, cmd, 2, reply, 64);
    EXPECT(n == 64 && reply[0] == 0x81 && reply[1] == 0x02, "handshake");

    cmd[1] = 0x04;
    n = rb_s1_handle_output(&m, &cfg, cmd, 2, reply, 64);
    EXPECT(n == 0 && m.usb_only, "usb-only no reply");

    memset(cmd, 0, 64);
    cmd[0] = 0x01;
    cmd[10] = 0x02;
    n = rb_s1_handle_output(&m, &cfg, cmd, 16, reply, 64);
    EXPECT(n == 64 && reply[0] == 0x21 && reply[14] == 0x02 && reply[17] == 0x03, "device info");

    memset(cmd, 0, 64);
    cmd[0] = 0x01;
    cmd[10] = 0x03;
    cmd[11] = 0x30;
    n = rb_s1_handle_output(&m, &cfg, cmd, 16, reply, 64);
    EXPECT(m.state == RB_S1_STREAMING, "streaming after 0x30 mode");
}

static void test_s1_input_passthrough(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x30;
    rpt[3] = 0x08; /* A */
    rpt[5] = 0x02; /* Up */
    uint8_t ls[3];
    rb_pack12(ls, 0x810, 0x7F0);
    memcpy(rpt + 6, ls, 3);
    rb_pack12(ls, 0x800, 0x800);
    memcpy(rpt + 9, ls, 3);
    rb_pad_t pad;
    EXPECT(rb_s1_parse_report(rpt, 64, RB_KIND_S1_PRO, &pad), "parse S1 0x30");
    EXPECT(pad.buttons[0] == 0x08, "A");
    EXPECT(pad.buttons[2] == 0x02, "Up");
    EXPECT(pad.lx == 0x810, "lx");
    rb_config_t cfg;
    rb_config_default(&cfg);
    rb_pad_t out;
    rb_translate(&pad, &cfg, &out);
    EXPECT(out.ly == pad.ly, "S1 Y not inverted");
}

static void test_xbox360(void) {
    uint8_t rpt[20];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x00;
    rpt[1] = 0x14;
    rpt[3] = 0x10; /* A */
    rpt[2] = 0x01; /* dpad up */
    rpt[4] = 200;  /* LT */
    int16_t c = 0;
    memcpy(rpt + 6, &c, 2);
    memcpy(rpt + 8, &c, 2);
    memcpy(rpt + 10, &c, 2);
    memcpy(rpt + 12, &c, 2);
    rb_pad_t pad;
    EXPECT(rb_xbox360_parse(rpt, 20, &pad), "xbox360 parse");
    EXPECT(pad.buttons[0] & 0x04, "Xbox A → Nintendo B");
    EXPECT(!(pad.buttons[0] & 0x08), "Xbox A is not Nintendo A");
    EXPECT(pad.buttons[2] & 0x02, "dpad up");
    EXPECT(pad.buttons[2] & 0x80, "LT → ZL");
    EXPECT(pad.lx == RB_STICK_CENTER, "lx center");
}

static void test_xboxone(void) {
    uint8_t rpt[32];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x20;
    rpt[3] = 0x0E;
    rpt[4] = 0x20; /* Xbox B */
    rpt[5] = 0x10; /* LB */
    uint16_t lt = 1023;
    memcpy(rpt + 6, &lt, 2);
    int16_t z = 0;
    memcpy(rpt + 10, &z, 2);
    memcpy(rpt + 12, &z, 2);
    memcpy(rpt + 14, &z, 2);
    memcpy(rpt + 16, &z, 2);
    rb_pad_t pad;
    EXPECT(rb_xboxone_parse(rpt, 32, RB_KIND_XBOXSERIES, &pad), "gip parse");
    EXPECT(pad.buttons[0] & 0x08, "Xbox B → Nintendo A");
    EXPECT(pad.buttons[2] & 0x40, "LB → L");
}

static void test_ds4(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x01;
    rpt[1] = 128;
    rpt[2] = 128;
    rpt[3] = 128;
    rpt[4] = 128;
    rpt[5] = 0x40; /* circle, hat center 8 would be better */
    rpt[5] = 0x48; /* hat=8 (none) | circle */
    rpt[6] = 0x20; /* options */
    rpt[7] = 0x00;
    rpt[8] = 0;
    rpt[9] = 0;
    rb_pad_t pad;
    EXPECT(rb_ds4_parse(rpt, 64, &pad), "ds4 parse");
    EXPECT(pad.buttons[0] & 0x08, "Circle → Nintendo A");
    EXPECT(pad.buttons[1] & 0x02, "Options → Plus");
    EXPECT(pad.lx == rb_axis_u8_to12(128), "lx");
}

static void test_dualsense(void) {
    uint8_t rpt[64];
    memset(rpt, 0, sizeof(rpt));
    rpt[0] = 0x01;
    rpt[1] = 128;
    rpt[2] = 128;
    rpt[3] = 128;
    rpt[4] = 128;
    rpt[5] = 0;
    rpt[6] = 0;
    rpt[8] = 0x28; /* hat none(8) | cross 0x20 */
    rpt[9] = 0x01; /* L1 */
    rpt[10] = 0x01; /* PS */
    rb_pad_t pad;
    EXPECT(rb_dualsense_parse(rpt, 64, &pad), "ds parse");
    EXPECT(pad.buttons[0] & 0x04, "Cross → Nintendo B");
    EXPECT(pad.buttons[2] & 0x40, "L1 → L");
    EXPECT(pad.buttons[1] & 0x10, "PS → Home");
}

static void test_gc_adapter_roundtrip(void) {
    rb_pad_t pads[4];
    for (int i = 0; i < 4; i++) {
        rb_pad_neutral(&pads[i]);
        pads[i].valid = (i < 2);
        pads[i].kind = RB_KIND_S2_GC;
        if (i == 0) {
            pads[i].buttons[0] = 0x08; /* A */
            pads[i].lx = 0xC00;
            pads[i].lt = 180;
        }
        if (i == 1) {
            pads[i].buttons[0] = 0x80; /* ZR → Z */
            pads[i].rx = 0x200;
        }
    }
    uint8_t rpt[37];
    rb_gc_encode_adapter(pads, rpt);
    EXPECT(rpt[0] == 0x21, "report id");
    EXPECT(rpt[1] == 0x14, "port0 connected");
    EXPECT(rpt[1 + 9] == 0x14, "port1 connected");
    EXPECT(rpt[1 + 18] == 0x00, "port2 empty");
    EXPECT(rpt[2] & 0x01, "A on port0");
    EXPECT(rpt[1 + 9 + 2] & 0x02, "Z on port1");

    rb_pad_t back[4];
    int n = rb_gc_parse_adapter(rpt, 37, back);
    EXPECT(n == 2, "two live ports");
    EXPECT(back[0].buttons[0] & 0x08, "A survived");
    EXPECT(back[1].buttons[0] & 0x80, "Z→ZR survived");
}

static void test_ids(void) {
    EXPECT(rb_kind_from_vid_pid(0x057E, 0x2069) == RB_KIND_S2_PRO, "s2 pro");
    EXPECT(rb_kind_from_vid_pid(0x057E, 0x2009) == RB_KIND_S1_PRO, "s1 pro");
    EXPECT(rb_kind_from_vid_pid(0x057E, 0x0337) == RB_KIND_GC_ADAPTER, "gc adapter");
    EXPECT(rb_kind_from_vid_pid(0x045E, 0x0B12) == RB_KIND_XBOXSERIES, "series");
    EXPECT(rb_kind_from_vid_pid(0x045E, 0x028E) == RB_KIND_XBOX360, "360");
    EXPECT(rb_kind_from_vid_pid(0x054C, 0x0CE6) == RB_KIND_DUALSENSE, "dualsense");
    EXPECT(rb_kind_from_vid_pid(0x054C, 0x09CC) == RB_KIND_DS4, "ds4");
    EXPECT(rb_kind_family(RB_KIND_XBOXONE) == RB_FAMILY_XBOX, "family xbox");
    EXPECT(rb_kind_family(RB_KIND_S1_PRO) == RB_FAMILY_NINTENDO_S1, "family s1");
}

static void test_split_even(void) {
    int c[4];
    int n = rb_split_even(5, 2, 4, c);
    EXPECT(n == 5, "placed 5");
    EXPECT(c[0] == 3 && c[1] == 2, "3+2 even split");
    n = rb_split_even(1, 1, 1, c);
    EXPECT(n == 1 && c[0] == 1, "single");
    n = rb_split_even(8, 2, 4, c);
    EXPECT(n == 8 && c[0] == 4 && c[1] == 4, "two adapters");
    n = rb_split_even(3, 1, 4, c);
    EXPECT(n == 3 && c[0] == 3, "one cable three pads");

    n = rb_split_even(8, 1, 8, c);
    EXPECT(n == 8 && c[0] == 8, "8 on one hub");

    rb_assign_t a[8];
    int na = rb_plan_assigns(5, 2, RB_OUT_GC_ADAPTER, a, 8);
    EXPECT(na == 5, "5 assigns");
    EXPECT(a[0].link == 0 && a[0].port == 0, "first");
    EXPECT(a[3].link == 1 && a[3].port == 0, "overflow to link 1");
    EXPECT(rb_out_resolve(RB_OUT_AUTO, 1) == RB_OUT_PRO, "auto 1 → pro");
    EXPECT(rb_out_resolve(RB_OUT_AUTO, 3) == RB_OUT_PRO_HUB, "auto 3 → hub");
    EXPECT(rb_out_capacity(RB_OUT_GC_ADAPTER) == 8, "gc cap");
    EXPECT(rb_out_capacity(RB_OUT_PRO_HUB) == 8, "hub cap");
    EXPECT(rb_gadget_functions(RB_OUT_PRO_HUB, 8) == 8, "8 hid functions");
    EXPECT(rb_gadget_functions(RB_OUT_GC_ADAPTER, 8) == 2, "2 gc hid");
    EXPECT(rb_gadget_functions(RB_OUT_GC_ADAPTER, 3) == 1, "1 gc hid");
    na = rb_plan_assigns(8, 1, RB_OUT_PRO_HUB, a, 8);
    EXPECT(na == 8 && a[7].port == 7, "hub 8 ports");
}

static void test_translate_latency(void) {
    rb_config_t cfg;
    rb_config_default(&cfg);
    rb_pad_t in, out;
    rb_pad_neutral(&in);
    in.kind = RB_KIND_XBOXSERIES;
    in.valid = true;
    in.buttons[0] = 0x08;
    in.lx = 0x910;
    uint8_t report[64];
    uint8_t gc[37];
    rb_pad_t four[4];
    for (int i = 0; i < 4; i++) four[i] = in;
    uint64_t t0 = rb_now_ns();
    for (int i = 0; i < 10000; i++) {
        rb_translate(&in, &cfg, &out);
        rb_s1_encode_input(&out, &cfg, (uint8_t)i, report);
        rb_gc_encode_adapter(four, gc);
    }
    uint64_t t1 = rb_now_ns();
    double us_each = (double)(t1 - t0) / 10000.0 / 1000.0;
    EXPECT(us_each < 10000.0, "each translate+encode under 10ms");
    EXPECT(report[0] == 0x30, "report id");
    EXPECT(report[3] == 0x08, "A forwarded");
    EXPECT(gc[0] == 0x21, "gc report");
    printf("translate+encode+gc: %.3f µs (budget 10000 µs)\n", us_each);
}

static void test_rumble(void) {
    rb_rumble_t r;
    uint8_t idle[10] = {0x10, 0, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};
    EXPECT(rb_rumble_parse_s1(idle, 10, &r), "parse idle");
    EXPECT(!r.active && r.amp_l == 0 && r.amp_r == 0, "idle amps");

    uint8_t live[16];
    memset(live, 0, sizeof(live));
    live[0] = 0x10;
    rb_rumble_encode_hd(live + 2, 200);
    rb_rumble_encode_hd(live + 6, 0);
    EXPECT(rb_rumble_parse_s1(live, 10, &r), "parse live");
    EXPECT(r.active && r.amp_l > 0 && r.amp_r == 0, "left HD rumble");
    EXPECT(r.hd, "hd flag");

    EXPECT(rb_kind_hd_rumble(RB_KIND_S2_PRO), "s2 hd");
    EXPECT(rb_kind_hd_rumble(RB_KIND_S1_PRO), "s1 hd");
    EXPECT(rb_kind_hd_rumble(RB_KIND_S2_GC), "nso gc hd");
    EXPECT(!rb_kind_hd_rumble(RB_KIND_XBOX360), "360 erm");
    EXPECT(!rb_kind_hd_rumble(RB_KIND_DUALSENSE), "ds erm");

    uint8_t out[64];
    int n = rb_rumble_encode_for(RB_KIND_S1_PRO, &r, out, 64);
    EXPECT(n == 10 && out[0] == 0x10, "s1 hd forward");
    EXPECT(memcmp(out + 2, live + 2, 8) == 0, "raw hd bytes forwarded");

    n = rb_rumble_encode_for(RB_KIND_XBOX360, &r, out, 64);
    EXPECT(n == 8 && out[0] == 0x00 && out[1] == 0x08 && out[3] > 0, "360 erm");

    n = rb_rumble_encode_for(RB_KIND_S2_PRO, &r, out, 64);
    EXPECT(n == 16 && out[0] == 0x02, "s2 hd");

    uint8_t gc[8] = {0x11, 1, 0, 1, 0};
    rb_rumble_t ports[4];
    EXPECT(rb_rumble_parse_gc(gc, 5, ports), "gc rumble parse");
    EXPECT(ports[0].active && !ports[1].active && ports[2].active && !ports[3].active, "gc ports");
    uint8_t enc[8];
    n = rb_rumble_encode_gc_ports(ports, enc, 8);
    EXPECT(n == 5 && enc[0] == 0x11 && enc[1] == 1 && enc[3] == 1, "gc encode");
}

static void test_spi_cal(void) {
    uint8_t buf[18];
    int n = rb_spi_read(0x603D, 18, buf);
    EXPECT(n == 18, "cal size");
    uint16_t x, y;
    rb_unpack12(buf + 3, &x, &y);
    EXPECT(x == 0x800 && y == 0x800, "left center");
}

int main(void) {
    test_pack_roundtrip();
    test_s2_05_buttons();
    test_s2_09_remap();
    test_gc_z_to_zr();
    test_s1_handshake();
    test_s1_input_passthrough();
    test_xbox360();
    test_xboxone();
    test_ds4();
    test_dualsense();
    test_gc_adapter_roundtrip();
    test_ids();
    test_split_even();
    test_rumble();
    test_spi_cal();
    test_translate_latency();
    if (g_fail) {
        fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
