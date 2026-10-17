import { specMag01 } from "./scopeModel";

export const ANALYZE_N = 2048;
export const SPEC_BINS = 32;
export const FMIN = 20;
export const LOW_HZ = 250;
export const MID_HZ = 4000;

const ATTACK = 0.05;
const RELEASE = 0.22;

export type Bands = { low: number; mid: number; high: number };

export type AnalyzeOut = {
  spec: number[];
  bands: Bands;
  rms: number;
};

export type AnalyzeState = {
  ring: Float32Array;
  at: number;
  filled: number;
  env: Bands;
};

const ZERO: AnalyzeOut = {
  spec: new Array(SPEC_BINS).fill(0),
  bands: { low: 0, mid: 0, high: 0 },
  rms: 0,
};

export function createAnalyze(): AnalyzeState {
  return {
    ring: new Float32Array(ANALYZE_N),
    at: 0,
    filled: 0,
    env: { low: 0, mid: 0, high: 0 },
  };
}

export function resetAnalyze(s: AnalyzeState): void {
  s.ring.fill(0);
  s.at = 0;
  s.filled = 0;
  s.env = { low: 0, mid: 0, high: 0 };
}

const reWork = new Float32Array(ANALYZE_N);
const imWork = new Float32Array(ANALYZE_N);
const unwrapWork = new Float32Array(ANALYZE_N);
const magWork = new Float32Array(ANALYZE_N / 2);
const hannByN = new Map<number, { w: Float32Array; sum: number }>();

function hann(n: number): { w: Float32Array; sum: number } {
  const hit = hannByN.get(n);
  if (hit) {
    return hit;
  }
  const w = new Float32Array(n);
  const den = Math.max(1, n - 1);
  let sum = 0;
  for (let i = 0; i < n; i += 1) {
    w[i] = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / den);
    sum += w[i]!;
  }
  const row = { w, sum };
  hannByN.set(n, row);
  return row;
}

function fft(re: Float32Array, im: Float32Array, n: number): void {
  let j = 0;
  for (let i = 1; i < n; i += 1) {
    let bit = n >> 1;
    for (; (j & bit) !== 0; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      const tr = re[i]!;
      const ti = im[i]!;
      re[i] = re[j]!;
      im[i] = im[j]!;
      re[j] = tr;
      im[j] = ti;
    }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (-2 * Math.PI) / len;
    const wlenRe = Math.cos(ang);
    const wlenIm = Math.sin(ang);
    const half = len >> 1;
    for (let i = 0; i < n; i += len) {
      let wRe = 1;
      let wIm = 0;
      for (let k = 0; k < half; k += 1) {
        const p = i + k;
        const q = p + half;
        const vr = re[q]! * wRe - im[q]! * wIm;
        const vi = re[q]! * wIm + im[q]! * wRe;
        re[q] = re[p]! - vr;
        im[q] = im[p]! - vi;
        re[p] += vr;
        im[p] += vi;
        const nr = wRe * wlenRe - wIm * wlenIm;
        wIm = wRe * wlenIm + wIm * wlenRe;
        wRe = nr;
      }
    }
  }
}

function magLerp(nHalf: number, k: number): number {
  if (nHalf < 1) {
    return 0;
  }
  const x = Math.max(0, Math.min(nHalf - 1, k));
  const i0 = Math.floor(x);
  const i1 = Math.min(nHalf - 1, i0 + 1);
  const f = x - i0;
  return (magWork[i0] ?? 0) * (1 - f) + (magWork[i1] ?? 0) * f;
}

function pow2le(n: number): number {
  let p = 16;
  while (p * 2 <= n) {
    p *= 2;
  }
  return p;
}

