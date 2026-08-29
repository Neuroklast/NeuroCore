import { describe, expect, it } from "vitest";
import { peakToDb } from "../bridge/telemetry";
import {
  buildCableLanes,
  cableGeomStamp,
  cablePaintPass,
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
  streamAdvance,
  streamDashOffset,
  streamGlitch,
  streamIdentityPhase,
  streamSpeed,
  twinsCross,
  packetDistances,
  packetGapAt,
  packetMeanGap,
  packetSeed,
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
  it("runs send and bus tubes from the IN tap — those chips have no DSP meter", () => {
    const clips = { IN: 0.4, __in__: 0.4, stage1: 0.2 };
    const clipsL = { IN: 0.4, __in__: 0.4 };
    const clipsR = { IN: 0.35, __in__: 0.35 };
    expect(peakForLane("L", "send", clips, clipsL, clipsR, "send")).toBeCloseTo(0.4);
    expect(peakForLane("R", "send", clips, clipsL, clipsR, "send")).toBeCloseTo(0.35);
    expect(peakForLane("L", "dirt", clips, clipsL, clipsR, "bus")).toBeCloseTo(0.4);
    expect(rmsForLane("mono", "send", { IN: 0.22 }, { IN: 0.22 }, { IN: 0.22 }, "send")).toBeCloseTo(0.22);
    expect(peakForLane("L", "send", { stage1: 0.9 }, { stage1: 0.9 }, { stage1: 0.9 }, "send")).toBe(0);
  });

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
    expect(streamSpeed(1)).toBeGreaterThan(0);
    expect(streamSpeed(1)).toBeLessThan(STREAM_PX * 60);
    expect(streamSpeed(0.5)).toBeGreaterThan(0);
    expect(streamSpeed(0.5)).toBeLessThan(STREAM_PX * 60);
    expect(streamAdvance(0, 1, 1 / 60) * 2).toBeCloseTo(streamAdvance(0, 1, 1 / 30));
    expect(streamAdvance(-10, 0, 1 / 60)).toBe(-10);
    expect(streamAdvance(0, 1, 1 / 60)).toBeGreaterThan(0);
    expect(streamAdvance(0, 1, 1)).toBeGreaterThan(streamAdvance(0, 0.25, 1));
    expect(streamSpeed(0, 0.5)).toBeGreaterThan(0);
    expect(streamSpeed(0, 0)).toBe(0);
    expect(Math.abs(streamDashOffset(1000, 0.9))).toBeGreaterThan(Math.abs(streamDashOffset(1000, 0.4)));
    expect(streamDashOffset(2000, 1)).toBeGreaterThan(streamDashOffset(1000, 1));
    expect(streamDashOffset(1000, 0, 0)).toBe(0);
    expect(streamAlpha(0)).toBeCloseTo(STREAM_ALPHA_STILL);
    expect(streamAlpha(1)).toBeCloseTo(STREAM_ALPHA_HOT);
    expect(streamBlur(0)).toBe(STREAM_BLUR_STILL);
    expect(streamBlur(1)).toBe(STREAM_BLUR_HOT);
    expect(streamGlitch(1)).toBe(0);
    expect(streamGlitch(1.2)).toBeGreaterThan(0);
    expect(streamIdentityPhase("e0:L", 30)).not.toBe(streamIdentityPhase("e0:R", 30));
    expect(streamIdentityPhase("stage1:L", 30)).not.toBe(streamIdentityPhase("filter1:L", 30));
    expect(streamIdentityPhase("e0:L", 30)).toBe(streamIdentityPhase("e0:L", 30));
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
  it("drops traces and glow while the camera is moving", () => {
    expect(cablePaintPass(false)).toEqual({ traces: true, glow: true });
    expect(cablePaintPass(true)).toEqual({ traces: false, glow: false });
  });

  it("keeps glow only in full motion, and stamps geometry so a frame can reuse polylines", () => {
    expect(cablePaintPass(false, "full")).toEqual({ traces: true, glow: true });
    expect(cablePaintPass(false, "reduced")).toEqual({ traces: true, glow: false });
    expect(cablePaintPass(false, "off")).toEqual({ traces: false, glow: false });
    const from = { x: 0, y: 16 };
    const to = { x: 128, y: 16 };
    const route = [from, { x: 64, y: 16 }, to];
    const a = cableGeomStamp("e0", route, from, to, "audio", "out");
    const b = cableGeomStamp("e0", route, from, to, "audio", "out");
    const c = cableGeomStamp("e0", route, from, { x: 160, y: 16 }, "audio", "out");
    expect(a).toBe(b);
    expect(a).not.toBe(c);
    const lanes = buildCableLanes(from, to, route, "audio", "out");
    expect(lanes.length).toBe(2);
    expect(lanes[0]![0]).toEqual(expect.objectContaining({ x: from.x }));
    const last = lanes[0]![lanes[0]!.length - 1]!;
    expect(last.x).toBe(to.x);
    expect(Math.max(...lanes[0]!.map((p) => p.x))).toBeLessThanOrEqual(to.x);
  });

  it("never paints a dest-first route so packets cannot leave OUT east", () => {
    const from = { x: 0, y: 80 };
    const to = { x: 200, y: 80 };
    const backwards = [to, { x: 120, y: 80 }, { x: 40, y: 80 }, from];
    const lanes = buildCableLanes(from, to, backwards, "audio", "out");
    const pts = lanes[0]!;
    expect(pts[0]!.x).toBe(from.x);
    expect(pts[pts.length - 1]!.x).toBe(to.x);
    expect(Math.max(...pts.map((p) => p.x))).toBeLessThanOrEqual(to.x);
  });

  it("gates cells by a pan-stable hash and culls to the camera", () => {
    expect(gridHash(3, 5)).toBe(gridHash(3, 5));
    expect(gridHash(3, 5)).not.toBe(gridHash(4, 5));
    const a = visibleBlockRange({ tx: 0, ty: 0, scale: 1, width: 256, height: 256 });
    expect(a.i1 - a.i0).toBeLessThanOrEqual(4);
    const b = visibleBlockRange({ tx: -128, ty: 0, scale: 1, width: 256, height: 256 });
    expect(b.i0).toBeGreaterThan(a.i0);
  });
});

