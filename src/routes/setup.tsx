import { createFileRoute } from "@tanstack/react-router";
import { Shell } from "@/components/shell";
import { Badge } from "@/components/ui/badge";

export const Route = createFileRoute("/setup")({ component: SetupPage });

function SetupPage() {
  return (
    <Shell>
      <div className="mx-auto flex max-w-3xl flex-col gap-10">
        <header>
          <p className="text-xs font-medium tracking-[0.18em] text-fg-muted uppercase">Physical path</p>
          <h1 className="mt-2 text-3xl font-semibold tracking-tight">Many pads, one or more cables.</h1>
          <p className="mt-3 text-sm leading-relaxed text-fg-muted">
            Host ports take Switch 2, Switch 1, Xbox, DualShock 4, DualSense, and the official GameCube
            adapter. The Pi USB-C gadget is the only wire the Switch 1 has to see — a Pro Controller, an
            eight-function Pro hub with HD rumble when the UDC allows, or Nintendo’s WUP-028 (×1 or ×2 for 8 ports).
          </p>
        </header>

        <section className="rounded-xl border border-border bg-surface p-5 sm:p-6">
          <div className="flex items-center justify-between gap-3">
            <h2 className="text-sm font-medium">Raspberry Pi 4 / 5 — one cable</h2>
            <Badge tone="ok">Full stack</Badge>
          </div>
          <p className="mt-2 text-sm text-fg-muted">
            USB-A stays host (a USB hub here is fine). USB-C runs dwc2 peripheral. Up to eight HID functions
            share that one gadget cable (HD rumble OUT tried on all eight), or the gadget enumerates as
            057e:0337 with one or two GC adapters (4 or 8 ports, rumble on all).
          </p>
          <WiringPi />
          <ol className="mt-6 space-y-3 text-sm leading-relaxed text-fg-muted">
            <li>
              <span className="font-medium text-fg">1. Pads.</span> Any mix of Switch 2 Pro / NSO GameCube,
              Switch 1 Pro / Joy-Con, Xbox 360 / One / Series, DualShock 4, DualSense — into the Pi USB-A
              ports or a hub on those ports. Official Wii U/Switch GC adapter also reads as four inputs.
            </li>
            <li>
              <span className="font-medium text-fg">2. Console.</span> Pi USB-C to Switch 1 dock USB-A
              (USB-C to USB-A). Undocked tablet: USB-C to USB-C if that port is host.
            </li>
            <li>
              <span className="font-medium text-fg">3. Identity.</span>{" "}
              <code className="font-mono text-xs">--out hub</code> presents up to eight Pro Controller HID
              interfaces and requests HD rumble OUT on every pad (falls back if the UDC is short on
              endpoints).{" "}
              <code className="font-mono text-xs">--out gc-adapter</code> is the Smash-native WUP-028;
              with 5+ pads it opens a second GC HID function for 8 ports with rumble on all. One pad
              defaults to a single 057e:2009 Pro.
            </li>
            <li>
              <span className="font-medium text-fg">4. Run.</span>{" "}
              <code className="font-mono text-xs">sudo railbridge --auto --out hub</code>
            </li>
          </ol>
        </section>

        <section className="rounded-xl border border-border bg-surface p-5 sm:p-6">
          <div className="flex items-center justify-between gap-3">
            <h2 className="text-sm font-medium">Extra links (even split)</h2>
            <Badge tone="wire">Optional</Badge>
          </div>
          <p className="mt-2 text-sm text-fg-muted">
            A Pi has one USB gadget port. A second Pi (or another gadget appliance) is another cable into
            the dock. Pads are split evenly: five pads on two GC-adapter links become 3+2, never queued.
          </p>
          <ol className="mt-4 space-y-3 text-sm leading-relaxed text-fg-muted">
            <li>
              Pi A (dock): <code className="font-mono text-xs">sudo railbridge --gadget --listen 0.0.0.0:7433 --out gc-adapter</code>
            </li>
            <li>
              Pi B (second dock USB): same, port 7434.
            </li>
            <li>
              Host: <code className="font-mono text-xs">railbridge --host --relay 192.168.1.50:7433 --relay 192.168.1.51:7434 --links 2</code>
            </li>
          </ol>
        </section>

        <section className="rounded-xl border border-border bg-surface p-5 sm:p-6">
          <div className="flex items-center justify-between gap-3">
            <h2 className="text-sm font-medium">Windows</h2>
            <Badge tone="wire">Host + relay</Badge>
          </div>
          <p className="mt-2 text-sm text-fg-muted">
            Windows hosts every pad family but cannot become a USB device. Relay translated reports to one
            or more Pi gadgets plugged into the Switch.
          </p>
          <WiringWin />
        </section>

        <section>
          <h2 className="text-sm font-medium">What the Switch 1 must see</h2>
          <dl className="mt-4 grid gap-3 sm:grid-cols-2">
            <Row k="Pro / hub" v="057e:2009 · Nintendo Co., Ltd. / Pro Controller" />
            <Row k="GC adapter" v="057e:0337 · WUP-028 · 37-byte 0x21" />
            <Row k="Hub functions" v="Up to 8 HID · HD rumble OUT preferred on all" />
            <Row k="GC ports" v="4 or 8 · rumble on every port via dual WUP-028" />
            <Row k="Latency" v="Event-driven · 8 ms keepalive · HD rumble · <10 ms software" />
          </dl>
          <p className="mt-4 text-sm leading-relaxed text-fg-muted">
            No payload, no CFW. Pro mode matches a wired official Pro Controller. GC adapter mode matches
            the official Wii U/Switch adapter Smash already speaks. Composite hub is one USB device with
            multiple HID interfaces — the software equivalent of a splitter on a single gadget port.
          </p>
        </section>
      </div>
    </Shell>
  );
}

