import { parseFaceNumber, parseFaceToken } from "./faceArgs";

/** Host-rate used for calculated EQ biquads. Not a live tap. */
export const FACE_FS = 48000;
export const FACE_FMIN = 20;
export const FACE_FMAX = 20000;
export const FACE_POINTS = 48;
const VIEW_W = 100;
const VIEW_H = 40;
const DB_MIN = -24;
const DB_MAX = 6;

export type FilterMode = "lowpass" | "highpass" | "bandpass" | "allpass" | "peak" | "lowshelf" | "highshelf" | "notch";

export type FilterFaceModel = {
  family: "filter" | "eq";
  mode: FilterMode;
  fc: number;
  q: number;
  gainDb: number;
};

export function logFreqs(n = FACE_POINTS): number[] {
  const out: number[] = [];
  const log0 = Math.log(FACE_FMIN);
  const log1 = Math.log(FACE_FMAX);
  const last = Math.max(1, n - 1);
  for (let i = 0; i < n; i += 1) {
    out.push(Math.exp(log0 + (log1 - log0) * (i / last)));
  }
  return out;
}

export function magToDb(mag: number): number {
  return 20 * Math.log10(Math.max(mag, 1e-6));
}

/** Analog SVF matching the engine's TPT job (Q 0.1–4). Allpass is the one-pole (|H|=1). */
export function svfMag(mode: FilterMode, f: number, fc: number, q: number): number {
  if (mode === "allpass") {
    return 1;
  }
  const w = 2 * Math.PI * f;
  const w0 = 2 * Math.PI * Math.max(1, fc);
  const w02 = w0 * w0;
  const re = -w * w + w02;
  const im = (w0 / Math.max(0.1, q)) * w;
  const den2 = re * re + im * im;
  let numRe = 0;
  let numIm = 0;
  if (mode === "lowpass") {
    numRe = w02;
  } else if (mode === "highpass") {
    numRe = -w * w;
  } else {
    numIm = w0 * w;
  }
  return Math.sqrt((numRe * numRe + numIm * numIm) / Math.max(den2, 1e-24));
}

function biquadMag(
  f: number,
  fs: number,
  b0: number,
  b1: number,
  b2: number,
  a0: number,
  a1: number,
  a2: number,
): number {
  const w = 2 * Math.PI * f / fs;
  const z1r = Math.cos(w);
  const z1i = -Math.sin(w);
  const z2r = Math.cos(2 * w);
  const z2i = -Math.sin(2 * w);
  const nr = b0 + b1 * z1r + b2 * z2r;
  const ni = b1 * z1i + b2 * z2i;
  const dr = a0 + a1 * z1r + a2 * z2r;
  const di = a1 * z1i + a2 * z2i;
  return Math.hypot(nr, ni) / Math.max(1e-12, Math.hypot(dr, di));
}

/** Audio EQ Cookbook coefficients, same shapes JUCE `IIR::ArrayCoefficients` uses. */
export function eqBiquad(
  mode: FilterMode,
  fc: number,
  q: number,
  gainDb: number,
  fs = FACE_FS,
): [number, number, number, number, number, number] {
  const f = Math.min(fs * 0.45, Math.max(20, fc));
  const qv = Math.min(12, Math.max(0.1, q));
  const A = 10 ** (gainDb / 40);
  const w0 = 2 * Math.PI * f / fs;
  const cos = Math.cos(w0);
  const sin = Math.sin(w0);
  const alpha = sin / (2 * qv);
  if (mode === "lowshelf") {
    const twoSqrtAAlpha = 2 * Math.sqrt(A) * alpha;
    return [
      A * ((A + 1) - (A - 1) * cos + twoSqrtAAlpha),
      2 * A * ((A - 1) - (A + 1) * cos),
      A * ((A + 1) - (A - 1) * cos - twoSqrtAAlpha),
      (A + 1) + (A - 1) * cos + twoSqrtAAlpha,
      -2 * ((A - 1) + (A + 1) * cos),
      (A + 1) + (A - 1) * cos - twoSqrtAAlpha,
    ];
  }
  if (mode === "highshelf") {
    const twoSqrtAAlpha = 2 * Math.sqrt(A) * alpha;
    return [
      A * ((A + 1) + (A - 1) * cos + twoSqrtAAlpha),
      -2 * A * ((A - 1) + (A + 1) * cos),
      A * ((A + 1) + (A - 1) * cos - twoSqrtAAlpha),
      (A + 1) - (A - 1) * cos + twoSqrtAAlpha,
      2 * ((A - 1) - (A + 1) * cos),
      (A + 1) - (A - 1) * cos - twoSqrtAAlpha,
    ];
  }
  if (mode === "notch") {
    return [1, -2 * cos, 1, 1 + alpha, -2 * cos, 1 - alpha];
  }
  if (mode === "lowpass") {
    const b = (1 - cos) / 2;
    return [b, 1 - cos, b, 1 + alpha, -2 * cos, 1 - alpha];
  }
  if (mode === "highpass") {
    const b = (1 + cos) / 2;
    return [b, -(1 + cos), b, 1 + alpha, -2 * cos, 1 - alpha];
  }
  const b0 = 1 + alpha * A;
  const b2 = 1 - alpha * A;
  const a0 = 1 + alpha / A;
  const a2 = 1 - alpha / A;
  return [b0, -2 * cos, b2, a0, -2 * cos, a2];
}

