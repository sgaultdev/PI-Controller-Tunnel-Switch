# Railbridge on Windows

Windows PCs are USB **hosts**. They can wake Switch 2, Switch 1, Xbox, DualShock
4, and DualSense pads and translate reports in well under 1 ms. They **cannot**
enumerate as a USB device to a Switch 1 — that needs a USB gadget controller.

## Recommended: Windows host + Pi gadget

1. Raspberry Pi 4/5 USB-C → Switch 1 dock (USB-C to USB-A cable).
2. On the Pi: `sudo ./railbridge --gadget --listen 0.0.0.0:7433 --out gc-adapter`
   (or `--out hub` / `--out pro`).
3. Pads → Windows USB (a hub is fine).
4. On Windows (same LAN): `railbridge.exe --host --relay 192.168.x.x:7433`

Repeat `--relay` for extra Pi gadgets; pads split evenly.

## Build

vcpkg:

```
vcpkg install libusb
cmake -B build -DCMAKE_TOOLCHAIN_FILE=.../vcpkg.cmake
cmake --build build --config Release
```

Bind WinUSB with [Zadig](https://zadig.akeo.ie/) on vendor interfaces that
libusb cannot claim (Switch 2 interface 1, Xbox GIP).