/** One-shot Hann FFT. Log-spaced display bins, dB-mapped 0..1. */
export function analyzeFrame(
  samples: ArrayLike<number>,
  sr: number,
  bins = SPEC_BINS,
): AnalyzeOut {
  const rate = sr > 0 && Number.isFinite(sr) ? sr : 48000;
  const rawN = Math.min(samples.length, ANALYZE_N);
  if (rawN < 16) {
    return { spec: new Array(bins).fill(0), bands: { low: 0, mid: 0, high: 0 }, rms: 0 };
  }
  const n = pow2le(rawN);
  const off = rawN - n;
  const win = hann(n);
  let acc2 = 0;
  for (let i = 0; i < n; i += 1) {
    const x = samples[off + i] ?? 0;
    reWork[i] = x * win.w[i]!;
    imWork[i] = 0;
    acc2 += x * x;
  }
  fft(reWork, imWork, n);
  const nyq = rate * 0.5;
  const logSpan = Math.log(Math.max(nyq / FMIN, 1.0001));
  let lowA = 0;
  let lowN = 0;
  let midA = 0;
  let midN = 0;
  let highA = 0;
  let highN = 0;
  const half = n >> 1;
  const gain = Math.max(win.sum, 1e-6);
  for (let k = 0; k < half; k += 1) {
    const hz = (k * rate) / n;
    const mag = (k === 0 ? 1 : 2) * Math.hypot(reWork[k] ?? 0, imWork[k] ?? 0) / gain;
    magWork[k] = mag;
    if (hz < LOW_HZ) {
      lowA += mag * mag;
      lowN += 1;
    } else if (hz < MID_HZ) {
      midA += mag * mag;
      midN += 1;
    } else {
      highA += mag * mag;
      highN += 1;
    }
  }
  const hzPerBin = rate / n;
  const spec = new Array<number>(bins);
  for (let b = 0; b < bins; b += 1) {
    const hzLo = FMIN * Math.exp(logSpan * (b / bins));
    const hzHi = FMIN * Math.exp(logSpan * ((b + 1) / bins));
    const kLo = hzLo / hzPerBin;
    const kHi = hzHi / hzPerBin;
    let peak = magLerp(half, (kLo + kHi) * 0.5);
    const i0 = Math.max(0, Math.ceil(kLo));
    const i1 = Math.min(half - 1, Math.floor(kHi));
    for (let i = i0; i <= i1; i += 1) {
      const v = magWork[i] ?? 0;
      if (v > peak) {
        peak = v;
      }
    }
    spec[b] = specMag01(peak);
  }
  const band = (acc: number, c: number) => specMag01(c > 0 ? Math.sqrt(acc / c) : 0);
  return {
    spec,
    bands: { low: band(lowA, lowN), mid: band(midA, midN), high: band(highA, highN) },
    rms: Math.sqrt(acc2 / n),
  };
}

export function feedAnalyze(
  s: AnalyzeState,
  samples: ArrayLike<number>,
  sr: number,
  dt: number,
): AnalyzeOut {
  const nIn = samples.length;
  if (nIn < 1) {
    return { spec: new Array(SPEC_BINS).fill(0), bands: s.env, rms: 0 };
  }
  for (let i = 0; i < nIn; i += 1) {
    s.ring[s.at] = samples[i] ?? 0;
    s.at += 1;
    if (s.at >= ANALYZE_N) {
      s.at = 0;
    }
    if (s.filled < ANALYZE_N) {
      s.filled += 1;
    }
  }
  const n = s.filled;
  let i = s.at - n;
  if (i < 0) {
    i += ANALYZE_N;
  }
  for (let k = 0; k < n; k += 1) {
    unwrapWork[k] = s.ring[i]!;
    i += 1;
    if (i >= ANALYZE_N) {
      i = 0;
    }
  }
  const frame = analyzeFrame(unwrapWork.subarray(0, n), sr);
  const tau = Math.max(dt, 0);
  const ease = (cur: number, tgt: number) => {
    const a = tgt > cur ? ATTACK : RELEASE;
    return cur + (tgt - cur) * (1 - Math.exp(-tau / a));
  };
  s.env = {
    low: ease(s.env.low, frame.bands.low),
    mid: ease(s.env.mid, frame.bands.mid),
    high: ease(s.env.high, frame.bands.high),
  };
  return { spec: frame.spec, bands: s.env, rms: frame.rms };
}

export function idleAnalyze(): AnalyzeOut {
  return {
    spec: ZERO.spec.slice(),
    bands: { ...ZERO.bands },
    rms: 0,
  };
}
