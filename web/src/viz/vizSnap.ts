import { bandRms, spectrumBins } from "../face/faceModel";
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

/** DFT once per telemetry tick. Shared by the ball and the HUD. */
export function cachedBands(
  tick: number,
  samples: ArrayLike<number>,
  sr: number,
  live: boolean,
): { bands: { low: number; mid: number; high: number }; spec: number[] } {
  if (tick === bandCache.tick && live === bandCache.live) {
    return bandCache;
  }
  const spec = live && samples.length >= 2
    ? spectrumBins(samples, 32).map((v) => (v > 1 ? 1 : v < 0 ? 0 : v))
    : new Array(32).fill(0);
  const bands = live ? bandRms(samples, sr) : { low: 0, mid: 0, high: 0 };
  bandCache = { tick, live, bands, spec };
  return bandCache;
}
