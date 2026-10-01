import type { ControllerKind } from "./ids";
import { kindFamily } from "./ids";
import { axisS16To12, axisU8To12, unpack12 } from "./pack";
import { emptyPad, type PadState } from "./types";

function parseButtons05(b: Uint8Array | number[]): [number, number, number, number] {
  return [b[0] ?? 0, (b[1] ?? 0) & 0x3f, b[2] ?? 0, b[3] ?? 0];
}

function parseButtons09(b: Uint8Array | number[]): [number, number, number, number] {
  const b0 = b[0] ?? 0;
  const b1 = b[1] ?? 0;
  const b2 = b[2] ?? 0;
  let s0 = 0;
  let s1 = 0;
  let s2 = 0;
  if (b0 & 0x01) s0 |= 0x04;
  if (b0 & 0x02) s0 |= 0x08;
  if (b0 & 0x04) s0 |= 0x01;
  if (b0 & 0x08) s0 |= 0x02;
  if (b0 & 0x10) s0 |= 0x40;
  if (b0 & 0x20) s0 |= 0x80;
  if (b0 & 0x40) s1 |= 0x02;
  if (b0 & 0x80) s1 |= 0x04;
  if (b1 & 0x01) s2 |= 0x01;
  if (b1 & 0x02) s2 |= 0x04;
  if (b1 & 0x04) s2 |= 0x08;
  if (b1 & 0x08) s2 |= 0x02;
  if (b1 & 0x10) s2 |= 0x40;
  if (b1 & 0x20) s2 |= 0x80;
  if (b1 & 0x40) s1 |= 0x01;
  if (b1 & 0x80) s1 |= 0x08;
  if (b2 & 0x01) s1 |= 0x10;
  if (b2 & 0x02) s1 |= 0x20;
  return [s0, s1, s2, b2];
}

function parseButtons0a(b: Uint8Array | number[]): [number, number, number, number] {
  const b0 = b[0] ?? 0;
  const b1 = b[1] ?? 0;
  const b2 = b[2] ?? 0;
  let s0 = 0;
  let s1 = 0;
  let s2 = 0;
  if (b0 & 0x01) s0 |= 0x04;
  if (b0 & 0x02) s0 |= 0x08;
  if (b0 & 0x04) s0 |= 0x01;
  if (b0 & 0x08) s0 |= 0x02;
  if (b0 & 0x10) s0 |= 0x80;
  if (b0 & 0x20) s0 |= 0x40;
  if (b0 & 0x40) s1 |= 0x02;
  if (b0 & 0x80) s1 |= 0x04;
  if (b1 & 0x01) s2 |= 0x01;
  if (b1 & 0x02) s2 |= 0x04;
  if (b1 & 0x04) s2 |= 0x08;
  if (b1 & 0x08) s2 |= 0x02;
  if (b1 & 0x10) s2 |= 0x80;
  if (b1 & 0x20) s2 |= 0x40;
  if (b1 & 0x40) s1 |= 0x01;
  if (b1 & 0x80) s1 |= 0x08;
  if (b2 & 0x01) s1 |= 0x10;
  if (b2 & 0x02) s1 |= 0x20;
  return [s0, s1, s2, b2];
}

function hatToDpad(hat: number, out: PadState) {
  const map = [0x02, 0x06, 0x04, 0x05, 0x01, 0x09, 0x08, 0x0a];
  if (hat < 8) out.buttons[2] |= map[hat] ?? 0;
}

function faceXboxToS1(a: boolean, b: boolean, x: boolean, y: boolean, out: PadState) {
  if (a) out.buttons[0] |= 0x04;
  if (b) out.buttons[0] |= 0x08;
  if (x) out.buttons[0] |= 0x01;
  if (y) out.buttons[0] |= 0x02;
}

function le16(buf: Uint8Array | number[], i: number): number {
  return (buf[i] ?? 0) | ((buf[i + 1] ?? 0) << 8);
}

function le16s(buf: Uint8Array | number[], i: number): number {
  const u = le16(buf, i);
  return u > 32767 ? u - 65536 : u;
}

