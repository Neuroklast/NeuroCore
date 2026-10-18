import { parseFaceNumber, parseFaceToken, type FaceHost } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;
const X_MIN = -1.2;
const X_MAX = 1.2;
const Y_MIN = -1.2;
const Y_MAX = 1.2;

export type DriveShaper = "identity" | "softclip" | "tube" | "diode" | "hardclip" | "fold" | "bitcrush";

export type DriveFaceModel = {
  shaper: DriveShaper;
  drive: number;
  limit: number;
  lo: number;
  hi: number;
  bits: number;
  mix: number;
};

function splitArgs(inner: string): string[] {
  const out: string[] = [];
  let depth = 0;
  let start = 0;
  for (let i = 0; i < inner.length; i += 1) {
    const c = inner[i];
    if (c === "(") {
      depth += 1;
    } else if (c === ")") {
      depth -= 1;
    } else if (c === "," && depth === 0) {
      out.push(inner.slice(start, i).trim());
      start = i + 1;
    }
  }
  out.push(inner.slice(start).trim());
  return out.filter((s) => s.length > 0);
}

function leadingCall(src: string): { name: string; args: string[] } | null {
  const s = src.trim().replace(/^y\s*=\s*/i, "");
  const m = /^(softclip|tube|diode|hardclip|bitcrush|fold)\s*\(/i.exec(s);
  if (! m || m.index !== 0) {
    return null;
  }
  let depth = 0;
  const open = s.indexOf("(");
  for (let i = open; i < s.length; i += 1) {
    if (s[i] === "(") {
      depth += 1;
    } else if (s[i] === ")") {
      depth -= 1;
      if (depth === 0) {
        return { name: m[1]!.toLowerCase(), args: splitArgs(s.slice(open + 1, i)) };
      }
    }
  }
  return null;
}

function numericArg(raw: string | undefined, fallback: number, host?: FaceHost): number {
  if (raw == null) {
    return fallback;
  }
  return parseFaceNumber(raw, fallback, -1e9, 1e9, host);
}

/** Named engine shapers only. Unknown or composite formulas stay identity — no eval. */
export function driveFaceModel(chipType: string, args: Record<string, string>, host?: FaceHost): DriveFaceModel {
  if (chipType === "bitcrush") {
    return {
      shaper: "bitcrush",
      drive: 1,
      limit: 1,
      lo: -1,
      hi: 1,
      bits: parseFaceNumber(args.bits, 8, 1, 16, host),
      mix: parseFaceNumber(args.mix, 1, 0, 1, host),
    };
  }
  const call = leadingCall(args.y ?? "x");
  if (! call) {
    return { shaper: "identity", drive: 1, limit: 1, lo: -1, hi: 1, bits: 8, mix: 1 };
  }
  const shaper = call.name as DriveShaper;
  return {
    shaper,
    drive: numericArg(call.args[1], shaper === "softclip" ? (call.args.length < 2 ? 1 : 2) : 2, host),
    limit: numericArg(call.args[1], 0.7, host),
    lo: numericArg(call.args[1], -0.5, host),
    hi: numericArg(call.args[2], 0.5, host),
    bits: numericArg(call.args[1], 8, host),
    mix: parseFaceNumber(args.mix, 1, 0, 1, host),
  };
}

function hardClipSoftKnee(x: number, limit: number): number {
  const L = Math.max(1e-6, Math.abs(limit));
  const t = x / L;
  const absT = Math.abs(t);
  if (absT < 0.8) {
    return x;
  }
  const den = (1 + absT ** 24) ** (1 / 24);
  return L * (t / Math.max(den, 1e-12));
}

function softClipSmooth(x: number): number {
  const k = Math.PI / 2;
  const s = 2 / Math.PI;
  return s * Math.atan(k * Math.min(80, Math.max(-80, x)));
}

function tubeTransfer(x: number, drive: number): number {
  const d = Math.min(12, Math.max(0.5, drive));
  const u = Math.min(12, Math.max(-12, x * d));
  const bias = 0.12;
  const y = 0.7 * Math.tanh(u + bias) + 0.3 * Math.tanh(u * 1.08 - bias * 0.3);
  return y - (0.7 * Math.tanh(bias) + 0.3 * Math.tanh(-bias * 0.3));
}

function diodeTransfer(x: number, drive: number): number {
  const d = Math.min(16, Math.max(0.5, drive));
  const u = Math.min(40, Math.max(-40, x * d));
  const den = Math.asinh(d);
  return den < 1e-6 ? 0 : Math.asinh(u) / den;
}

function foldTransfer(x: number, lo: number, hi: number): number {
  const a = Math.min(lo, hi);
  const b = Math.max(lo, hi);
  if (b - a < 1e-6) {
    return Math.min(b, Math.max(a, x));
  }
  let y = x;
  for (let i = 0; i < 8; i += 1) {
    if (y > b) {
      y = b - (y - b);
    } else if (y < a) {
      y = a + (a - y);
    } else {
      break;
    }
  }
  return Math.min(b, Math.max(a, y));
}

function bitcrushTransfer(x: number, bits: number): number {
  const b = Math.min(24, Math.max(1, bits));
  const levels = 2 ** (b - 1);
  const xn = Math.min(1, Math.max(-1, x));
  return Math.round(xn * levels) / levels;
}

export function driveSample(chipType: string, args: Record<string, string>, x: number, host?: FaceHost): number {
  const m = driveFaceModel(chipType, args, host);
  let y = x;
  if (m.shaper === "softclip") {
    const d = Math.min(8, Math.max(0.25, m.drive));
    y = softClipSmooth(x * d);
  } else if (m.shaper === "tube") {
    y = tubeTransfer(x, m.drive);
  } else if (m.shaper === "diode") {
    y = diodeTransfer(x, m.drive);
  } else if (m.shaper === "hardclip") {
    y = hardClipSoftKnee(x, m.limit);
  } else if (m.shaper === "fold") {
    y = foldTransfer(x, m.lo, m.hi);
  } else if (m.shaper === "bitcrush") {
    y = bitcrushTransfer(x, m.bits);
  }
  return x * (1 - m.mix) + y * m.mix;
}

export function driveTransferPath(chipType: string, args: Record<string, string>, host?: FaceHost): string {
  const last = FACE_POINTS - 1;
  const parts: string[] = [];
  for (let i = 0; i < FACE_POINTS; i += 1) {
    const x = X_MIN + (X_MAX - X_MIN) * (i / last);
    const yv = driveSample(chipType, args, x, host);
    const px = (VIEW_W * i) / last;
    const t = (yv - Y_MIN) / (Y_MAX - Y_MIN);
    const py = VIEW_H - VIEW_H * Math.min(1, Math.max(0, t));
    parts.push(`${i === 0 ? "M" : "L"}${px.toFixed(2)} ${py.toFixed(2)}`);
  }
  return parts.join(" ");
}

export function driveToneHz(args: Record<string, string>, host?: FaceHost): number {
  return parseFaceNumber(args.tone, 8000, 800, 16000, host);
}

export function driveChannel(args: Record<string, string>): string {
  return parseFaceToken(args.channel, "both");
}
