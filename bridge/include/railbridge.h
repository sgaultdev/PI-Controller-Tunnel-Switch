#ifndef RAILBRIDGE_H
#define RAILBRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RB_REPORT_LEN 64
#define RB_GC_REPORT_LEN 37
#define RB_GC_PORTS 4
#define RB_MAX_PADS 8
#define RB_MAX_LINKS 4
#define RB_MAX_HIDG 8
#define RB_MAX_GC_HID 2

#define RB_VID_NINTENDO 0x057E
#define RB_VID_MICROSOFT 0x045E
#define RB_VID_SONY 0x054C

#define RB_PID_S1_PRO 0x2009
#define RB_PID_S1_JC_L 0x2006
#define RB_PID_S1_JC_R 0x2007
#define RB_PID_GC_ADAPTER 0x0337
#define RB_PID_S2_PRO 0x2069
#define RB_PID_S2_GC_A 0x2073
#define RB_PID_S2_GC_B 0x206A
#define RB_PID_S2_GC_C 0x206B
#define RB_PID_JC2_R 0x2066
#define RB_PID_JC2_L 0x2067

#define RB_PID_XBOX360 0x028E
#define RB_PID_XBOX360_W 0x028F
#define RB_PID_XBOXONE 0x02D1
#define RB_PID_XBOXONE_S 0x02EA
#define RB_PID_XBOXONE_S2 0x02FD
#define RB_PID_XBOXONE_ELITE 0x02E3
#define RB_PID_XBOXONE_ELITE2 0x0B00
#define RB_PID_XBOX_SERIES 0x0B12
#define RB_PID_XBOX_SERIES2 0x0B13

#define RB_PID_DS4 0x05C4
#define RB_PID_DS4_V2 0x09CC
#define RB_PID_DS4_DONGLE 0x0BA0
#define RB_PID_DUALSENSE 0x0CE6
#define RB_PID_DUALSENSE_EDGE 0x0DF2

#define RB_STICK_CENTER 0x800
#define RB_STICK_MAX 0xFFF

typedef enum {
    RB_KIND_UNKNOWN = 0,
    RB_KIND_S2_PRO = 1,
    RB_KIND_S2_GC = 2,
    RB_KIND_JC2_L = 3,
    RB_KIND_JC2_R = 4,
    RB_KIND_S1_PRO = 5,
    RB_KIND_S1_JC_L = 6,
    RB_KIND_S1_JC_R = 7,
    RB_KIND_XBOX360 = 8,
    RB_KIND_XBOXONE = 9,
    RB_KIND_XBOXSERIES = 10,
    RB_KIND_DS4 = 11,
    RB_KIND_DUALSENSE = 12,
    RB_KIND_GC_ADAPTER = 13
} rb_controller_kind_t;

typedef enum {
    RB_FAMILY_UNKNOWN = 0,
    RB_FAMILY_NINTENDO_S2,
    RB_FAMILY_NINTENDO_S1,
    RB_FAMILY_XBOX,
    RB_FAMILY_SONY,
    RB_FAMILY_GC_ADAPTER
} rb_family_t;

typedef enum {
    RB_S2_IDLE = 0,
    RB_S2_ENUMERATED,
    RB_S2_WAKE_USB,
    RB_S2_ENABLE_HID,
    RB_S2_SELECT_REPORT,
    RB_S2_STREAMING,
    RB_S2_ERROR
} rb_s2_state_t;

typedef enum {
    RB_S1_IDLE = 0,
    RB_S1_ATTACHED,
    RB_S1_CONN_STATUS,
    RB_S1_HANDSHAKE,
    RB_S1_BAUD,
    RB_S1_USB_ONLY,
    RB_S1_SUBCMD,
    RB_S1_STREAMING,
    RB_S1_ERROR
} rb_s1_state_t;

typedef enum {
    RB_OUT_AUTO = 0,
    RB_OUT_PRO = 1,
    RB_OUT_PRO_HUB = 2,
    RB_OUT_GC_ADAPTER = 3
} rb_out_mode_t;

typedef struct {
    uint8_t raw[8];
    uint8_t amp_l;
    uint8_t amp_r;
    uint8_t hf_l, hf_r;
    uint8_t lf_l, lf_r;
    bool active;
    bool hd;
} rb_rumble_t;

typedef struct {
    uint16_t lx, ly, rx, ry;
    uint8_t lt, rt;
    uint8_t buttons[3];
    uint8_t extra;
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint8_t report_id;
    rb_controller_kind_t kind;
    uint16_t vid, pid;
    uint32_t counter;
    uint8_t slot;
    bool valid;
} rb_pad_t;

typedef struct {
    uint8_t mac[6];
    uint8_t player_led;
    uint8_t rumble_l;
    uint8_t rumble_r;
    bool invert_y;
    bool gc_analog_shoulders;
    uint8_t gc_trigger_threshold;
    rb_out_mode_t out_mode;
    int links;
    int gc_adapters;
} rb_config_t;

