import { liveTheme } from "../theme/theme";

export type ScopeSource = "in" | "out" | "both";
export type ScopeXScale = "samples" | "time" | "freq";
export type ScopeYScale = "linear" | "db";

export const SCOPE_COLOR = {
  get in() {
    return liveTheme().cyan;
  },
  get out() {
    return liveTheme().accent;
  },
  get delta() {
    return liveTheme().warn;
  },
};

export interface ScopeTrace {
  id: "in" | "out" | "delta";
  color: string;
  samples: Float32Array;
}

export function deltaSamples(out: ArrayLike<number>, inn: ArrayLike<number>, n: number): Float32Array {
  const dest = new Float32Array(n);
  for (let i = 0; i < n; i += 1) {
    dest[i] = (out[i] ?? 0) - (inn[i] ?? 0);
  }
  return dest;
}

/** Pixel 0 is sample 0, last pixel is the last shown sample. */
export function sampleAtPx(samples: ArrayLike<number>, n: number, px: number, width: number): number {
  const count = Math.max(2, n);
  const span = Math.max(1, width - 1);
  const t = (Math.max(0, Math.min(span, px)) / span) * (count - 1);
  const i0 = Math.floor(t);
  const i1 = Math.min(count - 1, i0 + 1);
  const f = t - i0;
  return ((samples[i0] ?? 0) * (1 - f)) + ((samples[i1] ?? 0) * f);
}

export function tracesFor(
  source: ScopeSource,
  delta: boolean,
  scopeIn: ArrayLike<number>,
  scopeOut: ArrayLike<number>,
  n: number,
): ScopeTrace[] {
  const traces: ScopeTrace[] = [];
  if (source === "in" || source === "both") {
    traces.push({ id: "in", color: SCOPE_COLOR.in, samples: Float32Array.from(scopeIn).subarray(0, n) });
  }
  if (source === "out" || source === "both") {
    traces.push({ id: "out", color: SCOPE_COLOR.out, samples: Float32Array.from(scopeOut).subarray(0, n) });
  }
  if (delta) {
    traces.push({ id: "delta", color: SCOPE_COLOR.delta, samples: deltaSamples(scopeOut, scopeIn, n) });
  }
  return traces;
}

export function scopeTitle(source: ScopeSource, delta: boolean): string {
  const head = source === "in" ? "IN // PRE" : source === "out" ? "OUT // POST" : "IN+OUT";
  return delta ? `${head} + Δ` : head;
}

export function fieldTitle(source: ScopeSource): string {
  if (source === "in") {
    return "IN FIELD";
  }
  if (source === "out") {
    return "OUT FIELD";
  }
  return "FIELD";
}

export function loudTitle(source: ScopeSource): string {
  if (source === "in") {
    return "IN LU";
  }
  if (source === "out") {
    return "OUT LU";
  }
  return "LU";
}

/** 0..100 bar height from linear peak/rms. Silence sits on the floor. */
export function barFillPercent(linear: number): number {
  const db = 20 * Math.log10(Math.max(1.0e-8, linear));
  return Math.max(2, Math.min(100, ((db + 60) / 60) * 100));
}

/** Demo telemetry must move the LU bars, not park them on a constant. */
export function demoLoudness(t: number): {
  inPeak: number;
  outPeak: number;
  inRms: number;
  outRms: number;
} {
  const inn = 0.18 + 0.5 * (0.5 + 0.5 * Math.sin(t * 0.11));
  const out = 0.22 + 0.55 * (0.5 + 0.5 * Math.sin(t * 0.11 + 0.45));
  return { inPeak: inn, outPeak: out, inRms: inn * 0.62, outRms: out * 0.58 };
}

/** Goniometer: x = side, y = −mid (up is in-phase). Same as native ScopeAnalytics. */
export function gonioPoint(l: number, r: number): { x: number; y: number } {
  const left = Number.isFinite(l) ? l : 0;
  const right = Number.isFinite(r) ? r : 0;
  return { x: 0.5 * (left - right), y: -0.5 * (left + right) };
}

