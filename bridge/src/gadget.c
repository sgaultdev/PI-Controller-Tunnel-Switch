#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

/* Nintendo Pro Controller HID report descriptor (64-byte vendor reports). */
static const uint8_t k_hid_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x06, 0x01, 0xFF,
    0x85, 0x21, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x30, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x31, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x32, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x33, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x3F, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x81, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02,
    0x85, 0x01, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x02,
    0x85, 0x10, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x02,
    0x85, 0x11, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x02,
    0x85, 0x12, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x02,
    0x85, 0x80, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x02,
    0xC0
};

/* Official Wii U / Switch GameCube Adapter: report 0x21 (37 bytes in) / 0x11 (5 bytes out). */
static const uint8_t k_gc_desc[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01,
    0xA1, 0x00, 0x85, 0x11, 0x19, 0x00, 0x2A, 0xFF, 0x00,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x05, 0x91, 0x00, 0xC0,
    0xA1, 0x00, 0x85, 0x21, 0x15, 0x00, 0x25, 0xFF, 0x75, 0x08, 0x95, 0x25, 0x81, 0x00, 0xC0,
    0xC0
};

static int g_gadget_count = 0;
static int g_gadget_rlen = 64;

#if defined(__linux__)

static int write_file(const char *path, const void *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t w = fwrite(data, 1, n, f);
    fclose(f);
    return w == n ? 0 : -1;
}

static int write_str(const char *path, const char *s) {
    return write_file(path, s, strlen(s));
}

static int ensure_dir(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 0;
    return -1;
}

static int pick_udc(char *out, size_t cap) {
    DIR *d = opendir("/sys/class/udc");
    if (!d) return -1;
    struct dirent *e;
    int rc = -1;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(out, cap, "%.*s", (int)cap - 1, e->d_name);
        rc = 0;
        break;
    }
    closedir(d);
    return rc;
}

static int g_rumble_outs = 0; /* how many HID functions expose an OUT ep (HD rumble) */

static void unlink_hid_links(int functions) {
    for (int i = 0; i < functions; i++) {
        char link[192];
        snprintf(link, sizeof(link), "/sys/kernel/config/usb_gadget/railbridge/configs/c.1/hid.usb%d", i);
        unlink(link);
    }
}

/* Remove a HID function directory so no_out_endpoint can be rewritten on recreate. */
static void remove_hid_fn(int index) {
    char fn[128], path[192];
    snprintf(fn, sizeof(fn), "/sys/kernel/config/usb_gadget/railbridge/functions/hid.usb%d", index);
    snprintf(path, sizeof(path), "%s/report_desc", fn);
    unlink(path);
    snprintf(path, sizeof(path), "%s/protocol", fn);
    unlink(path);
    snprintf(path, sizeof(path), "%s/subclass", fn);
    unlink(path);
    snprintf(path, sizeof(path), "%s/report_length", fn);
    unlink(path);
    snprintf(path, sizeof(path), "%s/no_out_endpoint", fn);
    unlink(path);
    rmdir(fn);
}

static int add_hid_fn(int index, const uint8_t *desc, size_t desclen, const char *rlen, int no_out) {
    char fn[128], path[192], link[192];
    snprintf(fn, sizeof(fn), "/sys/kernel/config/usb_gadget/railbridge/functions/hid.usb%d", index);
    /* Always recreate so OUT vs no-OUT can change across bind retries. */
    remove_hid_fn(index);
    if (ensure_dir(fn) != 0) return -1;
    snprintf(path, sizeof(path), "%s/protocol", fn);
    write_str(path, "0");
    snprintf(path, sizeof(path), "%s/subclass", fn);
    write_str(path, "0");
    snprintf(path, sizeof(path), "%s/report_length", fn);
    write_str(path, rlen);
    snprintf(path, sizeof(path), "%s/report_desc", fn);
    write_file(path, desc, desclen);
    snprintf(path, sizeof(path), "%s/no_out_endpoint", fn);
    if (no_out)
        write_str(path, "1");
    else
        unlink(path); /* ensure OUT endpoint is requested */
    snprintf(link, sizeof(link), "/sys/kernel/config/usb_gadget/railbridge/configs/c.1/hid.usb%d", index);
    unlink(link);
    if (symlink(fn, link) != 0) return -1;
    return 0;
}

