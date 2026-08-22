import { describe, expect, it } from "vitest";
import { peakToDb } from "../bridge/telemetry";
import {
  edgeLanes,
  edgePaintKind,
  MAIN_DASH,
  parallelOffset,
  gridHash,
  visibleBlockRange,
  peakForLane,
  rmsForLane,
  SIDE_DASH,
  sideBreakaway,
  STEREO_GAP,
  STEREO_OFFSET,
  STREAM_ALPHA_HOT,
  STREAM_ALPHA_STILL,
  STREAM_BLUR_HOT,
  STREAM_BLUR_STILL,
  STREAM_GAP_HOT,
  STREAM_GAP_STILL,
  STREAM_PX,
  streamAlpha,
  streamBlur,
  streamDash,
  streamGlitch,
  streamSpeed,
  twinsCross,
} from "./cablePaint";
import { chamferWaypoints, hasLightning } from "./layout/chamfer";

describe("edgePaintKind", () => {
  it("treats a combined audio out as a stereo bus, split jacks as mid/side or mono", () => {
    expect(edgePaintKind({ jackId: "out", kind: "audio" }, { type: "stage" })).toBe("stereo");
    expect(edgePaintKind({ jackId: "out", kind: "audio" }, { type: "in" })).toBe("stereo");
    expect(edgePaintKind({ jackId: "mid", kind: "audio" }, { type: "ms", args: { mode: "encode" } })).toBe("mid");
    expect(edgePaintKind({ jackId: "side", kind: "audio" }, { type: "ms", args: { mode: "split" } })).toBe("side");
    expect(edgePaintKind({ jackId: "left", kind: "audio" }, { type: "ms", args: { family: "lr" } })).toBe("mono");
    expect(edgePaintKind({ jackId: "right", kind: "audio" }, { type: "ms", args: { family: "lr" } })).toBe("mono");
    expect(edgePaintKind({ jackId: "mid", kind: "audio" }, { type: "msplit" })).toBe("mono");
    expect(edgePaintKind({ jackId: "mod", kind: "mod" }, { type: "osc" })).toBe("mod");
    expect(edgePaintKind({ jackId: "sc", kind: "sc" }, { type: "in" })).toBe("sc");
  });
});

describe("stereo bus paint", () => {
  it("uses one route and two normal offsets 8 px apart", () => {
    const lanes = edgeLanes("stereo");
    expect(lanes).toHaveLength(2);
    expect(lanes[0]?.offset).toBe(-STEREO_OFFSET);
    expect(lanes[1]?.offset).toBe(STEREO_OFFSET);
    expect(STEREO_GAP).toBe(8);
    expect(lanes[0]?.dash).toEqual(MAIN_DASH);
    expect(lanes[1]?.color).toBe("accent");
    expect(lanes[0]?.color).toBe("cyan");
    const line = [
      { x: 0, y: 80 },
      { x: 120, y: 80 },
    ];
    const L = parallelOffset(line, -STEREO_OFFSET);
    const R = parallelOffset(line, STEREO_OFFSET);
    expect(L[0]?.y).toBeCloseTo(76);
    expect(R[0]?.y).toBeCloseTo(84);
    expect(twinsCross(L, R)).toBe(false);
  });

  it("keeps twin lanes from crossing on a 45° chamfer", () => {
    const route = chamferWaypoints([
      { x: 16, y: 80 },
      { x: 80, y: 80 },
      { x: 80, y: 208 },
      { x: 240, y: 208 },
    ]);
    const L = parallelOffset(route, -STEREO_OFFSET);
    const R = parallelOffset(route, STEREO_OFFSET);
    expect(hasLightning(route)).toBe(false);
    expect(twinsCross(L, R)).toBe(false);
  });
});

