import type { ControllerKind, OutMode } from "./ids";

export type PadState = {
  lx: number;
  ly: number;
  rx: number;
  ry: number;
  lt: number;
  rt: number;
  buttons: [number, number, number];
  extra: number;
  ax: number;
  ay: number;
  az: number;
  gx: number;
  gy: number;
  gz: number;
  reportId: number;
  kind: ControllerKind;
  counter: number;
  slot: number;
  valid: boolean;
  label?: string;
};

export type BridgeConfig = {
  mac: number[];
  playerLed: number;
  invertY: boolean;
  gcAnalogShoulders: boolean;
  gcTriggerThreshold: number;
  outMode: OutMode;
  links: number;
};

export type S1State =
  | "idle"
  | "attached"
  | "conn-status"
  | "handshake"
  | "baud"
  | "usb-only"
  | "subcommands"
  | "streaming"
  | "error";

export type S2State =
  | "idle"
  | "enumerated"
  | "wake-usb"
  | "enable-hid"
  | "select-report"
  | "streaming"
  | "error";

export type ButtonName =
  | "A"
  | "B"
  | "X"
  | "Y"
  | "L"
  | "R"
  | "ZL"
  | "ZR"
  | "Minus"
  | "Plus"
  | "LStick"
  | "RStick"
  | "Home"
  | "Capture"
  | "Up"
  | "Down"
  | "Left"
  | "Right"
  | "GL"
  | "GR"
  | "C"
  | "Z";

export const S1_BUTTON_BITS: Record<Exclude<ButtonName, "GL" | "GR" | "C" | "Z">, { byte: 0 | 1 | 2; bit: number }> = {
  Y: { byte: 0, bit: 0x01 },
  X: { byte: 0, bit: 0x02 },
  B: { byte: 0, bit: 0x04 },
  A: { byte: 0, bit: 0x08 },
  R: { byte: 0, bit: 0x40 },
  ZR: { byte: 0, bit: 0x80 },
  Minus: { byte: 1, bit: 0x01 },
  Plus: { byte: 1, bit: 0x02 },
  RStick: { byte: 1, bit: 0x04 },
  LStick: { byte: 1, bit: 0x08 },
  Home: { byte: 1, bit: 0x10 },
  Capture: { byte: 1, bit: 0x20 },
  Down: { byte: 2, bit: 0x01 },
  Up: { byte: 2, bit: 0x02 },
  Right: { byte: 2, bit: 0x04 },
  Left: { byte: 2, bit: 0x08 },
  L: { byte: 2, bit: 0x40 },
  ZL: { byte: 2, bit: 0x80 },
};

export function emptyPad(kind: ControllerKind = "s2-pro"): PadState {
  return {
    lx: 0x800,
    ly: 0x800,
    rx: 0x800,
    ry: 0x800,
    lt: 0,
    rt: 0,
    buttons: [0, 0, 0],
    extra: 0,
    ax: 0,
    ay: 0,
    az: 0,
    gx: 0,
    gy: 0,
    gz: 0,
    reportId: 0,
    kind,
    counter: 0,
    slot: 0,
    valid: false,
  };
}

export function defaultConfig(): BridgeConfig {
  return {
    mac: [0x98, 0xb6, 0xe9, 0x12, 0x34, 0x56],
    playerLed: 0x01,
    invertY: true,
    gcAnalogShoulders: true,
    gcTriggerThreshold: 40,
    outMode: "auto",
    links: 1,
  };
}

export function isPressed(pad: PadState, name: Exclude<ButtonName, "GL" | "GR" | "C" | "Z">): boolean {
  const m = S1_BUTTON_BITS[name];
  return (pad.buttons[m.byte] & m.bit) !== 0;
}

export function setPressed(
  pad: PadState,
  name: Exclude<ButtonName, "GL" | "GR" | "C" | "Z">,
  down: boolean,
): PadState {
  const m = S1_BUTTON_BITS[name];
  const buttons: [number, number, number] = [...pad.buttons];
  if (down) buttons[m.byte] |= m.bit;
  else buttons[m.byte] &= ~m.bit;
  return { ...pad, buttons, valid: true };
}
