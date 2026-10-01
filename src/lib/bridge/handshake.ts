import { pack12 } from "./pack";
import type { BridgeConfig, S1State } from "./types";

export type HandshakeEvent = {
  dir: "in" | "out";
  label: string;
  hex: string;
  state: S1State;
};

export type S1Machine = {
  state: S1State;
  timer: number;
  reportMode: number;
  imu: boolean;
  rumble: boolean;
  usbOnly: boolean;
  playerLed: number;
  lastCmd: Uint8Array;
  lastReply: Uint8Array | null;
  log: HandshakeEvent[];
};

function hex(buf: Uint8Array, n = 16): string {
  return Array.from(buf.slice(0, n))
    .map((b) => b.toString(16).padStart(2, "0"))
    .join(" ");
}

export function initS1Machine(cfg: BridgeConfig): S1Machine {
  return {
    state: "attached",
    timer: 0,
    reportMode: 0x3f,
    imu: false,
    rumble: false,
    usbOnly: false,
    playerLed: cfg.playerLed,
    lastCmd: new Uint8Array(64),
    lastReply: null,
    log: [],
  };
}

export function unsolicitedConn(cfg: BridgeConfig): Uint8Array {
  const out = new Uint8Array(64);
  out[0] = 0x81;
  out[1] = 0x01;
  out[3] = 0x03;
  for (let i = 0; i < 6; i++) out[4 + i] = cfg.mac[i] ?? 0;
  return out;
}

function fillPrefix(r: Uint8Array, id: number, timer: number) {
  r[0] = id;
  r[1] = timer;
  r[2] = 0x91;
  r[4] = 0x80;
  const c = pack12(0x800, 0x800);
  r[6] = c[0];
  r[7] = c[1];
  r[8] = c[2];
  r[9] = c[0];
  r[10] = c[1];
  r[11] = c[2];
}

export function handleS1Output(m: S1Machine, cfg: BridgeConfig, input: Uint8Array): Uint8Array | null {
  m.lastCmd = input.slice(0, 64);
  const id = input[0] ?? 0;

  if (id === 0x80) {
    const cmd = input[1] ?? 0;
    const reply = new Uint8Array(64);
    reply[0] = 0x81;
    reply[1] = cmd;
    let label = `USB 0x80 0x${cmd.toString(16)}`;
    if (cmd === 0x01) {
      reply[3] = 0x03;
      for (let i = 0; i < 6; i++) reply[4 + i] = cfg.mac[i] ?? 0;
      m.state = "conn-status";
      label = "Connection status → Pro Controller + MAC";
    } else if (cmd === 0x02) {
      m.state = "handshake";
      label = "UART handshake ACK";
    } else if (cmd === 0x03) {
      m.state = "baud";
      label = "Baud 3 Mbps ACK";
    } else if (cmd === 0x04) {
      m.usbOnly = true;
      m.state = "usb-only";
      m.log.push({ dir: "in", label: "Disable timeout (USB-only)", hex: hex(input), state: m.state });
      return null;
    } else if (cmd === 0x05) {
      m.usbOnly = false;
      return null;
    }
    m.lastReply = reply;
    m.log.push({ dir: "in", label, hex: hex(input), state: m.state });
    m.log.push({ dir: "out", label: `Reply ${label}`, hex: hex(reply), state: m.state });
    return reply;
  }

  if (id === 0x01 && input.length >= 11) {
    const sub = input[10] ?? 0;
    const reply = new Uint8Array(64);
    fillPrefix(reply, 0x21, m.timer++);
    reply[13] = 0x80 | sub;
    reply[14] = sub;
    let label = `Subcommand 0x${sub.toString(16)}`;
    if (sub === 0x02) {
      reply[15] = 0x03;
      reply[16] = 0x48;
      reply[17] = 0x03;
      reply[18] = 0x02;
      for (let i = 0; i < 6; i++) reply[19 + i] = cfg.mac[i] ?? 0;
      reply[25] = 0x01;
      reply[26] = 0x01;
      m.state = "subcommands";
      label = "Device info (Pro Controller FW 3.89)";
    } else if (sub === 0x10) {
      const addr = (input[11] ?? 0) | ((input[12] ?? 0) << 8) | ((input[13] ?? 0) << 16) | ((input[14] ?? 0) << 24);
      const sz = input[15] ?? 0;
      reply[13] = 0x90;
      reply[14] = 0x10;
      reply[15] = addr & 0xff;
      reply[16] = (addr >> 8) & 0xff;
      reply[17] = (addr >> 16) & 0xff;
      reply[18] = (addr >> 24) & 0xff;
      reply[19] = sz;
      label = `SPI read @ 0x${addr.toString(16)} (${sz} B)`;
    } else if (sub === 0x03) {
      m.reportMode = input[11] ?? 0x30;
      if (m.reportMode === 0x30) m.state = "streaming";
      label = `Set report mode 0x${m.reportMode.toString(16)}`;
    } else if (sub === 0x30) {
      m.playerLed = input[11] ?? 0;
      label = `Player lights 0x${m.playerLed.toString(16)}`;
    } else if (sub === 0x40) {
      m.imu = (input[11] ?? 0) !== 0;
      label = m.imu ? "Enable IMU" : "Disable IMU";
    } else if (sub === 0x48) {
      m.rumble = (input[11] ?? 0) !== 0;
      label = m.rumble ? "Enable vibration" : "Disable vibration";
    }
    m.lastReply = reply;
    m.log.push({ dir: "in", label, hex: hex(input), state: m.state });
    m.log.push({ dir: "out", label: `0x21 ACK`, hex: hex(reply), state: m.state });
    return reply;
  }

  if (id === 0x10) {
    m.log.push({ dir: "in", label: "Rumble-only", hex: hex(input, 10), state: m.state });
    return null;
  }

  return null;
}

