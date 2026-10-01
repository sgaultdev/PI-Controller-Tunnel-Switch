#include "railbridge.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#define RB_MAGIC 0x31304252u /* RB01 */
#define RB_NET_S1_IN 1
#define RB_NET_S1_OUT 2

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t type;
    uint8_t len;
    uint8_t data[64];
} rb_net_frame_t;
#pragma pack(pop)

#ifdef _WIN32
typedef SOCKET rb_sock_t;
#define RB_INV INVALID_SOCKET
#else
typedef int rb_sock_t;
#define RB_INV (-1)
#endif

static void nodelay(rb_sock_t s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
}

rb_sock_t rb_net_listen(const char *bind_addr, int port) {
#ifdef _WIN32
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
#endif
    rb_sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == RB_INV) return RB_INV;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = bind_addr ? inet_addr(bind_addr) : htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        fprintf(stderr, "bind %d failed\n", port);
        return RB_INV;
    }
    listen(s, 1);
    fprintf(stderr, "Gadget relay listening on %d\n", port);
    return s;
}

rb_sock_t rb_net_accept(rb_sock_t ls) {
    rb_sock_t c = accept(ls, NULL, NULL);
    if (c != RB_INV) nodelay(c);
    return c;
}

rb_sock_t rb_net_connect(const char *host, int port) {
#ifdef _WIN32
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
#endif
    rb_sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == RB_INV) return RB_INV;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = inet_addr(host);
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        fprintf(stderr, "connect %s:%d failed\n", host, port);
        return RB_INV;
    }
    nodelay(s);
    fprintf(stderr, "Connected gadget relay %s:%d\n", host, port);
    return s;
}

int rb_net_send(rb_sock_t s, uint8_t type, const uint8_t *data, uint8_t len) {
    rb_net_frame_t f;
    memset(&f, 0, sizeof(f));
    f.magic = RB_MAGIC;
    f.type = type;
    f.len = len;
    if (len) memcpy(f.data, data, len);
    const char *p = (const char *)&f;
    size_t left = sizeof(f);
    while (left) {
#ifdef _WIN32
        int n = send(s, p, (int)left, 0);
#else
        ssize_t n = send(s, p, left, 0);
#endif
        if (n <= 0) return -1;
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

int rb_net_recv(rb_sock_t s, uint8_t *type, uint8_t *data) {
    rb_net_frame_t f;
    char *p = (char *)&f;
    size_t left = sizeof(f);
    while (left) {
#ifdef _WIN32
        int n = recv(s, p, (int)left, 0);
#else
        ssize_t n = recv(s, p, left, 0);
#endif
        if (n <= 0) return -1;
        p += n;
        left -= (size_t)n;
    }
    if (f.magic != RB_MAGIC) return -1;
    *type = f.type;
    memcpy(data, f.data, 64);
    return f.len;
}
