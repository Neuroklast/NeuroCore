import { describe, expect, it } from "vitest";
import { logoSliceFrame, LOGO_SLICE_COUNT } from "./logoSlices";

describe("logo slice motion", () => {
  it("reassembles the complete logo between short glitches", () => {
    const frame = logoSliceFrame(1600, 0, "full");
    expect(frame).toHaveLength(LOGO_SLICE_COUNT);
    expect(frame.every((s) => s.x === 0 && s.opacity === 1)).toBe(true);
    expect(frame[0]!.top).toBe(0);
    expect(frame.at(-1)!.bottom).toBe(0);
    for (let i = 1; i < frame.length; i++) {
      expect(frame[i]!.top).toBeCloseTo(100 - frame[i - 1]!.bottom);
    }
  });

  it("shifts horizontal bands independently and responds more strongly to audio", () => {
    const idle = logoSliceFrame(2580, 0, "full");
    const loud = logoSliceFrame(2580, 1, "full");
    expect(new Set(idle.map((s) => s.x)).size).toBeGreaterThan(3);
    expect(loud.reduce((n, s) => n + Math.abs(s.x), 0)).toBeGreaterThan(idle.reduce((n, s) => n + Math.abs(s.x), 0));
    expect(loud.every((s) => Math.abs(s.x) <= 18 && s.opacity >= 0 && s.opacity <= 1)).toBe(true);
    expect(logoSliceFrame(1600, 1, "full", 1).some((s) => s.x !== 0)).toBe(true);
  });

  it("disables displacement and entry effects in reduced and off modes", () => {
    for (const mode of ["reduced", "off"] as const) {
      for (const time of [0, 100, 2580]) {
        expect(logoSliceFrame(time, 1, mode).every((s) => s.x === 0 && s.opacity === 1)).toBe(true);
      }
    }
    expect(logoSliceFrame(100, 0, "full").some((s) => s.opacity < 1)).toBe(true);
    expect(logoSliceFrame(2580, Number.NaN, "full").every((s) => Number.isFinite(s.x))).toBe(true);
  });
});
