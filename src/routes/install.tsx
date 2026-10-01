import { createFileRoute } from "@tanstack/react-router";
import { Download } from "lucide-react";
import { Shell } from "@/components/shell";
import { Button } from "@/components/ui/button";

export const Route = createFileRoute("/install")({ component: InstallPage });

const PI = `# on Raspberry Pi 4 or 5
sudo apt-get install -y build-essential pkg-config libusb-1.0-0-dev
tar xf railbridge-src.tar.gz
cd railbridge
sudo sh linux/install-pi.sh
sudo reboot
# pads in USB-A (hub OK), Pi USB-C to Switch 1 dock
sudo systemctl start railbridge
# or pick identity:
sudo railbridge --auto --out hub
sudo railbridge --auto --out gc-adapter`;

const WIN = `# Windows host + one or more Pi gadgets
# Pi A, plugged into the dock:
sudo ./railbridge --gadget --listen 0.0.0.0:7433 --out gc-adapter

# optional Pi B, second dock USB:
sudo ./railbridge --gadget --listen 0.0.0.0:7434 --out gc-adapter

# Windows (vcpkg libusb, then):
cmake -B build
cmake --build build --config Release
.\\build\\Release\\railbridge.exe --host --relay PI_A:7433 --relay PI_B:7434 --links 2`;

const TEST = `make test
# translate+encode+gc: ~0.04 µs  (budget 10 000 µs)`;

export function InstallPage() {
  return (
    <Shell>
      <div className="mx-auto flex max-w-3xl flex-col gap-10">
        <header>
          <p className="text-xs font-medium tracking-[0.18em] text-fg-muted uppercase">Native daemon</p>
          <h1 className="mt-2 text-3xl font-semibold tracking-tight">Install the software, not a webpage.</h1>
          <p className="mt-3 text-sm leading-relaxed text-fg-muted">
            This preview is the translator workbench. Host, gadget, hub, GameCube adapter, and handshake live
            in a C daemon for Raspberry Pi and Windows. Source, udev rules, systemd unit, and tests are in
            the archive.
          </p>
        </header>

        <Button asChild className="min-h-12 w-full sm:w-auto">
          <a href="/railbridge-src.tar.gz" download>
            <Download />
            Download source archive
          </a>
        </Button>

        <Block title="Raspberry Pi (recommended)" code={PI} />
        <Block title="Windows host + Pi gadget(s)" code={WIN} />
        <Block title="Protocol tests (no hardware)" code={TEST} />

        <section className="rounded-xl border border-border bg-surface p-5">
          <h2 className="text-sm font-medium">Auto-detect</h2>
          <p className="mt-2 text-sm leading-relaxed text-fg-muted">
            <code className="font-mono text-xs">railbridge --auto</code> watches USB for Nintendo (Switch 2,
            Switch 1, GC adapter), Microsoft (Xbox 360 / One / Series), and Sony (DualShock 4 / DualSense).
            New pads take the next slot; disconnects free it. Output identity is{" "}
            <code className="font-mono text-xs">--out auto|pro|hub|gc-adapter</code>. Extra{" "}
            <code className="font-mono text-xs">--relay</code> targets split pads evenly across gadget links.
          </p>
        </section>
      </div>
    </Shell>
  );
}

function Block({ title, code }: { title: string; code: string }) {
  return (
    <section>
      <h2 className="text-sm font-medium">{title}</h2>
      <pre className="mt-3 overflow-x-auto rounded-lg border border-border bg-surface p-4 font-mono text-[12px] leading-relaxed text-fg-muted">
        {code}
      </pre>
    </section>
  );
}
