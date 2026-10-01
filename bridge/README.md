# Railbridge

Wired adapter that presents **Switch 2, Switch 1, Xbox, DualShock 4, DualSense,
and GameCube-adapter** pads to an original **Nintendo Switch** as either official
**Pro Controllers** or the official **Wii U / Switch GameCube adapter**. No
console modding. Latest-wins translation, no queue.

```
pads  --USB-A-->  Railbridge (Pi)  --USB-C-->  Switch 1 dock
        host                       gadget
        mixed VIDs                 057e:2009 hub  or  057e:0337 adapter
```

## Raspberry Pi 4 / 5 (one box)

USB-A ports are host (a USB hub here is fine). USB-C is the dwc2 gadget.

```
sudo sh linux/install-pi.sh
sudo reboot
sudo railbridge --auto --out hub          # up to 8 Pro HID on one cable (HD rumble when UDC allows)
sudo railbridge --auto --out gc-adapter   # official WUP-028 ×1 or ×2 (4 or 8 ports, rumble on all)
```

- `--out auto` — one pad → single Pro; two or more → Pro hub
- `--out pro` — one 057e:2009
- `--out hub` — up to eight Pro Controller HID functions; tries HD rumble OUT on all 8
- `--out gc-adapter` — 057e:0337, 37-byte 0x21; one or two HID functions (4 or 8 ports, rumble on all)

A Pi has one gadget port. Extra dock cables are extra Pi gadgets; pass multiple
`--relay host:port` and `--links N` to split pads evenly.

## Windows

Windows hosts pads but cannot become a USB device. Pair with a Pi gadget:

```
# Pi
sudo ./railbridge --gadget --listen 0.0.0.0:7433 --out gc-adapter

# Windows
railbridge.exe --host --relay <pi-ip>:7433
```

## Inputs

| Family | USB ID | Wake |
| --- | --- | --- |
| Switch 2 Pro / NSO GC / Joy-Con 2 | 057e:2069 / 2073 / 2066 | vendor bulk 03 91 … |
| Switch 1 Pro / Joy-Con | 057e:2009 / 2006 / 2007 | 0x80 handshake, mode 0x30 |
| Xbox 360 | 045e:028e | XInput interrupt |
| Xbox One / Series | 045e:02ea / 0b12 … | GIP 0x05 0x20 power-on |
| DualShock 4 / DualSense | 054c:09cc / 0ce6 | HID 0x01 (wired) |
| Official GC adapter | 057e:0337 | 0x13 start, 4 ports |

Face buttons map by **position** (Xbox A / DualSense Cross → Nintendo B).

## Build

```
make test          # protocol tests, no USB required
make               # railbridge binary (libusb if present)
```