export function parseS2Report(buf: Uint8Array | number[], kind: ControllerKind): PadState | null {
  if (buf.length < 2) return null;
  const out = emptyPad(kind);
  let id = buf[0] ?? 0;
  let p = 1;

  if (id !== 0x05 && id !== 0x09 && id !== 0x0a && id !== 0x07 && id !== 0x08) {
    if (buf.length >= 16) {
      p = 0;
      id = 0x05;
    } else {
      return null;
    }
  }

  out.reportId = id;
  const slice = (start: number, n: number) => {
    const r: number[] = [];
    for (let i = 0; i < n; i++) r.push(buf[p + start + i] ?? 0);
    return r;
  };

  if (id === 0x05) {
    out.counter = (buf[p] ?? 0) | ((buf[p + 1] ?? 0) << 8) | ((buf[p + 2] ?? 0) << 16) | ((buf[p + 3] ?? 0) << 24);
    const [s0, s1, s2, extra] = parseButtons05(slice(4, 4));
    out.buttons = [s0, s1, s2];
    out.extra = extra;
    const l = unpack12(buf, p + 0x0a);
    const r = unpack12(buf, p + 0x0d);
    out.lx = l.x;
    out.ly = l.y;
    out.rx = r.x;
    out.ry = r.y;
    out.lt = buf[p + 0x3c] ?? 0;
    out.rt = buf[p + 0x3d] ?? 0;
    out.valid = true;
    return out;
  }

  if (id === 0x09) {
    out.counter = buf[p] ?? 0;
    const [s0, s1, s2, extra] = parseButtons09(slice(2, 3));
    out.buttons = [s0, s1, s2];
    out.extra = extra;
    const l = unpack12(buf, p + 5);
    const r = unpack12(buf, p + 8);
    out.lx = l.x;
    out.ly = l.y;
    out.rx = r.x;
    out.ry = r.y;
    out.valid = true;
    return out;
  }

  if (id === 0x0a) {
    out.counter = buf[p] ?? 0;
    const [s0, s1, s2, extra] = parseButtons0a(slice(2, 3));
    out.buttons = [s0, s1, s2];
    out.extra = extra;
    const l = unpack12(buf, p + 5);
    const r = unpack12(buf, p + 8);
    out.lx = l.x;
    out.ly = l.y;
    out.rx = r.x;
    out.ry = r.y;
    out.lt = buf[p + 0x0c] ?? 0;
    out.rt = buf[p + 0x0d] ?? 0;
    out.valid = true;
    return out;
  }

  return null;
}

export function parseS1Report(buf: Uint8Array | number[], kind: ControllerKind = "s1-pro"): PadState | null {
  if (buf.length < 2) return null;
  const out = emptyPad(kind);
  const id = buf[0] ?? 0;
  out.reportId = id;
  if (id === 0x30 && buf.length >= 12) {
    out.counter = buf[1] ?? 0;
    out.buttons = [buf[3] ?? 0, (buf[4] ?? 0) & 0x3f, buf[5] ?? 0];
    const l = unpack12(buf, 6);
    const r = unpack12(buf, 9);
    out.lx = l.x;
    out.ly = l.y;
    out.rx = r.x;
    out.ry = r.y;
    out.valid = true;
    return out;
  }
  if (id === 0x3f && buf.length >= 8) {
    const b0 = buf[1] ?? 0;
    const b1 = buf[2] ?? 0;
    if (b0 & 0x01) out.buttons[0] |= 0x01;
    if (b0 & 0x02) out.buttons[0] |= 0x02;
    if (b0 & 0x04) out.buttons[0] |= 0x04;
    if (b0 & 0x08) out.buttons[0] |= 0x08;
    if (b0 & 0x10) out.buttons[0] |= 0x40;
    if (b0 & 0x20) out.buttons[0] |= 0x80;
    if (b1 & 0x01) out.buttons[1] |= 0x01;
    if (b1 & 0x02) out.buttons[1] |= 0x02;
    if (b1 & 0x04) out.buttons[1] |= 0x08;
    if (b1 & 0x08) out.buttons[1] |= 0x04;
    if (b1 & 0x10) out.buttons[1] |= 0x10;
    if (b1 & 0x20) out.buttons[1] |= 0x20;
    if (b1 & 0x40) out.buttons[2] |= 0x40;
    if (b1 & 0x80) out.buttons[2] |= 0x80;
    hatToDpad(buf[3] ?? 8, out);
    out.lx = axisU8To12(buf[4] ?? 128);
    out.ly = axisU8To12(buf[5] ?? 128);
    out.rx = axisU8To12(buf[6] ?? 128);
    out.ry = axisU8To12(buf[7] ?? 128);
    out.valid = true;
    return out;
  }
  return null;
}

