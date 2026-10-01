import { cn } from "@/lib/utils";
import { isPressed, kindFamily, type ControllerKind, type PadState } from "@/lib/bridge";

function Lit({
  cx,
  cy,
  r = 8,
  on,
  label,
}: {
  cx: number;
  cy: number;
  r?: number;
  on: boolean;
  label?: string;
}) {
  return (
    <g>
      <circle
        cx={cx}
        cy={cy}
        r={r}
        className={cn(on ? "fill-accent" : "fill-surface-2")}
        stroke="currentColor"
        strokeOpacity={0.22}
        strokeWidth="1"
      />
      {label ? (
        <text
          x={cx}
          y={cy + 1}
          textAnchor="middle"
          dominantBaseline="middle"
          className={cn("text-[8px]", on ? "fill-accent-foreground" : "fill-fg-muted")}
          style={{ fontSize: 8, fontWeight: 500 }}
        >
          {label}
        </text>
      ) : null}
    </g>
  );
}

function Shoulder({
  x,
  y,
  w,
  h,
  on,
  label,
}: {
  x: number;
  y: number;
  w: number;
  h: number;
  on: boolean;
  label: string;
}) {
  return (
    <g>
      <rect
        x={x}
        y={y}
        width={w}
        height={h}
        rx={6}
        className={cn(on ? "fill-accent" : "fill-surface-2")}
        stroke="currentColor"
        strokeOpacity={0.22}
      />
      <text
        x={x + w / 2}
        y={y + h / 2 + 1}
        textAnchor="middle"
        dominantBaseline="middle"
        className={on ? "fill-accent-foreground" : "fill-fg-muted"}
        style={{ fontSize: 8, fontWeight: 500 }}
      >
        {label}
      </text>
    </g>
  );
}

function Stick({ cx, cy, x, y }: { cx: number; cy: number; x: number; y: number }) {
  const dx = ((x - 0x800) / 0x800) * 10;
  const dy = ((y - 0x800) / 0x800) * 10;
  return (
    <g>
      <circle cx={cx} cy={cy} r={18} className="fill-surface-2" stroke="currentColor" strokeOpacity={0.2} />
      <circle cx={cx + dx} cy={cy + dy} r={7} className="fill-fg-muted" />
    </g>
  );
}

export function ProPad({ pad, title }: { pad: PadState; title: string }) {
  const p = (n: Parameters<typeof isPressed>[1]) => isPressed(pad, n);
  return (
    <figure className="w-full">
      <svg viewBox="0 0 360 200" className="w-full text-fg" role="img" aria-label={title}>
        <rect x="28" y="48" width="304" height="128" rx="42" className="fill-surface" stroke="currentColor" strokeOpacity={0.16} />
        <Shoulder x={48} y={22} w={52} h={18} on={p("ZL")} label="ZL" />
        <Shoulder x={48} y={38} w={52} h={14} on={p("L")} label="L" />
        <Shoulder x={260} y={22} w={52} h={18} on={p("ZR")} label="ZR" />
        <Shoulder x={260} y={38} w={52} h={14} on={p("R")} label="R" />
        <Stick cx={96} cy={118} x={pad.lx} y={pad.ly} />
        <Stick cx={210} cy={132} x={pad.rx} y={pad.ry} />
        <Lit cx={96} cy={72} r={6} on={p("Up")} />
        <Lit cx={96} cy={96} r={6} on={p("Down")} />
        <Lit cx={84} cy={84} r={6} on={p("Left")} />
        <Lit cx={108} cy={84} r={6} on={p("Right")} />
        <Lit cx={268} cy={88} r={9} on={p("X")} label="X" />
        <Lit cx={250} cy={106} r={9} on={p("Y")} label="Y" />
        <Lit cx={286} cy={106} r={9} on={p("A")} label="A" />
        <Lit cx={268} cy={124} r={9} on={p("B")} label="B" />
        <Lit cx={148} cy={78} r={6} on={p("Minus")} label="−" />
        <Lit cx={200} cy={78} r={6} on={p("Plus")} label="+" />
        <Lit cx={164} cy={100} r={7} on={p("Capture")} />
        <Lit cx={184} cy={100} r={7} on={p("Home")} />
        <Lit cx={148} cy={148} r={5} on={p("LStick")} />
        <Lit cx={210} cy={160} r={5} on={p("RStick")} />
      </svg>
      {title ? (
        <figcaption className="mt-1 text-center text-xs font-medium tracking-wide text-fg-muted">{title}</figcaption>
      ) : null}
    </figure>
  );
}

export function GcPad({ pad, title }: { pad: PadState; title: string }) {
  const p = (n: Parameters<typeof isPressed>[1]) => isPressed(pad, n);
  const lt = pad.lt / 255;
  const rt = pad.rt / 255;
  return (
    <figure className="w-full">
      <svg viewBox="0 0 360 200" className="w-full text-fg" role="img" aria-label={title}>
        <rect x="40" y="40" width="280" height="132" rx="36" className="fill-surface" stroke="currentColor" strokeOpacity={0.16} />
        <rect x="52" y="18" width="18" height={28 + lt * 16} rx="4" className={cn(lt > 0.15 ? "fill-accent" : "fill-surface-2")} />
        <rect x="290" y="18" width="18" height={28 + rt * 16} rx="4" className={cn(rt > 0.15 ? "fill-accent" : "fill-surface-2")} />
        <Stick cx={108} cy={96} x={pad.lx} y={pad.ly} />
        <Stick cx={232} cy={128} x={pad.rx} y={pad.ry} />
        <Lit cx={88} cy={148} r={5} on={p("Up")} />
        <Lit cx={88} cy={168} r={5} on={p("Down")} />
        <Lit cx={76} cy={158} r={5} on={p("Left")} />
        <Lit cx={100} cy={158} r={5} on={p("Right")} />
        <Lit cx={268} cy={88} r={12} on={p("A")} label="A" />
        <Lit cx={246} cy={104} r={8} on={p("B")} label="B" />
        <Lit cx={288} cy={104} r={8} on={p("X")} label="X" />
        <Lit cx={268} cy={122} r={8} on={p("Y")} label="Y" />
        <Shoulder x={250} y={48} w={40} h={16} on={p("ZR")} label="Z" />
        <Lit cx={180} cy={72} r={7} on={p("Plus")} label="S" />
      </svg>
      {title ? (
        <figcaption className="mt-1 text-center text-xs font-medium tracking-wide text-fg-muted">{title}</figcaption>
      ) : null}
    </figure>
  );
}

