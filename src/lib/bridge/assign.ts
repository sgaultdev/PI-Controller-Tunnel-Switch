import { outCapacity, outResolve, type OutMode } from "./ids";

export type PadAssign = {
  slot: number;
  link: number;
  port: number;
};

export function splitEven(npads: number, nlinks: number, capPer: number): number[] {
  const links = Math.max(1, Math.min(4, nlinks | 0));
  const cap = Math.max(1, capPer | 0);
  const n = Math.min(Math.max(0, npads), links * cap);
  const base = Math.floor(n / links);
  const rem = n % links;
  return Array.from({ length: links }, (_, i) => Math.min(cap, base + (i < rem ? 1 : 0)));
}

export function planAssigns(npads: number, nlinks: number, mode: OutMode): PadAssign[] {
  const resolved = outResolve(mode, npads);
  const per = outCapacity(resolved);
  const counts = splitEven(npads, nlinks, per);
  const out: PadAssign[] = [];
  let slot = 0;
  counts.forEach((count, link) => {
    for (let port = 0; port < count; port++) {
      out.push({ slot, link, port });
      slot++;
    }
  });
  return out;
}