export const S1_HANDSHAKE_SCRIPT: { dir: "console" | "controller"; bytes: number[]; label: string }[] = [
  { dir: "console", bytes: [0x80, 0x01], label: "Request connection status" },
  { dir: "controller", bytes: [0x81, 0x01, 0x00, 0x03], label: "MAC + Pro Controller type" },
  { dir: "console", bytes: [0x80, 0x02], label: "UART handshake" },
  { dir: "controller", bytes: [0x81, 0x02], label: "Handshake ACK" },
  { dir: "console", bytes: [0x80, 0x03], label: "Switch baud 3 Mbps" },
  { dir: "controller", bytes: [0x81, 0x03], label: "Baud ACK" },
  { dir: "console", bytes: [0x80, 0x04], label: "Disable USB timeout" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02], label: "Request device info" },
  { dir: "controller", bytes: [0x21], label: "FW / type / MAC" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x3d, 0x60, 0x00, 0x00, 0x12], label: "SPI stick calibration" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x30], label: "Set report mode 0x30" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x01], label: "Player LED 1" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01], label: "Enable IMU" },
  { dir: "console", bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x01], label: "Enable vibration" },
];

export const S2_WAKE_SCRIPT: { bytes: number[]; label: string }[] = [
  { bytes: [0x03, 0x91, 0x00, 0x0d, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff], label: "Initialise USB (0x03/0x0D) with dummy host MAC" },
  { bytes: [0x07, 0x91, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00], label: "Command 0x07 probe" },
  { bytes: [0x03, 0x91, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00], label: "Enable HID input reports" },
  { bytes: [0x0c, 0x91, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00], label: "Feature select (IMU / motion bits)" },
  { bytes: [0x03, 0x91, 0x00, 0x0a, 0x00, 0x04, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00], label: "Select input report 0x09 (Pro 2) / 0x0A (GC)" },
];

export const S1_HOST_WAKE_SCRIPT: { bytes: number[]; label: string }[] = [
  { bytes: [0x80, 0x01], label: "Ask Switch 1 pad for connection status" },
  { bytes: [0x80, 0x02], label: "UART handshake (as host)" },
  { bytes: [0x80, 0x03], label: "Baud" },
  { bytes: [0x80, 0x04], label: "USB-only" },
  { bytes: [0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x30], label: "Request 0x30 full input reports" },
];

export const XBOX_WAKE_SCRIPT: { bytes: number[]; label: string }[] = [
  { bytes: [0x05, 0x20, 0x00, 0x01, 0x00], label: "GIP power-on (Xbox One / Series)" },
  { bytes: [0x05, 0x20, 0x00, 0x0f, 0x06], label: "GIP S-init / LED" },
];

export const GC_ADAPTER_SCRIPT: { bytes: number[]; label: string }[] = [
  { bytes: [0x13], label: "Host start (interrupt OUT 0x13)" },
  { bytes: [0x21], label: "37-byte 0x21 input: 4 ports × 9 bytes" },
  { bytes: [0x11, 0x00, 0x00, 0x00, 0x00], label: "Rumble: 0x11 + 4 port bits" },
];