export function XboxPad({ pad, title }: { pad: PadState; title: string }) {
  const p = (n: Parameters<typeof isPressed>[1]) => isPressed(pad, n);
  return (
    <figure className="w-full">
      <svg viewBox="0 0 360 200" className="w-full text-fg" role="img" aria-label={title}>
        <rect x="32" y="46" width="296" height="128" rx="48" className="fill-surface" stroke="currentColor" strokeOpacity={0.16} />
        <Shoulder x={50} y={22} w={50} h={16} on={p("ZL")} label="LT" />
        <Shoulder x={50} y={36} w={50} h={14} on={p("L")} label="LB" />
        <Shoulder x={260} y={22} w={50} h={16} on={p("ZR")} label="RT" />
        <Shoulder x={260} y={36} w={50} h={14} on={p("R")} label="RB" />
        <Stick cx={108} cy={124} x={pad.lx} y={pad.ly} />
        <Stick cx={252} cy={124} x={pad.rx} y={pad.ry} />
        <Lit cx={108} cy={72} r={5} on={p("Up")} />
        <Lit cx={108} cy={92} r={5} on={p("Down")} />
        <Lit cx={96} cy={82} r={5} on={p("Left")} />
        <Lit cx={120} cy={82} r={5} on={p("Right")} />
        {/* Xbox diamond: A south, B east, X west, Y north — shown as Nintendo targets */}
        <Lit cx={260} cy={88} r={9} on={p("X")} label="Y" />
        <Lit cx={242} cy={106} r={9} on={p("Y")} label="X" />
        <Lit cx={278} cy={106} r={9} on={p("A")} label="B" />
        <Lit cx={260} cy={124} r={9} on={p("B")} label="A" />
        <Lit cx={156} cy={78} r={6} on={p("Minus")} label="☰" />
        <Lit cx={204} cy={78} r={6} on={p("Plus")} label="≡" />
        <Lit cx={180} cy={100} r={8} on={p("Home")} />
      </svg>
      {title ? (
        <figcaption className="mt-1 text-center text-xs font-medium tracking-wide text-fg-muted">{title}</figcaption>
      ) : null}
    </figure>
  );
}

export function PsPad({ pad, title }: { pad: PadState; title: string }) {
  const p = (n: Parameters<typeof isPressed>[1]) => isPressed(pad, n);
  return (
    <figure className="w-full">
      <svg viewBox="0 0 360 200" className="w-full text-fg" role="img" aria-label={title}>
        <rect x="28" y="50" width="304" height="122" rx="36" className="fill-surface" stroke="currentColor" strokeOpacity={0.16} />
        <Shoulder x={48} y={24} w={52} h={16} on={p("ZL")} label="L2" />
        <Shoulder x={48} y={38} w={52} h={14} on={p("L")} label="L1" />
        <Shoulder x={260} y={24} w={52} h={16} on={p("ZR")} label="R2" />
        <Shoulder x={260} y={38} w={52} h={14} on={p("R")} label="R1" />
        <Stick cx={118} cy={128} x={pad.lx} y={pad.ly} />
        <Stick cx={242} cy={128} x={pad.rx} y={pad.ry} />
        <Lit cx={90} cy={78} r={5} on={p("Up")} />
        <Lit cx={90} cy={102} r={5} on={p("Down")} />
        <Lit cx={78} cy={90} r={5} on={p("Left")} />
        <Lit cx={102} cy={90} r={5} on={p("Right")} />
        <Lit cx={270} cy={78} r={9} on={p("X")} label="△" />
        <Lit cx={252} cy={96} r={9} on={p("Y")} label="□" />
        <Lit cx={288} cy={96} r={9} on={p("A")} label="○" />
        <Lit cx={270} cy={114} r={9} on={p("B")} label="×" />
        <Lit cx={150} cy={72} r={6} on={p("Minus")} />
        <Lit cx={210} cy={72} r={6} on={p("Plus")} />
        <Lit cx={180} cy={88} r={7} on={p("Home")} />
      </svg>
      {title ? (
        <figcaption className="mt-1 text-center text-xs font-medium tracking-wide text-fg-muted">{title}</figcaption>
      ) : null}
    </figure>
  );
}

export function PadFace({
  pad,
  title,
  kind,
}: {
  pad: PadState;
  title: string;
  kind: ControllerKind;
}) {
  const fam = kindFamily(kind);
  if (kind === "s2-gc" || kind === "gc-adapter") return <GcPad pad={pad} title={title} />;
  if (fam === "xbox") return <XboxPad pad={pad} title={title} />;
  if (fam === "playstation") return <PsPad pad={pad} title={title} />;
  return <ProPad pad={pad} title={title} />;
}
