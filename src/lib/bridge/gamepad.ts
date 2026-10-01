import type { ControllerKind } from "./ids";
import { kindFromGamepadId } from "./ids";
import { emptyPad, setPressed, type PadState } from "./types";

function axis12(v: number): number {
  const n = Math.max(-1, Math.min(1, v));
  return Math.round((n + 1) * 0.5 * 0xfff);
}

export function fromBrowserGamepad(gp: Gamepad, kind?: ControllerKind): PadState {
  const resolved = kind ?? kindFromGamepadId(gp.id);
  let pad = emptyPad(resolved);
  pad.valid = true;
  pad.label = gp.id;
  const b = (i: number) => Boolean(gp.buttons[i]?.pressed);
  const a = (i: number) => gp.axes[i] ?? 0;

  /* Standard Gamepad is position-based: 0 south, 1 east, 2 west, 3 north. */
  pad = setPressed(pad, "B", b(0));
  pad = setPressed(pad, "A", b(1));
  pad = setPressed(pad, "Y", b(2));
  pad = setPressed(pad, "X", b(3));
  pad = setPressed(pad, "L", b(4));
  pad = setPressed(pad, "R", b(5));
  pad = setPressed(pad, "ZL", b(6) || (gp.buttons[6]?.value ?? 0) > 0.35);
  pad = setPressed(pad, "ZR", b(7) || (gp.buttons[7]?.value ?? 0) > 0.35);
  pad = setPressed(pad, "Minus", b(8));
  pad = setPressed(pad, "Plus", b(9));
  pad = setPressed(pad, "LStick", b(10));
  pad = setPressed(pad, "RStick", b(11));
  pad = setPressed(pad, "Home", b(16));
  pad = setPressed(pad, "Capture", b(17));

  pad = setPressed(pad, "Up", b(12) || a(7) < -0.5);
  pad = setPressed(pad, "Down", b(13) || a(7) > 0.5);
  pad = setPressed(pad, "Left", b(14) || a(6) < -0.5);
  pad = setPressed(pad, "Right", b(15) || a(6) > 0.5);

  pad.lx = axis12(a(0));
  pad.ly = axis12(a(1));
  pad.rx = axis12(a(2));
  pad.ry = axis12(a(3));
  pad.lt = Math.round((gp.buttons[6]?.value ?? 0) * 255);
  pad.rt = Math.round((gp.buttons[7]?.value ?? 0) * 255);
  return pad;
}

export const KEY_MAP: Record<string, Parameters<typeof setPressed>[1]> = {
  KeyK: "A",
  KeyJ: "B",
  KeyI: "X",
  KeyU: "Y",
  KeyQ: "L",
  KeyE: "R",
  KeyZ: "ZL",
  KeyC: "ZR",
  Enter: "Plus",
  Backspace: "Minus",
  Digit8: "Home",
  Digit9: "Capture",
  ArrowUp: "Up",
  ArrowDown: "Down",
  ArrowLeft: "Left",
  ArrowRight: "Right",
  ShiftLeft: "LStick",
  ShiftRight: "RStick",
};

export function applyKeyboard(base: PadState, down: Set<string>): PadState {
  let pad = { ...base, buttons: [...base.buttons] as [number, number, number], valid: true };
  for (const [code, btn] of Object.entries(KEY_MAP)) {
    pad = setPressed(pad, btn, down.has(code));
  }
  const lx = (down.has("KeyD") ? 1 : 0) + (down.has("KeyA") ? -1 : 0);
  const ly = (down.has("KeyS") ? 1 : 0) + (down.has("KeyW") ? -1 : 0);
  const rx = (down.has("KeyL") ? 1 : 0) + (down.has("KeyO") ? -1 : 0);
  const ry = (down.has("KeyPeriod") ? 1 : 0) + (down.has("KeyP") ? -1 : 0);
  if (lx || ly) {
    pad.lx = axis12(lx);
    pad.ly = axis12(ly);
  }
  if (rx || ry) {
    pad.rx = axis12(rx);
    pad.ry = axis12(ry);
  }
  return pad;
}

export const DEMO_KINDS: ControllerKind[] = [
  "s2-pro",
  "xbox-series",
  "dualsense",
  "s1-pro",
  "s2-gc",
  "ds4",
  "xbox360",
  "s1-jc-l",
];
