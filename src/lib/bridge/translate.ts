import { kindFamily } from "./ids";
import { clamp12, invertY, pack12 } from "./pack";
import type { BridgeConfig, PadState } from "./types";

export function translate(input: PadState, cfg: BridgeConfig): PadState {
  const fam = kindFamily(input.kind);
  const inv = cfg.invertY && fam === "switch-2";
  const out: PadState = {
    ...input,
    buttons: [...input.buttons],
    ly: invertY(input.ly, inv),
    ry: invertY(input.ry, inv),
  };
  if ((input.kind === "s2-gc" || input.kind === "gc-adapter") && cfg.gcAnalogShoulders) {
    if (input.lt > cfg.gcTriggerThreshold) out.buttons[2] |= 0x80;
    if (input.rt > cfg.gcTriggerThreshold) out.buttons[0] |= 0x80;
    if (input.lt > 180) out.buttons[2] |= 0x40;
    if (input.rt > 180) out.buttons[0] |= 0x40;
  }
  out.lx = clamp12(out.lx);
  out.ly = clamp12(out.ly);
  out.rx = clamp12(out.rx);
  out.ry = clamp12(out.ry);
  return out;
}

export function encodeS1Input(pad: PadState, timer: number): Uint8Array {
  const out = new Uint8Array(64);
  out[0] = 0x30;
  out[1] = timer & 0xff;
  out[2] = 0x91;
  out[3] = pad.buttons[0];
  out[4] = pad.buttons[1] | 0x80;
  out[5] = pad.buttons[2];
  const l = pack12(pad.lx, pad.ly);
  const r = pack12(pad.rx, pad.ry);
  out[6] = l[0];
  out[7] = l[1];
  out[8] = l[2];
  out[9] = r[0];
  out[10] = r[1];
  out[11] = r[2];
  const imu = new Uint8Array(12);
  const view = new DataView(imu.buffer);
  view.setInt16(0, pad.ax, true);
  view.setInt16(2, pad.ay, true);
  view.setInt16(4, pad.az, true);
  view.setInt16(6, pad.gx, true);
  view.setInt16(8, pad.gy, true);
  view.setInt16(10, pad.gz, true);
  for (let s = 0; s < 3; s++) out.set(imu, 13 + s * 12);
  return out;
}

export type TranslateTiming = {
  ns: number;
  ms: number;
  underBudget: boolean;
};

export function timedTranslate(input: PadState, cfg: BridgeConfig): { pad: PadState; report: Uint8Array; timing: TranslateTiming } {
  const t0 = performance.now();
  const pad = translate(input, cfg);
  const report = encodeS1Input(pad, 0);
  const t1 = performance.now();
  const ms = t1 - t0;
  return {
    pad,
    report,
    timing: {
      ns: Math.round(ms * 1e6),
      ms,
      underBudget: ms < 10,
    },
  };
}
