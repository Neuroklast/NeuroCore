import { DISPLAY_AMP_FLOOR, peakToDb } from "../bridge/telemetry";
import { LOW_HZ, MID_HZ, analyzeFrame, type Bands } from "./audioAnalyze";

export type SignalSourceId = "plugin.in" | "plugin.out" | `node.${string}`;
export type FeatureValidity = "live" | "preview" | "unavailable" | "stale";
export type BandId = "low" | "mid" | "high";

export const BAND_EDGES = { low: LOW_HZ, mid: MID_HZ } as const;

export type AudioFeatures = {
  source: SignalSourceId;
  validity: FeatureValidity;
  peak: number;
  rms: number;
  crestDb: number | null;
  bands: Bands;
  spec: number[];
  balance: number;
  corr: number;
  midEnergy: number;
  sideEnergy: number;
  transient: number;
};

export const FEATURES_UNAVAILABLE: AudioFeatures = {
  source: "plugin.out",
  validity: "unavailable",
  peak: 0,
  rms: 0,
  crestDb: null,
  bands: { low: 0, mid: 0, high: 0 },
  spec: new Array(32).fill(0),
  balance: 0,
  corr: 0,
  midEnergy: 0,
  sideEnergy: 0,
  transient: 0,
};

export function peakAbs(samples: ArrayLike<number>): number {
  let p = 0;
  for (let i = 0; i < samples.length; i += 1) {
    const a = Math.abs(samples[i] ?? 0);
    if (Number.isFinite(a) && a > p) {
      p = a;
    }
  }
  return p;
}

export function rmsAbs(samples: ArrayLike<number>): number {
  const n = samples.length;
  if (n < 1) {
    return 0;
  }
  let acc = 0;
  for (let i = 0; i < n; i += 1) {
    const x = samples[i] ?? 0;
    acc += x * x;
  }
  return Math.sqrt(acc / n);
}

/** Peak minus RMS in dB over the same window. Silence has no crest. */
export function crestFactorDb(peak: number, rms: number): number | null {
  if (! (peak > DISPLAY_AMP_FLOOR) || ! (rms > DISPLAY_AMP_FLOOR)) {
    return null;
  }
  return peakToDb(peak) - peakToDb(rms);
}

/** L/R balance −1 (left) … +1 (right). Silence is 0, never a random pan. */
export function stereoBalance(peakL: number, peakR: number): number {
  const l = Number.isFinite(peakL) ? Math.max(0, peakL) : 0;
  const r = Number.isFinite(peakR) ? Math.max(0, peakR) : 0;
  const s = l + r;
  if (! (s > DISPLAY_AMP_FLOOR)) {
    return 0;
  }
  return (r - l) / s;
}

export function midSideEnergy(gonioX: ArrayLike<number>, gonioY: ArrayLike<number>): {
  mid: number;
  side: number;
  peakL: number;
  peakR: number;
  corr: number;
} {
  const n = Math.min(gonioX.length, gonioY.length);
  let mid = 0;
  let side = 0;
  let peakL = 0;
  let peakR = 0;
  let accLr = 0;
  let accL2 = 0;
  let accR2 = 0;
  for (let i = 0; i < n; i += 1) {
    const x = gonioX[i] ?? 0;
    const y = gonioY[i] ?? 0;
    side += x * x;
    mid += y * y;
    const l = x - y;
    const r = -x - y;
    const al = Math.abs(l);
    const ar = Math.abs(r);
    if (al > peakL) {
      peakL = al;
    }
    if (ar > peakR) {
      peakR = ar;
    }
    accLr += l * r;
    accL2 += l * l;
    accR2 += r * r;
  }
  const den = Math.sqrt(accL2 * accR2);
  const corr = den > 1e-12 ? Math.max(-1, Math.min(1, accLr / den)) : 0;
  return { mid, side, peakL, peakR, corr };
}

export function dominantBand(bands: Bands): BandId {
  if (bands.low >= bands.mid && bands.low >= bands.high) {
    return "low";
  }
  if (bands.mid >= bands.high) {
    return "mid";
  }
  return "high";
}

export function measureFeatures(
  samples: ArrayLike<number>,
  gonioX: ArrayLike<number>,
  gonioY: ArrayLike<number>,
  sr: number,
  opts: { source?: SignalSourceId; validity?: FeatureValidity; transient?: number } = {},
): AudioFeatures {
  const validity = opts.validity ?? "live";
  if (validity === "unavailable" || validity === "stale") {
    return { ...FEATURES_UNAVAILABLE, source: opts.source ?? "plugin.out", validity };
  }
  const peak = peakAbs(samples);
  const rms = rmsAbs(samples);
  const packed = samples.length >= 16 ? analyzeFrame(samples, sr) : {
    spec: new Array(32).fill(0),
    bands: { low: 0, mid: 0, high: 0 },
    rms,
  };
  const ms = midSideEnergy(gonioX, gonioY);
  return {
    source: opts.source ?? "plugin.out",
    validity,
    peak,
    rms,
    crestDb: crestFactorDb(peak, rms),
    bands: packed.bands,
    spec: packed.spec,
    balance: stereoBalance(ms.peakL, ms.peakR),
    corr: ms.corr,
    midEnergy: ms.mid,
    sideEnergy: ms.side,
    transient: Math.max(0, Math.min(1, opts.transient ?? 0)),
  };
}

export function formatUnavailable(validity: FeatureValidity, liveText: string): string {
  if (validity === "live" || validity === "preview") {
    return liveText;
  }
  return "—";
}