/* Install Pro HID functions: first out_count with IN+OUT (HD rumble), rest IN-only. */
static int install_pro_hids(int functions, int out_count) {
    if (out_count < 0) out_count = 0;
    if (out_count > functions) out_count = functions;
    unlink_hid_links(functions);
    for (int i = 0; i < functions; i++) {
        if (add_hid_fn(i, k_hid_desc, sizeof(k_hid_desc), "64", i >= out_count) != 0)
            return -1;
    }
    return 0;
}

int rb_gadget_bind(rb_out_mode_t mode, int functions) {
    const char *root = "/sys/kernel/config/usb_gadget/railbridge";
    if (ensure_dir("/sys/kernel/config/usb_gadget") != 0) {
        fprintf(stderr, "configfs gadget not available. On a Pi: enable dwc2 (dtoverlay=dwc2) and libcomposite.\n");
        return -1;
    }
    write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");
    ensure_dir(root);

    int gc = (mode == RB_OUT_GC_ADAPTER);
    if (functions < 1) functions = 1;
    if (gc) {
        if (functions < 1) functions = 1;
        if (functions > RB_MAX_GC_HID) functions = RB_MAX_GC_HID;
    } else if (functions > RB_MAX_HIDG) {
        functions = RB_MAX_HIDG;
    }

    write_str("/sys/kernel/config/usb_gadget/railbridge/idVendor", "0x057e");
    write_str("/sys/kernel/config/usb_gadget/railbridge/idProduct", gc ? "0x0337" : "0x2009");
    write_str("/sys/kernel/config/usb_gadget/railbridge/bcdDevice", "0x0200");
    write_str("/sys/kernel/config/usb_gadget/railbridge/bcdUSB", "0x0200");
    write_str("/sys/kernel/config/usb_gadget/railbridge/bDeviceClass", "0x00");
    write_str("/sys/kernel/config/usb_gadget/railbridge/bDeviceSubClass", "0x00");
    write_str("/sys/kernel/config/usb_gadget/railbridge/bDeviceProtocol", "0x00");
    ensure_dir("/sys/kernel/config/usb_gadget/railbridge/strings/0x409");
    write_str("/sys/kernel/config/usb_gadget/railbridge/strings/0x409/manufacturer", "Nintendo Co., Ltd.");
    write_str("/sys/kernel/config/usb_gadget/railbridge/strings/0x409/product",
              gc ? "WUP-028" : "Pro Controller");
    write_str("/sys/kernel/config/usb_gadget/railbridge/strings/0x409/serialnumber", "000000000001");
    ensure_dir("/sys/kernel/config/usb_gadget/railbridge/configs/c.1");
    ensure_dir("/sys/kernel/config/usb_gadget/railbridge/configs/c.1/strings/0x409");
    write_str("/sys/kernel/config/usb_gadget/railbridge/configs/c.1/strings/0x409/configuration",
              gc ? "GameCube Adapter" : "Pro Controller");
    write_str("/sys/kernel/config/usb_gadget/railbridge/configs/c.1/MaxPower", "500");
    write_str("/sys/kernel/config/usb_gadget/railbridge/configs/c.1/bmAttributes", "0xc0");

    char udc[128];
    if (pick_udc(udc, sizeof(udc)) != 0) {
        fprintf(stderr, "No USB device controller. Pi 4/5 USB-C needs dtoverlay=dwc2.\n");
        return -1;
    }

    g_rumble_outs = 0;
    write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");

    if (gc) {
        /* Dual WUP-028: both HID functions always have OUT (rumble for all 8 ports). */
        for (int i = 0; i < functions; i++) {
            if (add_hid_fn(i, k_gc_desc, sizeof(k_gc_desc), "37", 0) != 0) return -1;
        }
        if (write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", udc) != 0) {
            fprintf(stderr, "UDC bind failed for GC ×%d, retrying single adapter.\n", functions);
            write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");
            unlink_hid_links(functions);
            functions = 1;
            if (add_hid_fn(0, k_gc_desc, sizeof(k_gc_desc), "37", 0) != 0) return -1;
            if (write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", udc) != 0) {
                fprintf(stderr, "Failed to bind UDC %s\n", udc);
                return -1;
            }
        }
        g_rumble_outs = functions; /* each GC HID covers 4 ports of rumble */
    } else {
        /*
         * Prefer HD rumble (IN+OUT) on every Pro HID, up to RB_MAX_HIDG (8).
         * dwc2 may run out of endpoints: keep all input functions, peel OUT
         * endpoints from the highest index until bind succeeds.
         */
        int bound = 0;
        for (int out_count = functions; out_count >= 0; out_count--) {
            write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");
            if (install_pro_hids(functions, out_count) != 0) {
                fprintf(stderr, "Failed to install Pro HID functions (out=%d)\n", out_count);
                continue;
            }
            if (write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", udc) == 0) {
                g_rumble_outs = out_count;
                bound = 1;
                if (out_count < functions)
                    fprintf(stderr,
                            "Endpoint budget: HD rumble OUT on pads 1–%d; pads %d–%d are IN-only.\n",
                            out_count, out_count + 1, functions);
                break;
            }
            fprintf(stderr, "UDC bind failed with %d HID / %d OUT, retrying fewer OUT endpoints…\n",
                    functions, out_count);
        }
        if (!bound) {
            /* Last resort: fewer HID functions, all with OUT. */
            for (int n = functions - 1; n >= 1; n--) {
                write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");
                unlink_hid_links(functions);
                if (install_pro_hids(n, n) != 0) continue;
                if (write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", udc) == 0) {
                    functions = n;
                    g_rumble_outs = n;
                    bound = 1;
                    fprintf(stderr, "Fell back to %d Pro HID functions (all with HD rumble).\n", n);
                    break;
                }
            }
        }
        if (!bound) {
            fprintf(stderr, "Failed to bind UDC %s\n", udc);
            return -1;
        }
    }

    g_gadget_count = functions;
    g_gadget_rlen = gc ? 37 : 64;
    if (gc) {
        fprintf(stderr, "Gadget bound on UDC %s as 057e:0337 GameCube Adapter ×%d (%d ports, rumble on all)\n",
                udc, functions, functions * RB_GC_PORTS);
    } else if (functions > 1) {
        fprintf(stderr, "Gadget bound on UDC %s as 057e:2009 Pro hub (%d HID, HD rumble on %d)\n",
                udc, functions, g_rumble_outs);
    } else {
        fprintf(stderr, "Gadget bound on UDC %s as 057e:2009 Pro Controller (HD rumble %s)\n",
                udc, g_rumble_outs ? "yes" : "no");
    }
    return 0;
}