typedef struct {
    rb_s1_state_t state;
    uint8_t timer;
    uint8_t rumble_counter;
    uint8_t report_mode;
    bool imu_enabled;
    bool rumble_enabled;
    bool usb_only;
    uint8_t player_led;
    uint8_t last_cmd[RB_REPORT_LEN];
    uint8_t last_reply[RB_REPORT_LEN];
    rb_rumble_t rumble;
} rb_s1_machine_t;

typedef struct {
    uint8_t cmd;
    uint8_t sub;
    uint8_t payload[56];
    uint8_t payload_len;
} rb_s2_cmd_t;

typedef struct {
    int slot;
    int link;
    int port;
} rb_assign_t;

rb_controller_kind_t rb_kind_from_pid(uint16_t pid);
rb_controller_kind_t rb_kind_from_vid_pid(uint16_t vid, uint16_t pid);
rb_family_t rb_kind_family(rb_controller_kind_t kind);
const char *rb_kind_name(rb_controller_kind_t kind);
const char *rb_family_name(rb_family_t f);
const char *rb_s1_state_name(rb_s1_state_t s);
const char *rb_s2_state_name(rb_s2_state_t s);
const char *rb_out_mode_name(rb_out_mode_t m);
int rb_out_capacity(rb_out_mode_t m);
int rb_gadget_functions(rb_out_mode_t m, int npads);
rb_out_mode_t rb_out_resolve(rb_out_mode_t requested, int npads);
bool rb_kind_hd_rumble(rb_controller_kind_t kind);

void rb_config_default(rb_config_t *cfg);
void rb_pad_neutral(rb_pad_t *pad);

void rb_pack12(uint8_t out[3], uint16_t x, uint16_t y);
void rb_unpack12(const uint8_t in[3], uint16_t *x, uint16_t *y);
uint16_t rb_axis_u8_to12(uint8_t v);
uint16_t rb_axis_s16_to12(int16_t v);
uint8_t rb_axis_12_to_u8(uint16_t v);

bool rb_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out);
bool rb_s2_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out);
bool rb_s1_parse_report(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out);
bool rb_xbox360_parse(const uint8_t *buf, size_t len, rb_pad_t *out);
bool rb_xboxone_parse(const uint8_t *buf, size_t len, rb_controller_kind_t kind, rb_pad_t *out);
bool rb_ds4_parse(const uint8_t *buf, size_t len, rb_pad_t *out);
bool rb_dualsense_parse(const uint8_t *buf, size_t len, rb_pad_t *out);
int rb_gc_parse_adapter(const uint8_t *buf, size_t len, rb_pad_t out[RB_GC_PORTS]);
void rb_gc_encode_port(const rb_pad_t *pad, uint8_t out9[9]);
void rb_gc_encode_adapter(const rb_pad_t pads[RB_GC_PORTS], uint8_t out37[RB_GC_REPORT_LEN]);

void rb_s2_wakeup_commands(rb_controller_kind_t kind, rb_s2_cmd_t *out, int *count);
uint8_t rb_s2_preferred_report_id(rb_controller_kind_t kind);
int rb_xbox_wakeup_commands(rb_controller_kind_t kind, uint8_t *out, int cap);
int rb_s1_host_wakeup_commands(uint8_t cmds[][64], int *lens, int cap);

void rb_s1_encode_input(const rb_pad_t *pad, const rb_config_t *cfg, uint8_t timer, uint8_t *out64);
void rb_s1_machine_init(rb_s1_machine_t *m, const rb_config_t *cfg);
int rb_s1_handle_output(rb_s1_machine_t *m, const rb_config_t *cfg, const uint8_t *in, size_t len,
                        uint8_t *reply, size_t reply_cap);
void rb_s1_unsolicited_conn(const rb_config_t *cfg, uint8_t *out64);

void rb_rumble_clear(rb_rumble_t *r);
bool rb_rumble_parse_s1(const uint8_t *report, size_t len, rb_rumble_t *out);
bool rb_rumble_parse_gc(const uint8_t *report, size_t len, rb_rumble_t out[RB_GC_PORTS]);
void rb_rumble_encode_hd(uint8_t dst4[4], uint8_t amp);
int rb_rumble_encode_gc_ports(const rb_rumble_t ports[RB_GC_PORTS], uint8_t *out, int cap);
int rb_rumble_encode_for(rb_controller_kind_t kind, const rb_rumble_t *r, uint8_t *out, int cap);

void rb_translate(const rb_pad_t *in, const rb_config_t *cfg, rb_pad_t *out);
void rb_fill_factory_spi(uint8_t *spi, size_t len);
int rb_spi_read(uint32_t addr, uint8_t size, uint8_t *dst);

int rb_split_even(int npads, int nlinks, int cap_per, int *counts);
int rb_plan_assigns(int npads, int nlinks, rb_out_mode_t mode, rb_assign_t *out, int cap);

uint64_t rb_now_ns(void);

#ifdef __cplusplus
}
#endif

#endif