export function eqMag(mode: FilterMode, f: number, fc: number, q: number, gainDb: number, fs = FACE_FS): number {
  const [b0, b1, b2, a0, a1, a2] = eqBiquad(mode, fc, q, gainDb, fs);
  return biquadMag(f, fs, b0, b1, b2, a0, a1, a2);
}

function filterMode(chipType: string, args: Record<string, string>): FilterMode {
  const t = parseFaceToken(args.type, chipType === "eq" ? "peak" : "lowpass");
  if (t === "highpass" || t === "hpf" || t === "hp" || t === "lowcut") {
    return "highpass";
  }
  if (t === "bandpass" || t === "bpf" || t === "bp") {
    return "bandpass";
  }
  if (t === "allpass" || t === "ap" || t === "apf") {
    return "allpass";
  }
  if (t === "lowshelf" || t === "ls" || t === "low_shelf") {
    return "lowshelf";
  }
  if (t === "highshelf" || t === "hs" || t === "high_shelf") {
    return "highshelf";
  }
  if (t === "notch" || t === "bandstop" || t === "bandreject") {
    return "notch";
  }
  if (t === "peak") {
    return "peak";
  }
  return "lowpass";
}

export function filterFaceModel(chipType: string, args: Record<string, string>): FilterFaceModel {
  const family = chipType === "eq" ? "eq" : "filter";
  const mode = filterMode(chipType, args);
  const fcKey = family === "eq" ? (args.freq ?? args.cutoff) : (args.cutoff ?? args.center);
  const qKey = family === "eq" ? (args.q ?? args.resonance) : args.resonance;
  return {
    family,
    mode,
    fc: parseFaceNumber(fcKey, 1000, 20, 20000),
    q: parseFaceNumber(qKey, family === "eq" ? 0.7 : 0.4, 0.1, family === "eq" ? 12 : 4),
    gainDb: parseFaceNumber(args.gain, 0, -24, 24),
  };
}

export function filterMags(chipType: string, args: Record<string, string>, freqs = logFreqs()): number[] {
  const m = filterFaceModel(chipType, args);
  return freqs.map((f) => (
    m.family === "eq"
      ? eqMag(m.mode, f, m.fc, m.q, m.gainDb)
      : svfMag(m.mode, f, m.fc, m.q)
  ));
}

export function pathFromDb(dbs: number[], yMin = DB_MIN, yMax = DB_MAX): string {
  const last = Math.max(1, dbs.length - 1);
  return dbs.map((db, i) => {
    const x = (VIEW_W * i) / last;
    const t = (db - yMin) / (yMax - yMin);
    const y = VIEW_H - VIEW_H * Math.min(1, Math.max(0, t));
    return `${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`;
  }).join(" ");
}

export function filterCurvePath(chipType: string, args: Record<string, string>): string {
  return pathFromDb(filterMags(chipType, args).map(magToDb));
}
