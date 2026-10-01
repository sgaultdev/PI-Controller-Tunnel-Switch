import { useEffect, useMemo, useRef, useState } from "react";
import {
  applyKeyboard,
  defaultConfig,
  encodeGcAdapter,
  encodeS1Input,
  fromBrowserGamepad,
  hexBytes,
  outModeName,
  outResolve,
  planAssigns,
  translate,
  type OutMode,
  type PadState,
} from "@/lib/bridge";
import { GC_PORTS } from "@/lib/bridge/gc-adapter";

export type WorkbenchSlot = {
  id: string;
  kind: PadState["kind"];
  source: "gamepad";
  pad: PadState;
  translated: PadState;
  report: Uint8Array;
};

export function useWorkbench() {
  const [selected, setSelected] = useState(0);
  const [invertY, setInvertY] = useState(true);
  const [outMode, setOutMode] = useState<OutMode>("auto");
  const [links, setLinks] = useState(1);
  const [pads, setPads] = useState<PadState[]>([]);
  const [ms, setMs] = useState(0);
  const [gamepadCount, setGamepadCount] = useState(0);
  const keys = useRef(new Set<string>());
  const selRef = useRef(selected);
  selRef.current = selected;

  const cfg = useMemo(
    () => ({ ...defaultConfig(), invertY, gcAnalogShoulders: true, gcTriggerThreshold: 40, outMode, links }),
    [invertY, outMode, links],
  );

  useEffect(() => {
    const down = (e: KeyboardEvent) => {
      if (e.metaKey || e.ctrlKey) return;
      const typing = e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement;
      if (typing) return;
      keys.current.add(e.code);
      if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space"].includes(e.code)) e.preventDefault();
    };
    const up = (e: KeyboardEvent) => {
      keys.current.delete(e.code);
    };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
    };
  }, []);

  useEffect(() => {
    let raf = 0;
    let tickN = 0;
    const tick = () => {
      const t0 = performance.now();
      const gps = (navigator.getGamepads?.() ?? []).filter((g): g is Gamepad => Boolean(g && g.connected));
      const next: PadState[] = [];
      gps.slice(0, 8).forEach((gp, i) => {
        let p = fromBrowserGamepad(gp);
        p.slot = i;
        p.counter = tickN;
        p.valid = true;
        if (i === selRef.current && keys.current.size) {
          p = applyKeyboard(p, keys.current);
          p.slot = i;
          p.counter = tickN;
          p.valid = true;
        }
        next.push(p);
      });
      setGamepadCount(next.length);
      setPads(next);
      setMs(performance.now() - t0);
      tickN++;
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, []);

  const slots: WorkbenchSlot[] = useMemo(
    () =>
      pads.map((pad, i) => {
        const translated = translate(pad, cfg);
        return {
          id: `${pad.kind}-${i}`,
          kind: pad.kind,
          source: "gamepad" as const,
          pad,
          translated,
          report: encodeS1Input(translated, pad.counter & 0xff),
        };
      }),
    [pads, cfg],
  );

  const resolvedOut = outResolve(outMode, slots.length);
  const assigns = useMemo(() => planAssigns(slots.length, links, outMode), [slots.length, links, outMode]);

  const gcReport = useMemo(() => {
    const ports: Array<PadState | null> = Array.from({ length: GC_PORTS }, () => null);
    assigns
      .filter((a) => a.link === 0)
      .forEach((a) => {
        if (a.port < GC_PORTS) ports[a.port] = slots[a.slot]?.translated ?? null;
      });
    return encodeGcAdapter(ports);
  }, [assigns, slots]);

  const sel = Math.min(selected, Math.max(0, slots.length - 1));
  const selectedSlot = slots[sel] ?? slots[0];
  const activeReport = resolvedOut === "gc-adapter" ? gcReport : (selectedSlot?.report ?? new Uint8Array(64));

  return {
    slots,
    selected: sel,
    setSelected,
    invertY,
    setInvertY,
    outMode,
    setOutMode,
    resolvedOut,
    links,
    setLinks,
    assigns,
    gcReport,
    activeReport,
    hex: hexBytes(activeReport, resolvedOut === "gc-adapter" ? 37 : 32),
    ms,
    underBudget: ms < 10,
    gamepadCount,
    outLabel: outModeName(resolvedOut),
    cfg,
  };
}