describe("aperiodic packet train", () => {
  it("does not repeat a short spacing pattern so flow cannot look reversed", () => {
    const mean = 16;
    const seed = packetSeed("e0:L");
    const gaps = Array.from({ length: 24 }, (_, k) => packetGapAt(k, seed, mean));
    const period2 = gaps.every((g, i) => Math.abs(g - gaps[i % 2]!) < 0.05);
    expect(period2).toBe(false);
    for (let i = 1; i < gaps.length; i += 1) {
      expect(Math.abs(gaps[i]! - gaps[i - 1]!)).toBeGreaterThan(1);
    }
    expect(Math.max(...gaps) - Math.min(...gaps)).toBeGreaterThan(mean * 0.4);
    const onCable = packetDistances(220, 0, mean, seed);
    const spacings = onCable.slice(1).map((d, i) => d - onCable[i]!);
    const unique = new Set(spacings.map((g) => Math.round(g * 4)));
    expect(unique.size).toBeGreaterThan(2);
  });

  it("translates the train toward dest when travel increases", () => {
    const mean = 16;
    const seed = 0.31;
    const a = packetDistances(240, 10, mean, seed);
    const b = packetDistances(240, 14, mean, seed);
    const interior = a.filter((d) => d > 8 && d < 220);
    expect(interior.length).toBeGreaterThan(4);
    for (const d of interior) {
      expect(b.some((x) => Math.abs(x - (d + 4)) < 0.51)).toBe(true);
    }
  });

  it("packs denser when RMS is hot, never a 2 px lattice", () => {
    expect(packetMeanGap(0)).toBeGreaterThan(packetMeanGap(1));
    expect(packetMeanGap(1)).toBeGreaterThan(8);
  });

  it("keeps L and R on different trains", () => {
    const L = packetDistances(200, 0, 16, packetSeed("e0:L"));
    const R = packetDistances(200, 0, 16, packetSeed("e0:R"));
    expect(L).not.toEqual(R);
  });

  it("advances less than half the smallest gap per 60 fps frame so the eye cannot reverse", () => {
    const mean = packetMeanGap(1);
    const gaps = Array.from({ length: 32 }, (_, k) => packetGapAt(k, 0.2, mean));
    const smallest = Math.min(...gaps);
    const step = streamSpeed(1) / 60;
    expect(step).toBeLessThan(smallest * 0.5);
    expect(streamAdvance(0, 1, 1 / 60) * 2).toBeCloseTo(streamAdvance(0, 1, 1 / 30));
  });
});

describe("overload dB is the real peak", () => {
  it("matches peakToDb when the chip is actually hot", () => {
    expect(peakToDb(1.2)).toBeCloseTo(20 * Math.log10(1.2));
  });
});