export function parseXbox360(buf: Uint8Array | number[]): PadState | null {
  if (buf.length < 14) return null;
  const d = buf[0] === 0x00 && buf[1] === 0x14 ? buf : buf;
  const out = emptyPad("xbox360");
  const b0 = d[2] ?? 0;
  const b1 = d[3] ?? 0;
  if (b0 & 0x01) out.buttons[2] |= 0x02;
  if (b0 & 0x02) out.buttons[2] |= 0x01;
  if (b0 & 0x04) out.buttons[2] |= 0x08;
  if (b0 & 0x08) out.buttons[2] |= 0x04;
  if (b0 & 0x10) out.buttons[1] |= 0x02;
  if (b0 & 0x20) out.buttons[1] |= 0x01;
  if (b0 & 0x40) out.buttons[1] |= 0x08;
  if (b0 & 0x80) out.buttons[1] |= 0x04;
  if (b1 & 0x01) out.buttons[2] |= 0x40;
  if (b1 & 0x02) out.buttons[0] |= 0x40;
  if (b1 & 0x04) out.buttons[1] |= 0x10;
  faceXboxToS1(Boolean(b1 & 0x10), Boolean(b1 & 0x20), Boolean(b1 & 0x40), Boolean(b1 & 0x80), out);
  out.lt = d[4] ?? 0;
  out.rt = d[5] ?? 0;
  out.lx = axisS16To12(le16s(d, 6));
  out.ly = axisS16To12(-le16s(d, 8));
  out.rx = axisS16To12(le16s(d, 10));
  out.ry = axisS16To12(-le16s(d, 12));
  if (out.lt > 30) out.buttons[2] |= 0x80;
  if (out.rt > 30) out.buttons[0] |= 0x80;
  out.valid = true;
  return out;
}

export function parseXboxOne(buf: Uint8Array | number[], kind: ControllerKind = "xbox-one"): PadState | null {
  if (buf.length < 18 || (buf[0] ?? 0) !== 0x20) return null;
  const out = emptyPad(kind);
  out.reportId = 0x20;
  const b0 = buf[4] ?? 0;
  const b1 = buf[5] ?? 0;
  if (b0 & 0x04) out.buttons[1] |= 0x02;
  if (b0 & 0x08) out.buttons[1] |= 0x01;
  faceXboxToS1(Boolean(b0 & 0x10), Boolean(b0 & 0x20), Boolean(b0 & 0x40), Boolean(b0 & 0x80), out);
  if (b1 & 0x01) out.buttons[2] |= 0x02;
  if (b1 & 0x02) out.buttons[2] |= 0x01;
  if (b1 & 0x04) out.buttons[2] |= 0x08;
  if (b1 & 0x08) out.buttons[2] |= 0x04;
  if (b1 & 0x10) out.buttons[2] |= 0x40;
  if (b1 & 0x20) out.buttons[0] |= 0x40;
  if (b1 & 0x40) out.buttons[1] |= 0x08;
  if (b1 & 0x80) out.buttons[1] |= 0x04;
  out.lt = Math.min(255, le16(buf, 6) >> 2);
  out.rt = Math.min(255, le16(buf, 8) >> 2);
  out.lx = axisS16To12(le16s(buf, 10));
  out.ly = axisS16To12(-le16s(buf, 12));
  out.rx = axisS16To12(le16s(buf, 14));
  out.ry = axisS16To12(-le16s(buf, 16));
  if (out.lt > 30) out.buttons[2] |= 0x80;
  if (out.rt > 30) out.buttons[0] |= 0x80;
  if ((buf[18] ?? 0) & 0x01) out.buttons[1] |= 0x20;
  out.valid = true;
  return out;
}

