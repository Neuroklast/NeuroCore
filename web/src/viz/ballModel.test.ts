import { describe, expect, it } from "vitest";
import {
  BALL_STATIC,
  buildDust,
  buildPalette,
  buildStrands,
  ballCapable,
  ballDustCount,
  ballDrive,
  ballRadius,
  ballStill,
  rgbTriplet,
  stereoWidth,
  type BallDrive,
} from "./ballModel";

const bands = (low = 0, mid = 0, high = 0) => ({ low, mid, high });

const run = (
  n: number,
  b: ReturnType<typeof bands>,
  rms = 0,
  width = 0,
  motion: "full" | "reduced" = "full",
): BallDrive => {
  let p: BallDrive | null = null;
  for (let i = 0; i < n; i += 1) {
    p = ballDrive(p, b, rms, width, 1 / 60, { motion });
  }
  return p as BallDrive;
};

const settle = (from: BallDrive, n = 300): BallDrive => {
  let p = from;
  for (let i = 0; i < n; i += 1) {
    p = ballDrive(p, bands(), 0, 0, 1 / 60, { motion: "full" });
  }
  return p;
};

describe("ball drive (particles)", () => {
  it("stereo width spreads particles outward, silence drops it", () => {
    const wide = run(20, bands(), 0, 1);
    expect(wide.spread).toBeGreaterThan(0.4);
    expect(settle(wide).spread).toBeLessThan(0.02);
  });

  it("loudness only lifts the rim glow", () => {
    const loud = run(20, bands(), 1, 0);
    expect(loud.glow).toBeGreaterThan(0.25);
    expect(loud.glow).toBeLessThan(0.55);
    expect(loud.spread).toBeLessThan(0.05);
  });

  it("a transient kicks vertically and the kick decays", () => {
    const hit = ballDrive(null, bands(0.5, 0, 0), 0.5, 0, 1 / 60, { motion: "full", transient: true });
    expect(hit.kick).toBeGreaterThan(0.3);
    expect(settle(hit).kick).toBeLessThan(0.02);
  });

  it("bass lifts, treble twinkles", () => {
    expect(run(20, bands(1, 0, 0)).lift).toBeGreaterThan(0.2);
    expect(run(20, bands(0, 0, 1)).twinkle).toBeGreaterThan(0.15);
  });

  it("reduced motion is calmer, not frozen", () => {
    const full = run(40, bands(1, 1, 1), 1, 1, "full");
    const reduced = run(40, bands(1, 1, 1), 1, 1, "reduced");
    expect(reduced.still).toBe(false);
    expect(reduced.lift).toBeLessThan(full.lift);
    expect(reduced.spread).toBeLessThan(full.spread);
  });

  it("motion off is a stand frame", () => {
    expect(ballStill("off")).toBe(true);
    expect(ballDrive({ ...BALL_STATIC, spread: 1 }, bands(1, 1, 1), 1, 1, 1 / 60, { motion: "off" })).toEqual(BALL_STATIC);
  });
});

describe("stereo width from gonio", () => {
  it("is 0 for mid-only and 1 for side-only", () => {
    expect(stereoWidth([0, 0, 0], [1, 1, 1])).toBe(0);
    expect(stereoWidth([1, 1, 1], [0, 0, 0])).toBe(1);
  });
});

describe("ball assets", () => {
  it("builds even harmonics only, deterministically", () => {
    const a = buildStrands();
    expect(a).toEqual(buildStrands());
    expect(a.harmonics).toHaveLength(32);
    expect(a.harmonics.every((h) => h % 2 === 0)).toBe(true);
    expect(a.flareHarmonics).toHaveLength(32);
  });

  it("builds dust inside the unit sphere", () => {
    const a = buildDust(600);
    const b = buildDust(600);
    expect(a.home).toEqual(b.home);
    expect(a.rows).toBe(Math.ceil(600 / 256));
    expect(a.count).toBeGreaterThan(0);
    expect(a.count).toBeLessThanOrEqual(600);
    for (let i = 0; i < a.count; i += 1) {
      const r = Math.hypot(a.home[i * 4]!, a.home[i * 4 + 1]!, a.home[i * 4 + 2]!);
      expect(r).toBeLessThanOrEqual(0.98);
      expect(a.home[i * 4 + 3]).toBeGreaterThanOrEqual(0);
    }
  });

  it("builds a blood-red palette with no white sheen", () => {
    const palette = buildPalette(rgbTriplet("196, 42, 42"));
    expect(palette.tones).toHaveLength(5);
    expect(palette.rimHot[0]).toBeGreaterThan(palette.rimHot[1]!);
    expect(palette.rimHot[1]).toBeLessThan(0.45);
    for (const t of [palette.rimHot, palette.spark, ...palette.tones]) {
      expect(t[0]).toBeGreaterThan(t[1]!);
      expect(t[0]).toBeGreaterThan(t[2]!);
    }
  });
});

describe("ball capability and fixtures", () => {
  it("detects a missing or throwing context", () => {
    expect(ballCapable(() => null)).toBe(false);
    expect(ballCapable(() => ({}))).toBe(true);
    expect(ballCapable(() => {
      throw new Error("no gl");
    })).toBe(false);
  });

  it("parses the theme accent triplet", () => {
    expect(rgbTriplet("196, 42, 42")).toEqual([196 / 255, 42 / 255, 42 / 255]);
    expect(rgbTriplet("nope")).toEqual([1, 1, 1]);
  });

  it("scales the point count with the pane, clamped", () => {
    expect(ballDustCount(0, 0)).toBe(6000);
    expect(ballDustCount(1280, 860)).toBeLessThanOrEqual(40000);
    expect(ballDustCount(1280, 860)).toBeGreaterThan(6000);
  });

  it("scales the ball radius from the short side", () => {
    expect(ballRadius(1256, 500)).toBe(175);
    expect(ballRadius(0, 0)).toBe(8);
  });
});