export function nextProcessMode(mode: string): "STUDIO" | "LIVE" {
  return mode === "LIVE" ? "STUDIO" : "LIVE";
}

export function processModeIndex(mode: string): number {
  return mode === "LIVE" ? 1 : 0;
}

export const SPEC_BINS = 48;
export const SPEC_DEPTH = 28;

/** Which spectra to paint. BOTH is IN (cyan) then OUT (accent), never a single mix. */
export function scopeSpectra(source: ScopeSource): Array<"in" | "out"> {
  if (source === "both") {
    return ["in", "out"];
  }
  return source === "in" ? ["in"] : ["out"];
}

/** Linear FFT bin → 0..1 display. Floor −72 dB so a loud hit still fills. */
export function specMag01(linear: number): number {
  if (! Number.isFinite(linear) || linear <= 0) {
    return 0;
  }
  const db = 20 * Math.log10(Math.max(1.0e-8, linear));
  return Math.max(0, Math.min(1, (db + 72) / 72));
}

export function spectrogramPush(hist: number[][], bins: ArrayLike<number>): number[][] {
  const row = new Array<number>(SPEC_BINS);
  for (let i = 0; i < SPEC_BINS; i += 1) {
    const v = Number(bins[i] ?? 0);
    row[i] = Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : 0;
  }
  const next = [row, ...hist];
  if (next.length > SPEC_DEPTH) {
    next.length = SPEC_DEPTH;
  }
  return next;
}

/** row 0 = now (front/bottom). Farther rows recede up and pinch in. */
export function spectrogramProject(
  bin: number,
  row: number,
  mag: number,
  w: number,
  h: number,
): { x: number; y: number } {
  const recede = row / Math.max(1, SPEC_DEPTH - 1);
  const left = w * 0.05;
  const span = w * 0.9;
  const x0 = left + recede * span * 0.2;
  const x1 = left + span - recede * span * 0.06;
  const t = bin / Math.max(1, SPEC_BINS - 1);
  const x = x0 + t * (x1 - x0);
  const floor = h * 0.88;
  const rise = mag * h * 0.5 * (1 - recede * 0.28);
  const y = floor - recede * h * 0.58 - rise;
  return { x, y };
}

/** Sparse 0..1 grain. Most cells stay 0 so the overlay stays a speckle, not snow. */
export function techNoise(x: number, y: number, frame: number): number {
  const n = Math.sin(x * 12.9898 + y * 78.233 + frame * 0.17) * 43758.5453;
  const f = n - Math.floor(n);
  return f > 0.972 ? (f - 0.972) / 0.028 : 0;
}

/** Same 1px speckle as the footer spectrograph. */
export function paintTechNoise(
  ctx: CanvasRenderingContext2D,
  w: number,
  h: number,
  frame: number,
  fillStyle: string,
): void {
  ctx.fillStyle = fillStyle;
  for (let y = 2; y < h; y += 5) {
    for (let x = 2; x < w; x += 7) {
      const n = techNoise(x, y, frame);
      if (n > 0) {
        ctx.globalAlpha = 0.18 + n * 0.45;
        ctx.fillRect(x, y, 1, 1);
      }
    }
  }
  ctx.globalAlpha = 1;
}

export const SCOPE_MENU = {
  x: [
    { id: "samples", label: "Scale Samples" },
    { id: "time", label: "Scale Time" },
    { id: "freq", label: "Scale Frequency" },
  ],
  y: [
    { id: "linear", label: "Scale Linear" },
    { id: "db", label: "Scale Decibel" },
  ],
  flags: [
    { id: "grid", label: "Toggle Grid" },
    { id: "invertY", label: "Toggle Invert Y" },
    { id: "delta", label: "Toggle Delta" },
  ],
  source: [
    { id: "in", label: "Input" },
    { id: "out", label: "Output" },
    { id: "both", label: "Both" },
  ],
} as const;