function Row({ k, v }: { k: string; v: string }) {
  return (
    <div className="rounded-lg border border-border bg-surface px-4 py-3">
      <dt className="text-[11px] tracking-wide text-fg-subtle uppercase">{k}</dt>
      <dd className="mt-1 font-mono text-sm">{v}</dd>
    </div>
  );
}

function WiringPi() {
  return (
    <svg viewBox="0 0 640 140" className="mt-6 w-full text-fg" role="img" aria-label="Pi wiring">
      <rect x="8" y="12" width="70" height="36" rx="10" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <rect x="8" y="52" width="70" height="36" rx="10" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <rect x="8" y="92" width="70" height="36" rx="10" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <text x="43" y="34" textAnchor="middle" className="fill-fg" style={{ fontSize: 9 }}>S2 / Xbox</text>
      <text x="43" y="74" textAnchor="middle" className="fill-fg" style={{ fontSize: 9 }}>DualSense</text>
      <text x="43" y="114" textAnchor="middle" className="fill-fg" style={{ fontSize: 9 }}>S1 Pro</text>
      <path d="M78 30 H150" className="stroke-wire" strokeWidth="2" fill="none" />
      <path d="M78 70 H150" className="stroke-wire" strokeWidth="2" fill="none" />
      <path d="M78 110 H150" className="stroke-wire" strokeWidth="2" fill="none" />
      <rect x="150" y="32" width="200" height="76" rx="12" className="fill-bg" stroke="currentColor" strokeOpacity={0.25} />
      <text x="250" y="62" textAnchor="middle" className="fill-fg" style={{ fontSize: 12, fontWeight: 500 }}>
        Raspberry Pi
      </text>
      <text x="250" y="82" textAnchor="middle" className="fill-fg-muted" style={{ fontSize: 10 }}>
        USB-A host · USB-C gadget
      </text>
      <path d="M350 70 H430" className="stroke-wire" strokeWidth="2" fill="none" />
      <rect x="430" y="38" width="180" height="64" rx="16" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <text x="520" y="66" textAnchor="middle" className="fill-fg" style={{ fontSize: 11 }}>
        Switch 1 dock
      </text>
      <text x="520" y="84" textAnchor="middle" className="fill-fg-muted" style={{ fontSize: 9 }}>
        one cable · hub or WUP-028
      </text>
    </svg>
  );
}

function WiringWin() {
  return (
    <svg viewBox="0 0 640 120" className="mt-6 w-full text-fg" role="img" aria-label="Windows wiring">
      <rect x="8" y="28" width="120" height="64" rx="16" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <text x="68" y="64" textAnchor="middle" className="fill-fg" style={{ fontSize: 11 }}>
        Mixed pads
      </text>
      <path d="M128 60 H190" className="stroke-wire" strokeWidth="2" fill="none" />
      <rect x="190" y="22" width="140" height="76" rx="12" className="fill-bg" stroke="currentColor" strokeOpacity={0.25} />
      <text x="260" y="56" textAnchor="middle" className="fill-fg" style={{ fontSize: 12, fontWeight: 500 }}>
        Windows
      </text>
      <text x="260" y="74" textAnchor="middle" className="fill-fg-muted" style={{ fontSize: 10 }}>
        USB host
      </text>
      <path d="M330 60 H390" stroke="currentColor" strokeOpacity={0.35} strokeWidth="2" strokeDasharray="4 4" fill="none" />
      <rect x="390" y="22" width="110" height="76" rx="12" className="fill-bg" stroke="currentColor" strokeOpacity={0.25} />
      <text x="445" y="64" textAnchor="middle" className="fill-fg" style={{ fontSize: 11 }}>
        Pi gadget
      </text>
      <path d="M500 60 H548" className="stroke-wire" strokeWidth="2" fill="none" />
      <rect x="548" y="28" width="80" height="64" rx="14" className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <text x="588" y="64" textAnchor="middle" className="fill-fg" style={{ fontSize: 10 }}>
        S1
      </text>
    </svg>
  );
}
