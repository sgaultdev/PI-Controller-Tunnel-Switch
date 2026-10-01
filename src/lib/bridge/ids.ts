export const VID_NINTENDO = 0x057e;
export const VID_MICROSOFT = 0x045e;
export const VID_SONY = 0x054c;

export const PID_S1_PRO = 0x2009;
export const PID_S1_JC_L = 0x2006;
export const PID_S1_JC_R = 0x2007;
export const PID_GC_ADAPTER = 0x0337;
export const PID_S2_PRO = 0x2069;
export const PID_S2_GC = [0x2073, 0x206a, 0x206b] as const;
export const PID_JC2_R = 0x2066;
export const PID_JC2_L = 0x2067;

export type ControllerKind =
  | "unknown"
  | "s2-pro"
  | "s2-gc"
  | "jc2-l"
  | "jc2-r"
  | "s1-pro"
  | "s1-jc-l"
  | "s1-jc-r"
  | "xbox360"
  | "xbox-one"
  | "xbox-series"
  | "ds4"
  | "dualsense"
  | "gc-adapter";

export type ControllerFamily = "unknown" | "switch-2" | "switch-1" | "xbox" | "playstation" | "gc-adapter";

export type OutMode = "auto" | "pro" | "hub" | "gc-adapter";

export function kindFromVidPid(vid: number, pid: number): ControllerKind {
  if (vid === VID_NINTENDO) {
    if (pid === PID_S2_PRO) return "s2-pro";
    if ((PID_S2_GC as readonly number[]).includes(pid)) return "s2-gc";
    if (pid === PID_JC2_L) return "jc2-l";
    if (pid === PID_JC2_R) return "jc2-r";
    if (pid === PID_S1_PRO) return "s1-pro";
    if (pid === PID_S1_JC_L) return "s1-jc-l";
    if (pid === PID_S1_JC_R) return "s1-jc-r";
    if (pid === PID_GC_ADAPTER) return "gc-adapter";
  }
  if (vid === VID_MICROSOFT) {
    if (pid === 0x028e || pid === 0x028f) return "xbox360";
    if (pid === 0x0b12 || pid === 0x0b13) return "xbox-series";
    return "xbox-one";
  }
  if (vid === VID_SONY) {
    if (pid === 0x0ce6 || pid === 0x0df2) return "dualsense";
    if (pid === 0x05c4 || pid === 0x09cc || pid === 0x0ba0) return "ds4";
  }
  return "unknown";
}

export function kindFromPid(pid: number): ControllerKind {
  return kindFromVidPid(VID_NINTENDO, pid);
}

export function kindFamily(kind: ControllerKind): ControllerFamily {
  switch (kind) {
    case "s2-pro":
    case "s2-gc":
    case "jc2-l":
    case "jc2-r":
      return "switch-2";
    case "s1-pro":
    case "s1-jc-l":
    case "s1-jc-r":
      return "switch-1";
    case "xbox360":
    case "xbox-one":
    case "xbox-series":
      return "xbox";
    case "ds4":
    case "dualsense":
      return "playstation";
    case "gc-adapter":
      return "gc-adapter";
    default:
      return "unknown";
  }
}

export function kindName(kind: ControllerKind): string {
  switch (kind) {
    case "s2-pro":
      return "Switch 2 Pro Controller";
    case "s2-gc":
      return "NSO GameCube Controller";
    case "jc2-l":
      return "Joy-Con 2 (L)";
    case "jc2-r":
      return "Joy-Con 2 (R)";
    case "s1-pro":
      return "Switch 1 Pro Controller";
    case "s1-jc-l":
      return "Joy-Con (L)";
    case "s1-jc-r":
      return "Joy-Con (R)";
    case "xbox360":
      return "Xbox 360 Controller";
    case "xbox-one":
      return "Xbox One Controller";
    case "xbox-series":
      return "Xbox Series Controller";
    case "ds4":
      return "DualShock 4";
    case "dualsense":
      return "DualSense";
    case "gc-adapter":
      return "GameCube Adapter";
    default:
      return "Unknown controller";
  }
}

export function kindFromGamepadId(id: string): ControllerKind {
  const s = id.toLowerCase();
  if (s.includes("dualsense") || s.includes("0ce6") || s.includes("0df2")) return "dualsense";
  if (s.includes("dualshock") || s.includes("wireless controller") || s.includes("05c4") || s.includes("09cc"))
    return "ds4";
  if (s.includes("xbox 360") || s.includes("028e")) return "xbox360";
  if (s.includes("xbox series") || s.includes("0b12")) return "xbox-series";
  if (s.includes("xbox") || s.includes("xinput") || s.includes("045e")) return "xbox-one";
  if (s.includes("gamecube") || s.includes("2073") || s.includes("wup-028")) return "s2-gc";
  if (s.includes("joy-con")) return s.includes("left") || s.includes("(l)") ? "s1-jc-l" : "s1-jc-r";
  if (s.includes("pro controller") && (s.includes("2069") || s.includes("switch 2"))) return "s2-pro";
  if (s.includes("pro controller") || s.includes("2009")) return "s1-pro";
  return "s2-pro";
}

export const OUT_CAPACITY: Record<Exclude<OutMode, "auto">, number> = {
  pro: 1,
  hub: 8,
  "gc-adapter": 8,
};

export function outCapacity(mode: OutMode): number {
  if (mode === "auto") return 8;
  return OUT_CAPACITY[mode];
}

export function outResolve(mode: OutMode, npads: number): Exclude<OutMode, "auto"> {
  if (mode !== "auto") return mode;
  return npads <= 1 ? "pro" : "hub";
}

export function outModeName(mode: OutMode): string {
  switch (mode) {
    case "pro":
      return "Pro Controller";
    case "hub":
      return "Pro Controller hub";
    case "gc-adapter":
      return "GameCube Adapter";
    default:
      return "Auto";
  }
}

export const S2_WAKE = {
  initUsb: [0x03, 0x91, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff],
  enableHid: [0x03, 0x91, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00],
  select09: [0x03, 0x91, 0x00, 0x0a, 0x00, 0x04, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00],
  select0a: [0x03, 0x91, 0x00, 0x0a, 0x00, 0x04, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00],
  select05: [0x03, 0x91, 0x00, 0x0a, 0x00, 0x04, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00],
} as const;

export const XBOX_GIP_WAKE = {
  power: [0x05, 0x20, 0x00, 0x01, 0x00],
  sInit: [0x05, 0x20, 0x00, 0x0f, 0x06],
} as const;

export const S1_IDS = {
  manufacturer: "Nintendo Co., Ltd.",
  product: "Pro Controller",
  serial: "000000000001",
  vid: 0x057e,
  pid: 0x2009,
} as const;

export const GC_ADAPTER_IDS = {
  manufacturer: "Nintendo Co., Ltd.",
  product: "WUP-028",
  vid: 0x057e,
  pid: 0x0337,
} as const;
