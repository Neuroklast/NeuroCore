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

export const LU_LINES = 48;

/** Stacked-line LUFS: how many traces are lit from the floor. */
export function luLitLines(linear: number, n = LU_LINES): number {
  if (! Number.isFinite(linear) || linear <= 0) {
    return 0;
  }
  return Math.max(0, Math.min(n, Math.round((barFillPercent(linear) / 100) * n)));
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

/** Demo L/R for the goniometer. Native telemetry is also raw L/R, not plot x/y. */
export function demoGonioLr(i: number, n: number, t: number): { l: number; r: number } {
  const ph = (i / Math.max(1, n)) * Math.PI * 2 + t * 0.08;
  return { l: Math.sin(ph) * 0.72, r: Math.sin(ph + 0.32) * 0.58 };
}

export function nextProcessMode(mode: string): "STUDIO" | "LIVE" {
  return mode === "LIVE" ? "STUDIO" : "LIVE";
}

export function processModeIndex(mode: string): number {
  return mode === "LIVE" ? 1 : 0;
}

export const SPEC_BINS = 48;
export const SPEC_DEPTH = 56;
export const SPEC_PAD = { l: 56, r: 10, t: 18, b: 22 };

export function specInner(w: number, h: number): { x: number; y: number; w: number; h: number; cx: number } {
  const iw = Math.max(1, w - SPEC_PAD.l - SPEC_PAD.r);
  const ih = Math.max(1, h - SPEC_PAD.t - SPEC_PAD.b);
  return { x: SPEC_PAD.l, y: SPEC_PAD.t, w: iw, h: ih, cx: SPEC_PAD.l + iw * 0.5 };
}

/** Older history is more transparent. Front row is 1. */
export function specRowFade(row: number): number {
  const recede = row / Math.max(1, SPEC_DEPTH - 1);
  return Math.pow(1 - recede, 2);
}

/** Log-Hz ticks on a 20 Hz … Nyquist span, mapped onto SPEC_BINS. */
export function waveTimeMarks(sr: number, nSamples: number, bins = SPEC_BINS): Array<{ bin: number; label: string }> {
  const dur = sr > 0 ? nSamples / sr : 0;
  const ms = dur * 1000;
  const ticks = [0, 0.25, 0.5, 0.75, 1];
  return ticks.map((t) => ({
    bin: Math.round(t * (bins - 1)),
    label: `${(ms * t).toFixed(ms >= 10 ? 0 : 1)}ms`,
  }));
}

export function waveSampleMarks(nSamples: number, bins = SPEC_BINS): Array<{ bin: number; label: string }> {
  const ticks = [0, 0.25, 0.5, 0.75, 1];
  return ticks.map((t) => ({
    bin: Math.round(t * (bins - 1)),
    label: String(Math.round(t * Math.max(0, nSamples - 1))),
  }));
}

function dftMagnitudes(samples: ArrayLike<number>): { mags: number[]; n: number } {
  const n = Math.min(samples.length, 256);
  const half = Math.max(0, Math.floor(n / 2));
  const mags = new Array<number>(half + 1).fill(0);
  if (n < 2) {
    return { mags, n };
  }
  for (let k = 0; k <= half; k += 1) {
    let re = 0;
    let im = 0;
    const w = (-2 * Math.PI * k) / n;
    for (let i = 0; i < n; i += 1) {
      const x = Number(samples[i] ?? 0);
      re += x * Math.cos(w * i);
      im += x * Math.sin(w * i);
    }
    mags[k] = Math.sqrt(re * re + im * im) / n;
  }
  return { mags, n };
}

function magAt(mags: number[], k: number): number {
  if (mags.length < 1) {
    return 0;
  }
  const x = Math.max(0, Math.min(mags.length - 1, k));
  const i0 = Math.floor(x);
  const i1 = Math.min(mags.length - 1, i0 + 1);
  const f = x - i0;
  return (mags[i0] ?? 0) * (1 - f) + (mags[i1] ?? 0) * f;
}

/** Display bins on the same 20 Hz … Nyquist log map as `logFreqMarks`. */
export function logSpectrumBins(samples: ArrayLike<number>, sr: number, bins = SPEC_BINS): number[] {
  const count = Math.max(1, Math.floor(bins));
  const out = new Array<number>(count).fill(0);
  const { mags, n } = dftMagnitudes(samples);
  if (n < 2 || mags.length < 2) {
    return out;
  }
  const nyq = sr > 0 ? sr * 0.5 : 24000;
  const rate = sr > 0 ? sr : 48000;
  const fMin = 20;
  const span = Math.log(nyq / fMin);
  const hzPerBin = rate / n;
  const hzOf = (bin: number) => {
    const t = Math.max(0, Math.min(1, bin / Math.max(1, count - 1)));
    return fMin * Math.exp(span * t);
  };
  for (let b = 0; b < count; b += 1) {
    const kLo = hzOf(b - 0.5) / hzPerBin;
    const kHi = hzOf(b + 0.5) / hzPerBin;
    let peak = magAt(mags, (kLo + kHi) * 0.5);
    const i0 = Math.max(1, Math.ceil(kLo));
    const i1 = Math.min(mags.length - 1, Math.floor(kHi));
    for (let i = i0; i <= i1; i += 1) {
      const v = mags[i] ?? 0;
      if (v > peak) {
        peak = v;
      }
    }
    out[b] = peak;
  }
  return out;
}

/** Frequency always uses dB height so the grid (−72…0) matches the row. */
export function liftScopeMags(
  raw: ArrayLike<number>,
  xScale: ScopeXScale,
  yScale: ScopeYScale,
): number[] {
  const out = new Array<number>(raw.length);
  const asDb = xScale === "freq" || yScale === "db";
  for (let i = 0; i < raw.length; i += 1) {
    const v = Number(raw[i] ?? 0);
    out[i] = asDb ? specMag01(v) : Math.max(0, Math.min(1, Math.abs(v) * 8));
  }
  return out;
}

export function logFreqMarks(sr: number, bins = SPEC_BINS): Array<{ bin: number; hz: number; label: string }> {
  const nyq = sr > 0 ? sr * 0.5 : 24000;
  const fMin = 20;
  const span = Math.log(nyq / fMin);
  const raw = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
  const out: Array<{ bin: number; hz: number; label: string }> = [];
  for (const hz of raw) {
    if (hz < fMin || hz >= nyq) {
      continue;
    }
    const t = Math.log(hz / fMin) / span;
    const bin = Math.max(0, Math.min(bins - 1, Math.round(t * (bins - 1))));
    const label = hz >= 1000 ? `${hz / 1000}k` : `${hz}`;
    if (out.some((m) => m.bin === bin)) {
      continue;
    }
    out.push({ bin, hz, label });
  }
  return out;
}

/** Which spectra to paint. BOTH is IN (cyan) then OUT (accent), never a single mix. */
export function scopeSpectra(source: ScopeSource): Array<"in" | "out"> {
  if (source === "both") {
    return ["in", "out"];
  }
  return source === "in" ? ["in"] : ["out"];
}

/** Always roll IN and OUT history so switching source is not a frozen tail. */
export function scopeRecordIds(): Array<"in" | "out"> {
  return ["in", "out"];
}

/** Linear FFT bin → 0..1 display. Floor −72 dB so a loud hit still fills. */
export function specMag01(linear: number): number {
  if (! Number.isFinite(linear) || linear <= 0) {
    return 0;
  }
  const db = 20 * Math.log10(Math.max(1.0e-8, linear));
  return Math.max(0, Math.min(1, (db + 72) / 72));
}

/** Rising edge that stays positive — skip harmonic/zero chatter that hunts the standing wave. */
export function triggerCut(samples: ArrayLike<number>): number | null {
  const n = samples.length;
  if (n < 10) {
    return null;
  }
  for (let i = 1; i < n - 8; i += 1) {
    const a = Number(samples[i - 1] ?? 0);
    const b = Number(samples[i] ?? 0);
    if (! (a <= 0 && b > 0.02)) {
      continue;
    }
    let sum = 0;
    for (let k = 0; k < 8; k += 1) {
      sum += Number(samples[i + k] ?? 0);
    }
    if (sum > 0.4) {
      return i - 1;
    }
  }
  return null;
}

/** First confirmed rising zero-cross to index 0. Tail is silence — never wrap (no seam). */
export function triggerAlign(samples: ArrayLike<number>): Float32Array {
  const n = samples.length;
  const out = new Float32Array(n);
  if (n < 3) {
    for (let i = 0; i < n; i += 1) {
      out[i] = Number(samples[i] ?? 0);
    }
    return out;
  }
  const cut = triggerCut(samples) ?? 0;
  const keep = n - cut;
  for (let i = 0; i < keep; i += 1) {
    out[i] = Number(samples[cut + i] ?? 0);
  }
  return out;
}

export function shouldResetScopeHist(
  prev: ScopeXScale,
  next: ScopeXScale,
  prevY?: ScopeYScale,
  nextY?: ScopeYScale,
): boolean {
  const kind = (x: ScopeXScale) => (x === "freq" ? "freq" : "wave");
  if (kind(prev) !== kind(next)) {
    return true;
  }
  return prevY !== undefined && nextY !== undefined && prevY !== nextY;
}

export function shouldPushScopeRow(tick: number, lastTick: number): boolean {
  return tick !== lastTick;
}

export function scopeYMarks(
  xScale: ScopeXScale,
  yScale: ScopeYScale,
): Array<{ mag: number; label: string }> {
  if (xScale === "freq" || yScale === "db") {
    return specDbMarks();
  }
  return [
    { mag: 1, label: "+1" },
    { mag: 0.5, label: "0" },
    { mag: 0, label: "−1" },
  ];
}

export function isWaveScope(xScale: ScopeXScale): boolean {
  return xScale !== "freq";
}

/** Time/sample 3D row from the live buffer. No trigger lock — that froze the wave. */
export function standingWaveRow(
  samples: ArrayLike<number>,
  bins = SPEC_BINS,
  yScale: ScopeYScale = "linear",
): number[] {
  const n = Math.max(1, samples.length);
  const row = new Array<number>(bins);
  for (let b = 0; b < bins; b += 1) {
    const i0 = Math.floor((b / Math.max(1, bins - 1)) * (n - 1));
    const i1 = Math.min(n - 1, i0 + 1);
    const f = ((b / Math.max(1, bins - 1)) * (n - 1)) - i0;
    const x = Number(samples[i0] ?? 0) * (1 - f) + Number(samples[i1] ?? 0) * f;
    if (yScale === "db") {
      row[b] = specMag01(Math.abs(x));
    } else {
      row[b] = Math.max(0, Math.min(1, 0.5 + 0.5 * x));
    }
  }
  return row;
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

/**
 * Camera on the horizontal midpoint, pitched down, wide FOV.
 * Near row is the front floor; far rows recede toward the vanishing point.
 */
export function spectrogramProject(
  bin: number,
  row: number,
  mag: number,
  w: number,
  h: number,
): { x: number; y: number } {
  const inner = specInner(w, h);
  const recede = row / Math.max(1, SPEC_DEPTH - 1);
  const nearHalf = inner.w * 0.46;
  const farHalf = inner.w * 0.34;
  const half = nearHalf + (farHalf - nearHalf) * recede;
  const t = bin / Math.max(1, SPEC_BINS - 1);
  const x = inner.cx + (t - 0.5) * 2 * half;
  const nearY = inner.y + inner.h * 0.90;
  const farY = inner.y + inner.h * 0.32;
  const floor = nearY + (farY - nearY) * recede;
  const rise = mag * inner.h * 0.28 * (1 - recede * 0.28);
  return { x, y: floor - rise };
}

/** Same camera as the spectrogram: mag 0 is the floor (−1), mag 1 is +peak. Never below the plane. */
export function waveProject(
  bin: number,
  row: number,
  mag: number,
  w: number,
  h: number,
): { x: number; y: number } {
  return spectrogramProject(bin, row, mag, w, h);
}

/** dB ticks on the near-plane Y axis. Floor is −72 dB (specMag01). */
export function specDbMarks(): Array<{ mag: number; label: string }> {
  return [
    { mag: 1, label: "0" },
    { mag: 48 / 72, label: "-24" },
    { mag: 24 / 72, label: "-48" },
    { mag: 0, label: "-72" },
  ];
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
