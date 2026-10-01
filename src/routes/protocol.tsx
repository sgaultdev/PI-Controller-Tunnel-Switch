import { createFileRoute } from "@tanstack/react-router";
import { useMemo, useState } from "react";
import { Shell } from "@/components/shell";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import {
  defaultConfig,
  GC_ADAPTER_SCRIPT,
  handleS1Output,
  hexBytes,
  initS1Machine,
  S1_HANDSHAKE_SCRIPT,
  S1_HOST_WAKE_SCRIPT,
  S2_WAKE_SCRIPT,
  XBOX_WAKE_SCRIPT,
  type S1Machine,
} from "@/lib/bridge";

export const Route = createFileRoute("/protocol")({ component: ProtocolPage });

function ProtocolPage() {
  const cfg = useMemo(() => defaultConfig(), []);
  const [machine, setMachine] = useState<S1Machine>(() => initS1Machine(cfg));
  const [step, setStep] = useState(0);

  const consoleSteps = S1_HANDSHAKE_SCRIPT.filter((s) => s.dir === "console");

  function runNext() {
    const remaining = consoleSteps.slice(step);
    const next = remaining[0];
    if (!next) return;
    const bytes = new Uint8Array(64);
    bytes.set(next.bytes);
    setMachine((m) => {
      const copy: S1Machine = { ...m, log: [...m.log], lastCmd: m.lastCmd.slice(), lastReply: m.lastReply?.slice() ?? null };
      handleS1Output(copy, cfg, bytes);
      return copy;
    });
    setStep((s) => s + 1);
  }

  function reset() {
    setMachine(initS1Machine(cfg));
    setStep(0);
  }

  return (
    <Shell>
      <div className="mx-auto flex max-w-3xl flex-col gap-10">
        <header>
          <p className="text-xs font-medium tracking-[0.18em] text-fg-muted uppercase">Negotiation</p>
          <h1 className="mt-2 text-3xl font-semibold tracking-tight">Each family has a wake. The Switch has one.</h1>
          <p className="mt-3 text-sm leading-relaxed text-fg-muted">
            Host side: Switch 2 vendor-bulk, Switch 1 0x80 as host, Xbox GIP power-on, Sony HID already live,
            GameCube adapter 0x13. Gadget side: either a Pro Controller 0x80/SPI/0x30 session per HID
            function, or the official 37-byte WUP-028 report.
          </p>
        </header>

        <ScriptBlock title="Switch 2 wake (host, interface 1 bulk)" badge="057e:2069 / 2073" items={S2_WAKE_SCRIPT} />
        <ScriptBlock title="Switch 1 pad as input (host)" badge="057e:2009" items={S1_HOST_WAKE_SCRIPT} />
        <ScriptBlock title="Xbox One / Series GIP" badge="045e:0b12" items={XBOX_WAKE_SCRIPT} />
        <ScriptBlock title="GameCube adapter (gadget 057e:0337)" badge="WUP-028" items={GC_ADAPTER_SCRIPT} />

        <section>
          <div className="flex flex-wrap items-center justify-between gap-3">
            <h2 className="text-sm font-medium">Switch 1 handshake (Pro gadget)</h2>
            <Badge tone={machine.state === "streaming" ? "ok" : "neutral"}>{machine.state}</Badge>
          </div>
          <p className="mt-2 text-sm text-fg-muted">
            Step the console script. Hub mode runs this independently on each hidg function with a unique MAC
            and player LED.
          </p>
          <div className="mt-4 flex flex-wrap gap-2">
            <Button onClick={runNext} disabled={step >= consoleSteps.length}>
              {step >= consoleSteps.length ? "Complete" : "Issue next console command"}
            </Button>
            <Button variant="secondary" onClick={reset}>
              Reset
            </Button>
          </div>
          <ol className="mt-4 space-y-2">
            {S1_HANDSHAKE_SCRIPT.map((s, i) => (
              <li key={i} className="rounded-lg border border-border bg-surface px-4 py-3">
                <div className="flex items-center justify-between gap-2">
                  <p className="text-sm">{s.label}</p>
                  <span className="font-mono text-[10px] tracking-wide text-fg-subtle uppercase">{s.dir}</span>
                </div>
              </li>
            ))}
          </ol>
          {machine.log.length > 0 ? (
            <div className="mt-4 rounded-lg border border-border bg-bg p-4">
              <p className="text-xs font-medium text-fg-muted">Live log</p>
              <ul className="mt-2 space-y-1 font-mono text-[11px] text-fg-subtle">
                {machine.log.map((e, i) => (
                  <li key={i}>
                    {e.dir === "in" ? "←" : "→"} {e.label}
                  </li>
                ))}
              </ul>
            </div>
          ) : null}
        </section>

        <section>
          <h2 className="text-sm font-medium">Button map</h2>
          <p className="mt-2 text-sm leading-relaxed text-fg-muted">
            Face buttons are mapped by position, not label: Xbox A / DualSense Cross become Nintendo B
            (south); Xbox B / Circle become Nintendo A (east). Switch 1/2 bitfields pass through. GameCube Z
            is ZR; analog L/R cross a threshold onto ZL/ZR. C, GL, and GR have no Switch 1 equivalent and are
            dropped.
          </p>
        </section>
      </div>
    </Shell>
  );
}

function ScriptBlock({
  title,
  badge,
  items,
}: {
  title: string;
  badge: string;
  items: { bytes: number[]; label: string }[];
}) {
  return (
    <section>
      <div className="flex items-center justify-between gap-3">
        <h2 className="text-sm font-medium">{title}</h2>
        <Badge>{badge}</Badge>
      </div>
      <ol className="mt-4 space-y-2">
        {items.map((s, i) => (
          <li key={i} className="rounded-lg border border-border bg-surface px-4 py-3">
            <p className="text-sm">{s.label}</p>
            <p className="mt-1 font-mono text-[11px] text-fg-subtle">{hexBytes(s.bytes)}</p>
          </li>
        ))}
      </ol>
    </section>
  );
}
