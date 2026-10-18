import {
  analyzeFrame,
  createAnalyze,
  feedAnalyze,
  idleAnalyze,
  resetAnalyze,
  type AnalyzeState,
} from "./audioAnalyze";
import { perfBegin, perfEnd } from "../perf/marks";
import { BALL_STATIC, type BallDrive } from "./ballModel";
import { CONTAINMENT_STATIC, type ContainmentPose } from "./containmentModel";

export type VizSnap = {
  drive: BallDrive;
  pose: ContainmentPose;
  telTick: number;
};

const IDLE: VizSnap = { drive: BALL_STATIC, pose: CONTAINMENT_STATIC, telTick: -1 };

let snap: VizSnap = IDLE;

export function publishViz(drive: BallDrive, pose: ContainmentPose, telTick: number): void {
  snap = { drive, pose, telTick };
}

export function readViz(): VizSnap {
  return snap;
}

type BandCache = {
  tick: number;
  sr: number;
  live: boolean;
  bands: { low: number; mid: number; high: number };
  spec: number[];
};

let bandCache: BandCache = {
  tick: -1,
  sr: 0,
  live: false,
  bands: { low: 0, mid: 0, high: 0 },
  spec: new Array(32).fill(0),
};

let analyzer: AnalyzeState = createAnalyze();

/** One analyze hop per telemetry tick. Shared by the ball and the HUD. */
export function cachedBands(
  tick: number,
  samples: ArrayLike<number>,
  sr: number,
  live: boolean,
): { bands: { low: number; mid: number; high: number }; spec: number[] } {
  const rate = sr > 0 && Number.isFinite(sr) ? sr : 48000;
  if (tick === bandCache.tick && live === bandCache.live && rate === bandCache.sr) {
    return bandCache;
  }
  if (! live) {
    resetAnalyze(analyzer);
    const idle = idleAnalyze();
    bandCache = { tick, sr: rate, live, bands: idle.bands, spec: idle.spec };
    return bandCache;
  }
  perfBegin("bands");
  const out = feedAnalyze(analyzer, samples, rate, 1 / 45);
  let spec = bandCache.spec;
  if (spec.length !== out.spec.length) {
    spec = out.spec.slice();
  } else {
    for (let i = 0; i < out.spec.length; i += 1) {
      spec[i] = out.spec[i]!;
    }
  }
  bandCache = { tick, sr: rate, live, bands: out.bands, spec };
  perfEnd("bands");
  return bandCache;
}

// The dry input drives motion independently of effects which remove sub energy.
let inputCache = { tick: -1, sr: 0, live: false, bands: { low: 0, mid: 0, high: 0 } };
export function cachedInputBands(tick: number, samples: ArrayLike<number>, sr: number, live: boolean) {
  if (tick !== inputCache.tick || sr !== inputCache.sr || live !== inputCache.live) {
    inputCache = { tick, sr, live, bands: live ? analyzeFrame(samples, sr).bands : { low: 0, mid: 0, high: 0 } };
  }
  return inputCache.bands;
}
