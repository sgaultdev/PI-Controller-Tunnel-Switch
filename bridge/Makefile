CC ?= gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -D_GNU_SOURCE
INCLUDES = -Iinclude -Isrc
LDFLAGS ?=
LIBS_PTHREAD = -lpthread

CORE_SRC = src/core.c src/parse.c src/rumble.c
USB_SRC = src/host.c src/gadget.c src/net.c src/main.c

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all test clean install

all: railbridge test

test: tests/test_core
	./tests/test_core

tests/test_core: tests/test_core.c $(CORE_SRC) include/railbridge.h
	$(CC) $(CFLAGS) $(INCLUDES) -DRB_NO_USB tests/test_core.c $(CORE_SRC) -o $@

libusb_cflags := $(shell pkg-config --cflags libusb-1.0 2>/dev/null)
libusb_libs := $(shell pkg-config --libs libusb-1.0 2>/dev/null)

ifeq ($(libusb_libs),)
railbridge: $(CORE_SRC) $(USB_SRC) include/railbridge.h src/internal.h
	@echo "libusb-1.0 not found — building USB-less binary (sim/gadget/net only)"
	$(CC) $(CFLAGS) $(INCLUDES) -DRB_NO_USB $(CORE_SRC) src/gadget.c src/net.c src/main.c $(LIBS_PTHREAD) -o $@
else
railbridge: $(CORE_SRC) $(USB_SRC) include/railbridge.h src/internal.h
	$(CC) $(CFLAGS) $(INCLUDES) $(libusb_cflags) $(CORE_SRC) $(USB_SRC) $(libusb_libs) $(LIBS_PTHREAD) -o $@
endif

install: railbridge
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 railbridge $(DESTDIR)$(BINDIR)/railbridge
	install -d $(DESTDIR)/etc/udev/rules.d
	install -m 0644 linux/99-railbridge.rules $(DESTDIR)/etc/udev/rules.d/99-railbridge.rules || true

clean:
	rm -f railbridge tests/test_core