int rb_gadget_open_index(int i) {
    char path[64];
    snprintf(path, sizeof(path), "/dev/hidg%d", i);
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror(path);
        return -1;
    }
    return fd;
}

int rb_gadget_count(void) { return g_gadget_count; }
int rb_gadget_report_len(void) { return g_gadget_rlen; }

int rb_gadget_write(int fd, const uint8_t *rpt, size_t n) {
    ssize_t w = write(fd, rpt, n);
    return w == (ssize_t)n ? 0 : -1;
}

int rb_gadget_read(int fd, uint8_t *rpt, size_t n) {
    ssize_t r = read(fd, rpt, n);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return (int)r;
}

void rb_gadget_unbind(void) {
    write_str("/sys/kernel/config/usb_gadget/railbridge/UDC", "");
    g_gadget_count = 0;
}

#else

int rb_gadget_bind(rb_out_mode_t mode, int functions) {
    (void)mode;
    (void)functions;
    fprintf(stderr,
            "USB gadget (device) mode is a Linux UDC feature.\n"
            "On Windows, run this machine as --host --relay <pi-ip>:7433\n"
            "and run railbridge --gadget --listen 0.0.0.0:7433 on a Raspberry Pi\n"
            "plugged into the Switch 1 dock.\n");
    return -1;
}

int rb_gadget_open_index(int i) {
    (void)i;
    return -1;
}
int rb_gadget_count(void) { return 0; }
int rb_gadget_report_len(void) { return 64; }
int rb_gadget_write(int fd, const uint8_t *rpt, size_t n) {
    (void)fd;
    (void)rpt;
    (void)n;
    return -1;
}
int rb_gadget_read(int fd, uint8_t *rpt, size_t n) {
    (void)fd;
    (void)rpt;
    (void)n;
    return -1;
}
void rb_gadget_unbind(void) {}

#endif
