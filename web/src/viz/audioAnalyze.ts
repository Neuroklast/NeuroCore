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
  const re = new Float32Array(n);
  const im = new Float32Array(n);
  let winSum = 0;
  let acc2 = 0;
  const den = Math.max(1, n - 1);
  for (let i = 0; i < n; i += 1) {
    const w = 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / den);
    const x = samples[off + i] ?? 0;
    re[i] = x * w;
    winSum += w;
    acc2 += x * x;
  }
  fft(re, im, n);
  const nyq = rate * 0.5;
  const logSpan = Math.log(Math.max(nyq / FMIN, 1.0001));
  const peak = new Array<number>(bins).fill(0);
  let lowA = 0;
  let lowN = 0;
  let midA = 0;
  let midN = 0;
  let highA = 0;
  let highN = 0;
  const half = n >> 1;
  const gain = Math.max(winSum, 1e-6);
  for (let k = 0; k < half; k += 1) {
    const hz = (k * rate) / n;
    const mag = (k === 0 ? 1 : 2) * Math.hypot(re[k] ?? 0, im[k] ?? 0) / gain;
    const t = hz <= FMIN ? 0 : Math.log(hz / FMIN) / logSpan;
    const b = Math.max(0, Math.min(bins - 1, Math.floor(t * bins)));
    if (mag > (peak[b] ?? 0)) {
      peak[b] = mag;
    }
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
  const spec = peak.map((v) => specMag01(v));
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
  const buf = new Float32Array(n);
  let i = s.at - n;
  if (i < 0) {
    i += ANALYZE_N;
  }
  for (let k = 0; k < n; k += 1) {
    buf[k] = s.ring[i]!;
    i += 1;
    if (i >= ANALYZE_N) {
      i = 0;
    }
  }
  const frame = analyzeFrame(buf, sr);
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