function parseSonyCommon(b0: number, b1: number, b2: number, out: PadState, l2: number, r2: number) {
  hatToDpad(b0 & 0x0f, out);
  if (b0 & 0x10) out.buttons[0] |= 0x01;
  if (b0 & 0x20) out.buttons[0] |= 0x04;
  if (b0 & 0x40) out.buttons[0] |= 0x08;
  if (b0 & 0x80) out.buttons[0] |= 0x02;
  if (b1 & 0x01) out.buttons[2] |= 0x40;
  if (b1 & 0x02) out.buttons[0] |= 0x40;
  if (b1 & 0x04) out.buttons[2] |= 0x80;
  if (b1 & 0x08) out.buttons[0] |= 0x80;
  if (b1 & 0x10) out.buttons[1] |= 0x01;
  if (b1 & 0x20) out.buttons[1] |= 0x02;
  if (b1 & 0x40) out.buttons[1] |= 0x08;
  if (b1 & 0x80) out.buttons[1] |= 0x04;
  if (b2 & 0x01) out.buttons[1] |= 0x10;
  if (b2 & 0x02) out.buttons[1] |= 0x20;
  out.lt = l2 & 0xff;
  out.rt = r2 & 0xff;
  if (out.lt > 30) out.buttons[2] |= 0x80;
  if (out.rt > 30) out.buttons[0] |= 0x80;
}

export function parseDs4(buf: Uint8Array | number[]): PadState | null {
  if (buf.length < 10) return null;
  let off = 0;
  if ((buf[0] ?? 0) === 0x01) off = 1;
  else if ((buf[0] ?? 0) === 0x11 && buf.length >= 32) off = 3;
  const out = emptyPad("ds4");
  out.reportId = buf[0] ?? 0;
  out.lx = axisU8To12(buf[off] ?? 128);
  out.ly = axisU8To12(buf[off + 1] ?? 128);
  out.rx = axisU8To12(buf[off + 2] ?? 128);
  out.ry = axisU8To12(buf[off + 3] ?? 128);
  parseSonyCommon(buf[off + 4] ?? 8, buf[off + 5] ?? 0, buf[off + 6] ?? 0, out, buf[off + 7] ?? 0, buf[off + 8] ?? 0);
  out.valid = true;
  return out;
}

export function parseDualSense(buf: Uint8Array | number[]): PadState | null {
  if (buf.length < 12) return null;
  let off = 0;
  if ((buf[0] ?? 0) === 0x01) off = 1;
  else if ((buf[0] ?? 0) === 0x31 && buf.length >= 32) off = 2;
  const out = emptyPad("dualsense");
  out.reportId = buf[0] ?? 0;
  out.lx = axisU8To12(buf[off] ?? 128);
  out.ly = axisU8To12(buf[off + 1] ?? 128);
  out.rx = axisU8To12(buf[off + 2] ?? 128);
  out.ry = axisU8To12(buf[off + 3] ?? 128);
  parseSonyCommon(buf[off + 7] ?? 8, buf[off + 8] ?? 0, buf[off + 9] ?? 0, out, buf[off + 4] ?? 0, buf[off + 5] ?? 0);
  out.valid = true;
  return out;
}

export function parseReport(buf: Uint8Array | number[], kind: ControllerKind): PadState | null {
  switch (kindFamily(kind)) {
    case "switch-2":
      return parseS2Report(buf, kind);
    case "switch-1":
      return parseS1Report(buf, kind);
    case "xbox":
      return kind === "xbox360" ? parseXbox360(buf) : parseXboxOne(buf, kind);
    case "playstation":
      return kind === "dualsense" ? parseDualSense(buf) : parseDs4(buf);
    default:
      return parseS2Report(buf, kind) ?? parseS1Report(buf, kind) ?? parseXboxOne(buf, kind) ?? parseDs4(buf);
  }
}
