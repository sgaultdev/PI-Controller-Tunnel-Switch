#ifndef RAILBRIDGE_INTERNAL_H
#define RAILBRIDGE_INTERNAL_H

#include "railbridge.h"

#ifndef RB_NO_USB
#include <libusb-1.0/libusb.h>

typedef struct {
    libusb_device_handle *h;
    uint8_t bulk_out, bulk_in, int_in, int_out;
    int iface_hid, iface_vendor;
    uint16_t vid, pid;
    rb_controller_kind_t kind;
    uint8_t bus, addr;
    int slot;
    int gc_ports; /* 4 if this handle is an official GC adapter */
} rb_host_t;

int rb_host_open_all(libusb_context *ctx, rb_host_t *out, int cap, const rb_host_t *already, int nalready);
void rb_host_close(rb_host_t *h);
int rb_host_wakeup(rb_host_t *h);
int rb_host_read_report(rb_host_t *h, uint8_t *buf, int timeout_ms);
int rb_host_write(rb_host_t *h, const uint8_t *buf, int len, int timeout_ms);
int rb_host_rumble(rb_host_t *h, uint8_t l, uint8_t r);
int rb_host_rumble_hd(rb_host_t *h, const rb_rumble_t *r);
int rb_host_list(libusb_context *ctx);
#else
int rb_host_list(void *ctx);
#endif

int rb_gadget_bind(rb_out_mode_t mode, int functions);
int rb_gadget_open_index(int i);
int rb_gadget_count(void);
int rb_gadget_report_len(void);
int rb_gadget_write(int fd, const uint8_t *rpt, size_t n);
int rb_gadget_read(int fd, uint8_t *rpt, size_t n);
void rb_gadget_unbind(void);

#ifdef _WIN32
typedef uintptr_t rb_sock_t;
#else
typedef int rb_sock_t;
#endif

rb_sock_t rb_net_listen(const char *bind_addr, int port);
rb_sock_t rb_net_accept(rb_sock_t ls);
rb_sock_t rb_net_connect(const char *host, int port);
int rb_net_send(rb_sock_t s, uint8_t type, const uint8_t *data, uint8_t len);
int rb_net_recv(rb_sock_t s, uint8_t *type, uint8_t *data);

enum {
    RB_NET_S1_IN = 1,
    RB_NET_S1_OUT = 2,
    RB_NET_PAD = 3,
    RB_NET_GC = 4
};

#endif
