import { createFileRoute } from "@tanstack/react-router";
import { Cable, Minus, Plus } from "lucide-react";
import { Shell } from "@/components/shell";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { PadFace } from "@/components/pad-face";
import { ReportStrip } from "@/components/report-strip";
import { useWorkbench } from "@/hooks/use-workbench";
import { isPressed, kindName, type ControllerKind, type OutMode } from "@/lib/bridge";
import { cn } from "@/lib/utils";

export const Route = createFileRoute("/")({ component: Home });

const OUTS: { id: OutMode; label: string }[] = [
  { id: "auto", label: "Auto" },
  { id: "pro", label: "Pro" },
  { id: "hub", label: "Pro hub" },
  { id: "gc-adapter", label: "GC adapter" },
];

function Home() {
  const wb = useWorkbench();
  const faces = ["A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Plus", "Minus", "Home"] as const;
  const sel = wb.slots[wb.selected];
  const outKind: ControllerKind = wb.resolvedOut === "gc-adapter" ? "s2-gc" : "s2-pro";

  return (
    <Shell>
      <div className="flex flex-col gap-8">
        <header className="flex flex-col gap-4 sm:flex-row sm:items-end sm:justify-between">
          <div className="max-w-xl">
            <p className="text-xs font-medium tracking-[0.18em] text-fg-muted uppercase">Wired adapter</p>
            <h1 className="mt-2 text-3xl font-semibold tracking-tight sm:text-4xl">
              Any pad. One cable. Switch 1.
            </h1>
            <p className="mt-3 max-w-prose text-sm leading-relaxed text-fg-muted">
              Switch 2, Switch 1, Xbox, and PlayStation controllers wake over USB, remap in well under 10 ms,
              and leave the Pi as a Pro Controller hub or the official GameCube adapter — up to eight pads on
              the same cable (8× Pro HID with HD rumble when the UDC allows, or 2× WUP-028 with rumble on all
              ports). Event-driven 8 ms keepalives keep latency low.
            </p>
          </div>
          <div className="flex flex-wrap gap-2">
            <Badge tone="ok">{wb.slots.length} pads</Badge>
            <Badge tone="wire">{wb.outLabel}</Badge>
            <Badge tone={wb.underBudget ? "ok" : "bad"}>
              {wb.ms < 0.01 ? "<0.01" : wb.ms.toFixed(2)} ms
            </Badge>
          </div>
        </header>

        <section className="rounded-xl border border-border bg-surface p-4 sm:p-6">
          <div className="flex flex-col gap-4">
            <div className="flex flex-wrap items-center justify-between gap-3">
              <div className="flex flex-wrap gap-2">
                {OUTS.map((o) => (
                  <Button
                    key={o.id}
                    size="sm"
                    variant={wb.outMode === o.id ? "default" : "secondary"}
                    onClick={() => wb.setOutMode(o.id)}
                  >
                    {o.label}
                  </Button>
                ))}
              </div>
              <div className="flex items-center gap-2">
                <span className="text-xs text-fg-muted">Links</span>
                <Button size="icon" variant="secondary" className="size-9"
                  onClick={() => wb.setLinks(Math.max(1, wb.links - 1))} aria-label="Fewer links">
                  <Minus />
                </Button>
                <span className="w-4 text-center font-mono text-sm">{wb.links}</span>
                <Button size="icon" variant="secondary" className="size-9"
                  onClick={() => wb.setLinks(Math.min(4, wb.links + 1))} aria-label="More links">
                  <Plus />
                </Button>
                <label className="ml-2 flex min-h-11 items-center gap-2 text-sm text-fg-muted">
                  <input type="checkbox" checked={wb.invertY}
                    onChange={(e) => wb.setInvertY(e.target.checked)} className="size-4 accent-accent" />
                  Invert S2 Y
                </label>
              </div>
            </div>

            <AssignmentBoard wb={wb} />

            {wb.slots.length === 0 ? (
              <p className="rounded-lg border border-dashed border-border bg-bg/40 px-4 py-8 text-center text-sm text-fg-muted">
                Connect a controller over USB. Live pads appear here — no simulated data.
              </p>
            ) : (
              <div className="grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
                {wb.slots.map((slot, i) => {
                  const asg = wb.assigns.find((a) => a.slot === i);
                  return (
                    <button key={slot.id} type="button" onClick={() => wb.setSelected(i)}
                      className={cn(
                        "rounded-lg border p-2 text-left transition-colors duration-150",
                        i === wb.selected ? "border-accent bg-bg" : "border-border bg-bg/40 hover:border-border-strong",
                      )}>
                      <div className="mb-1 flex items-center justify-between gap-2 px-1">
                        <span className="truncate text-[11px] font-medium">{kindName(slot.kind)}</span>
                        <Badge tone="ok" className="shrink-0">
                          {asg ? `L${asg.link + 1}·P${asg.port + 1}` : "—"}
                        </Badge>
                      </div>
                      <PadFace pad={slot.pad} kind={slot.kind} title="" />
                    </button>
                  );
                })}
              </div>
            )}
          </div>
        </section>

        {sel ? (
          <section className="rounded-xl border border-border bg-surface p-4 sm:p-6">
            <div className="mb-3 flex items-center justify-between gap-2">
              <h2 className="text-sm font-medium">Selected · {kindName(sel.kind)}</h2>
              <span className="font-mono text-[11px] text-fg-subtle">
                {wb.resolvedOut === "gc-adapter" ? "057e:0337 WUP-028" : "057e:2009 Pro Controller"}
              </span>
            </div>
            <div className="grid items-center gap-2 lg:grid-cols-[1fr_auto_1fr]">
              <PadFace pad={sel.pad} kind={sel.kind} title="USB host in" />
              <div className="flex flex-col items-center justify-center py-2">
                <Cable className="size-5 text-wire" strokeWidth={1.75} />
                <span className="mt-1 font-mono text-[10px] tracking-widest text-wire">USB</span>
              </div>
              <PadFace pad={sel.translated} kind={outKind}
                title={wb.resolvedOut === "gc-adapter" ? "GC adapter port" : "Switch 1 Pro out"} />
            </div>
            <div className="mt-4 flex flex-wrap gap-1.5">
              {faces.map((name) => (
                <span key={name} className={cn(
                  "rounded-sm border px-2 py-1 font-mono text-[11px]",
                  isPressed(sel.translated, name)
                    ? "border-accent/40 bg-accent text-accent-foreground"
                    : "border-border text-fg-muted",
                )}>{name}</span>
              ))}
            </div>
          </section>
        ) : null}

        <ReportStrip report={wb.activeReport}
          label={wb.resolvedOut === "gc-adapter"
            ? "GameCube Adapter HID 0x21 (37 bytes · 4 ports; dual HID = 8)"
            : "Switch 1 HID 0x30 input report"} />
      </div>
    </Shell>
  );
}

function AssignmentBoard({ wb }: { wb: ReturnType<typeof useWorkbench> }) {
  const groups = Array.from({ length: wb.links }, (_, link) =>
    wb.assigns.filter((a) => a.link === link),
  );
  return (
    <div className="grid gap-2 sm:grid-cols-2">
      {groups.map((g, link) => (
        <div key={link} className="rounded-md border border-border bg-bg px-3 py-2">
          <p className="text-[10px] tracking-wide text-fg-subtle uppercase">
            Link {link + 1}{link === 0 ? " · Pi USB-C" : " · extra gadget"}{" · "}{wb.outLabel}
          </p>
          <ul className="mt-1 flex flex-col gap-0.5">
            {g.length === 0 ? (
              <li className="text-xs text-fg-muted">Empty</li>
            ) : (
              g.map((a) => (
                <li key={a.slot} className="flex items-center justify-between gap-2 font-mono text-[11px]">
                  <span className="truncate text-fg">P{a.port + 1} {kindName(wb.slots[a.slot]?.kind ?? "unknown")}</span>
                  <span className="text-fg-subtle">slot {a.slot}</span>
                </li>
              ))
            )}
          </ul>
        </div>
      ))}
    </div>
  );
}
