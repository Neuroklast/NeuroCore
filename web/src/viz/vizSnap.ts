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
  live: boolean;
  bands: { low: number; mid: number; high: number };
  spec: number[];
};

let bandCache: BandCache = {
  tick: -1,
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
  if (tick === bandCache.tick && live === bandCache.live) {
    return bandCache;
  }
  if (! live) {
    resetAnalyze(analyzer);
    bandCache = { tick, live, ...idleAnalyze() };
    return bandCache;
  }
  perfBegin("bands");
  const out = feedAnalyze(analyzer, samples, sr, 1 / 45);
  bandCache = { tick, live, bands: out.bands, spec: out.spec };
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
