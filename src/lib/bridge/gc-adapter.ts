import { axis12ToU8, axisU8To12 } from "./pack";
import { emptyPad, type PadState } from "./types";

export const GC_REPORT_LEN = 37;
export const GC_PORTS = 4;

export function encodeGcPort(pad: PadState | null): Uint8Array {
  const out = new Uint8Array(9);
  if (!pad?.valid) return out;
  out[0] = 0x14;
  let b1 = 0;
  let b2 = 0;
  if (pad.buttons[0] & 0x08) b1 |= 0x01;
  if (pad.buttons[0] & 0x04) b1 |= 0x02;
  if (pad.buttons[0] & 0x02) b1 |= 0x04;
  if (pad.buttons[0] & 0x01) b1 |= 0x08;
  if (pad.buttons[2] & 0x08) b1 |= 0x10;
  if (pad.buttons[2] & 0x04) b1 |= 0x20;
  if (pad.buttons[2] & 0x01) b1 |= 0x40;
  if (pad.buttons[2] & 0x02) b1 |= 0x80;
  if (pad.buttons[1] & 0x02) b2 |= 0x01;
  if (pad.buttons[0] & 0x80) b2 |= 0x02;
  if ((pad.buttons[0] & 0x40) || pad.rt > 180) b2 |= 0x04;
  if ((pad.buttons[2] & 0x40) || pad.lt > 180) b2 |= 0x08;
  out[1] = b1;
  out[2] = b2;
  out[3] = axis12ToU8(pad.lx);
  out[4] = axis12ToU8(pad.ly);
  out[5] = axis12ToU8(pad.rx);
  out[6] = axis12ToU8(pad.ry);
  out[7] = pad.lt & 0xff;
  out[8] = pad.rt & 0xff;
  return out;
}

export function encodeGcAdapter(pads: Array<PadState | null>): Uint8Array {
  const out = new Uint8Array(GC_REPORT_LEN);
  out[0] = 0x21;
  for (let i = 0; i < GC_PORTS; i++) {
    out.set(encodeGcPort(pads[i] ?? null), 1 + i * 9);
  }
  return out;
}

function parsePort(p: Uint8Array): PadState {
  const out = emptyPad("s2-gc");
  if (!(p[0] & 0x10) && p[0] !== 0x14 && p[0] !== 0x22) {
    out.valid = false;
    return out;
  }
  const b1 = p[1] ?? 0;
  const b2 = p[2] ?? 0;
  let s0 = 0;
  let s1 = 0;
  let s2 = 0;
  if (b1 & 0x01) s0 |= 0x08;
  if (b1 & 0x02) s0 |= 0x04;
  if (b1 & 0x04) s0 |= 0x02;
  if (b1 & 0x08) s0 |= 0x01;
  if (b1 & 0x10) s2 |= 0x08;
  if (b1 & 0x20) s2 |= 0x04;
  if (b1 & 0x40) s2 |= 0x01;
  if (b1 & 0x80) s2 |= 0x02;
  if (b2 & 0x01) s1 |= 0x02;
  if (b2 & 0x02) s0 |= 0x80;
  if (b2 & 0x04) s0 |= 0x40;
  if (b2 & 0x08) s2 |= 0x40;
  out.buttons = [s0, s1, s2];
  out.lx = axisU8To12(p[3] ?? 128);
  out.ly = axisU8To12(p[4] ?? 128);
  out.rx = axisU8To12(p[5] ?? 128);
  out.ry = axisU8To12(p[6] ?? 128);
  out.lt = p[7] ?? 0;
  out.rt = p[8] ?? 0;
  if (out.lt > 40) out.buttons[2] |= 0x80;
  if (out.rt > 40) out.buttons[0] |= 0x80;
  out.valid = true;
  return out;
}

export function parseGcAdapter(buf: Uint8Array | number[]): PadState[] {
  if (buf.length < GC_REPORT_LEN || (buf[0] ?? 0) !== 0x21) return [];
  const bytes = buf instanceof Uint8Array ? buf : Uint8Array.from(buf);
  const pads: PadState[] = [];
  for (let i = 0; i < GC_PORTS; i++) {
    const pad = parsePort(bytes.slice(1 + i * 9, 1 + (i + 1) * 9));
    pad.slot = i;
    pads.push(pad);
  }
  return pads;
}
