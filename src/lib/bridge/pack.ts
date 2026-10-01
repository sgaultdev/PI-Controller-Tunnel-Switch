export const STICK_CENTER = 0x800;
export const STICK_MAX = 0xfff;

export function pack12(x: number, y: number): [number, number, number] {
  x &= 0xfff;
  y &= 0xfff;
  return [x & 0xff, ((x >> 8) & 0x0f) | ((y & 0x0f) << 4), (y >> 4) & 0xff];
}

export function unpack12(bytes: number[] | Uint8Array, offset = 0): { x: number; y: number } {
  const a = bytes[offset] ?? 0;
  const b = bytes[offset + 1] ?? 0;
  const c = bytes[offset + 2] ?? 0;
  return {
    x: a | ((b & 0x0f) << 8),
    y: (b >> 4) | (c << 4),
  };
}

export function clamp12(v: number): number {
  if (v < 0) return 0;
  if (v > 0xfff) return 0xfff;
  return v & 0xfff;
}

export function invertY(y: number, invert: boolean): number {
  return invert ? 0xfff - y : y;
}

export function axisU8To12(v: number): number {
  return Math.round(((v & 0xff) * 0xfff) / 255);
}

export function axisS16To12(v: number): number {
  const u = (v + 32768) & 0xffff;
  return u >> 4;
}

export function axis12ToU8(v: number): number {
  return Math.round(((v & 0xfff) * 255) / 0xfff);
}

export function hexBytes(data: ArrayLike<number>, max = 64): string {
  const n = Math.min(data.length, max);
  const parts: string[] = [];
  for (let i = 0; i < n; i++) {
    parts.push(data[i]!.toString(16).padStart(2, "0"));
  }
  return parts.join(" ");
}
