import { describe, expect, it } from "vitest";
import { peakToDb } from "../bridge/telemetry";
import {
  buildCableLanes,
  cableGeomStamp,
  cablePaintPass,
  peakDistances,
  tapPeakFractions,
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
  chromaSplit,
  streamGlitch,
  streamIdentityPhase,
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
  it("uses one centered lane until the signal is explicitly split", () => {
    const lanes = edgeLanes("stereo");
    expect(lanes).toHaveLength(1);
    expect(lanes[0]?.offset).toBe(0);
    expect(lanes[0]?.dash).toEqual(MAIN_DASH);
    expect(edgeLanes("mono", "left")[0]).toMatchObject({ id: "L", color: "cyan" });
    expect(edgeLanes("mono", "right")[0]).toMatchObject({ id: "R", color: "accent" });
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
  it("meters sends and buses after their gain, never from unrelated program input", () => {
    const peaks = { IN: 0.9, "bus:dirt": 0.2 };
    expect(peakForLane("mono", "send_dirt", peaks, {}, {}, "send")).toBe(0.2);
    expect(peakForLane("mono", "dirt", peaks, {}, {}, "bus")).toBe(0.2);
    expect(peakForLane("mono", "send_missing", peaks, {}, {}, "send")).toBe(0);
    expect(peakForLane("mono", "xover1", { xover1: 0.8, "xover1:low": 0.1 }, {}, {}, "xover", "low")).toBe(0.1);
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
    expect(chromaSplit(0)).toBe(0);
    expect(chromaSplit(1)).toBeGreaterThan(0);
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

  it("crawls a lamp-lit still tube instead of freezing at the −60 dB floor", () => {
    expect(streamSpeed(0, 0.0005)).toBe(0);
    expect(streamSpeed(0, 0.0012)).toBeGreaterThan(0);
    expect(streamSpeed(0, 1)).toBeGreaterThan(streamSpeed(0, 0.0012));
  });
});

describe("pcb background traces", () => {
  it("keeps traces and glow while the camera pans; beads freeze", () => {
    expect(cablePaintPass(false)).toEqual({ traces: true, glow: true, animate: true });
    expect(cablePaintPass(true)).toEqual({ traces: true, glow: true, animate: false });
  });

  it("keeps glow only in full motion, and stamps geometry so a frame can reuse polylines", () => {
    expect(cablePaintPass(false, "full")).toEqual({ traces: true, glow: true, animate: true });
    expect(cablePaintPass(false, "reduced")).toEqual({ traces: true, glow: false, animate: false });
    expect(cablePaintPass(false, "off")).toEqual({ traces: false, glow: false, animate: false });
    const from = { x: 0, y: 16 };
    const to = { x: 128, y: 16 };
    const route = [from, { x: 64, y: 16 }, to];
    const a = cableGeomStamp("e0", route, from, to, "audio", "out");
    const b = cableGeomStamp("e0", route, from, to, "audio", "out");
    const c = cableGeomStamp("e0", route, from, { x: 160, y: 16 }, "audio", "out");
    expect(a).toBe(b);
    expect(a).not.toBe(c);
    const lanes = buildCableLanes(from, to, route, "audio", "out");
    expect(lanes.length).toBe(1);
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

describe("tap peak beads", () => {
  it("sits beads on local maxima, not a lattice", () => {
    const wave = new Float32Array(9);
    wave[2] = 0.9;
    wave[6] = -0.8;
    const frac = tapPeakFractions(wave);
    expect(frac).toEqual([2 / 8, 6 / 8]);
    expect(tapPeakFractions(new Float32Array(9))).toEqual([]);
  });

  it("maps fractions onto the polyline and wraps travel toward dest", () => {
    const a = peakDistances(200, [0.25, 0.5]);
    expect(a).toEqual([50, 100]);
    const b = peakDistances(200, [0.25, 0.5], 10);
    expect(b[0]).toBeCloseTo(60);
    expect(b[1]).toBeCloseTo(110);
    expect(peakDistances(200, [0.9], 40)[0]).toBeCloseTo(20);
  });

  it("30 and 60 fps travel the same distance", () => {
    expect(streamAdvance(0, 1, 1 / 60) * 2).toBeCloseTo(streamAdvance(0, 1, 1 / 30));
  });
});

describe("overload dB is the real peak", () => {
  it("matches peakToDb when the chip is actually hot", () => {
    expect(peakToDb(1.2)).toBeCloseTo(20 * Math.log10(1.2));
  });
});

it("draws one lane for an unsplit stereo bus", () => {
  expect(edgeLanes("stereo", "out")).toHaveLength(1);
});

it("uses the encoded channel for each MS lane and the actual sidechain tap", () => {
  expect(peakForLane("M", "ms1", { ms1: 0.9 }, { ms1: 0.2 }, { ms1: 0.7 })).toBe(0.2);
  expect(peakForLane("S", "ms1", { ms1: 0.9 }, { ms1: 0.2 }, { ms1: 0.7 })).toBe(0.7);
  expect(peakForLane("sc", "IN", { IN: 0.9, SC: 0.1 }, {}, {}, "in")).toBe(0.1);
  expect(peakForLane("sc", "IN", { IN: 0.9 }, {}, {}, "in")).toBe(0);
});

it("paints Mid and Side on their reserved route without a second geometry rewrite", () => {
  const route = [{x:200,y:80},{x:264,y:80},{x:264,y:176},{x:360,y:176}];
  expect(buildCableLanes(route[0]!, route[3]!, route, "audio", "side"))
    .toEqual(buildCableLanes(route[0]!, route[3]!, route, "audio", "mid"));
});