describe("mid/side vs L/R language", () => {
  it("paints mid thicker and side with the short dash", () => {
    const mid = edgeLanes("mid");
    const side = edgeLanes("side");
    expect(mid).toHaveLength(1);
    expect(side).toHaveLength(1);
    expect(mid[0]?.width).toBeGreaterThan(side[0]!.width);
    expect(side[0]?.dash).toEqual(SIDE_DASH);
    expect(mid[0]?.dash).toEqual(MAIN_DASH);
  });

  it("breaks the side path 45° off the port without a lightning bolt", () => {
    const route = [
      { x: 200, y: 80 },
      { x: 232, y: 80 },
      { x: 232, y: 160 },
      { x: 320, y: 160 },
    ];
    const painted = sideBreakaway(route);
    expect(painted[0]).toEqual(route[0]);
    expect(hasLightning(painted)).toBe(false);
  });
});

describe("lane telemetry", () => {
  it("reads L and R separately so a hard pan left dims the right lane", () => {
    const clips = { stage1: 0.8 };
    const clipsL = { stage1: 0.8 };
    const clipsR = { stage1: 0 };
    expect(peakForLane("L", "stage1", clips, clipsL, clipsR)).toBeCloseTo(0.8);
    expect(peakForLane("R", "stage1", clips, clipsL, clipsR)).toBeCloseTo(0);
    expect(rmsForLane("L", "stage1", { stage1: 0.5 }, { stage1: 0.5 }, { stage1: 0 })).toBeCloseTo(0.5);
    expect(rmsForLane("R", "stage1", { stage1: 0.5 }, { stage1: 0.5 }, { stage1: 0 })).toBeCloseTo(0);
  });

  it("maps RMS to gap and speed, peak to glow, and glitch only above 0 dBFS", () => {
    expect(streamSpeed(0)).toBe(0);
    expect(streamSpeed(1)).toBe(STREAM_PX);
    expect(streamSpeed(0.5)).toBeGreaterThan(0);
    expect(streamSpeed(0.5)).toBeLessThan(STREAM_PX);
    expect(streamAlpha(0)).toBeCloseTo(STREAM_ALPHA_STILL);
    expect(streamAlpha(1)).toBeCloseTo(STREAM_ALPHA_HOT);
    expect(streamBlur(0)).toBe(STREAM_BLUR_STILL);
    expect(streamBlur(1)).toBe(STREAM_BLUR_HOT);
    expect(streamGlitch(1)).toBe(0);
    expect(streamGlitch(1.2)).toBeGreaterThan(0);
    const still = streamDash(MAIN_DASH, 0);
    const hot = streamDash(MAIN_DASH, 1);
    const gap = (d: number[]) => d.filter((_, i) => i % 2 === 1).reduce((s, g) => s + g, 0) / 3;
    expect(still[0]).toBe(MAIN_DASH[0]);
    expect(hot[0]).toBe(MAIN_DASH[0]);
    expect(gap(still)).toBeCloseTo(STREAM_GAP_STILL);
    expect(gap(hot)).toBeCloseTo(STREAM_GAP_HOT);
    expect(gap(still)).toBeGreaterThan(gap(hot));
  });
});

describe("pcb background traces", () => {
  it("gates cells by a pan-stable hash and culls to the camera", () => {
    expect(gridHash(3, 5)).toBe(gridHash(3, 5));
    expect(gridHash(3, 5)).not.toBe(gridHash(4, 5));
    const a = visibleBlockRange({ tx: 0, ty: 0, scale: 1, width: 256, height: 256 });
    expect(a.i1 - a.i0).toBeLessThanOrEqual(4);
    const b = visibleBlockRange({ tx: -128, ty: 0, scale: 1, width: 256, height: 256 });
    expect(b.i0).toBeGreaterThan(a.i0);
  });
});

describe("overload dB is the real peak", () => {
  it("matches peakToDb when the chip is actually hot", () => {
    expect(peakToDb(1.2)).toBeCloseTo(20 * Math.log10(1.2));
  });
});
