#!/bin/sh
# One-box install for Raspberry Pi 4 / 5 (USB-C gadget + USB-A host).
set -eu
if [ "$(id -u)" -ne 0 ]; then
  echo "Re-run as root: sudo sh linux/install-pi.sh"
  exit 1
fi

apt-get update
apt-get install -y build-essential pkg-config libusb-1.0-0-dev

# dwc2 on the USB-C port (device/gadget). xHCI on USB-A stays host.
BOOTCFG=""
for f in /boot/firmware/config.txt /boot/config.txt; do
  if [ -f "$f" ]; then BOOTCFG=$f; break; fi
done
if [ -n "$BOOTCFG" ] && ! grep -q 'dtoverlay=dwc2' "$BOOTCFG"; then
  printf '\n# Railbridge gadget on USB-C\ndtoverlay=dwc2,dr_mode=peripheral\n' >> "$BOOTCFG"
  echo "Enabled dwc2 peripheral on $BOOTCFG (reboot required)."
fi

if [ -d /etc/modules-load.d ]; then
  printf 'dwc2\nlibcomposite\n' > /etc/modules-load.d/railbridge.conf
fi

install -m 0644 linux/99-railbridge.rules /etc/udev/rules.d/99-railbridge.rules
udevadm control --reload-rules || true

make -C "$(dirname "$0")/.." clean all
make -C "$(dirname "$0")/.." install

install -m 0644 linux/railbridge.service /etc/systemd/system/railbridge.service
systemctl daemon-reload
systemctl enable railbridge.service

echo
echo "Plug pads into USB-A (a hub is fine): Switch 2/1, Xbox, DualShock/DualSense."
echo "Plug the Pi USB-C port into the Switch 1 dock with a USB-C to USB-A cable."
echo "Then: sudo systemctl start railbridge   (or reboot)"
echo "Identities: --out hub  (Pro HID ×4)   --out gc-adapter  (WUP-028 ×4)"
