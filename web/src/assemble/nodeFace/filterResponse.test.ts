import { describe, expect, it } from "vitest";
import {
  eqMag,
  filterCurvePath,
  filterFaceModel,
  filterMags,
  magToDb,
  svfMag,
} from "./filterResponse";

describe("calculated filter response", () => {
  it("moves a lowpass cutoff when the ChipSpec Hz changes", () => {
    const low = filterMags("filter", { type: "lowpass", cutoff: "400", resonance: "0.7" });
    const high = filterMags("filter", { type: "lowpass", cutoff: "4000", resonance: "0.7" });
    expect(magToDb(high[24]!)).toBeGreaterThan(magToDb(low[24]!));
    expect(filterCurvePath("filter", { type: "lowpass", cutoff: "400", resonance: "0.7" }))
      .not.toBe(filterCurvePath("filter", { type: "lowpass", cutoff: "4000", resonance: "0.7" }));
    expect(filterFaceModel("filter", { type: "lowpass", cutoff: "900", resonance: "0.3" })).toEqual({
      family: "filter",
      mode: "lowpass",
      fc: 900,
      q: 0.3,
      gainDb: 0,
    });
  });

  it("paints highpass opposite a lowpass, and a resonance peak at cutoff", () => {
    expect(svfMag("lowpass", 200, 1000, 0.7)).toBeGreaterThan(svfMag("lowpass", 8000, 1000, 0.7));
    expect(svfMag("highpass", 8000, 1000, 0.7)).toBeGreaterThan(svfMag("highpass", 200, 1000, 0.7));
    expect(svfMag("bandpass", 1000, 1000, 2)).toBeGreaterThan(svfMag("bandpass", 1000, 1000, 0.5));
    expect(svfMag("allpass", 200, 1000, 0.7)).toBeCloseTo(1, 6);
    expect(filterCurvePath("filter", { type: "highpass", cutoff: "1000", resonance: "0.7" }))
      .not.toBe(filterCurvePath("filter", { type: "lowpass", cutoff: "1000", resonance: "0.7" }));
  });

  it("uses the EQ cookbook so a peak gain change is visible at fc", () => {
    const flat = eqMag("peak", 1000, 1000, 0.7, 0);
    const boost = eqMag("peak", 1000, 1000, 0.7, 6);
    expect(magToDb(boost)).toBeGreaterThan(magToDb(flat) + 4);
    expect(filterCurvePath("eq", { type: "peak", freq: "1000", q: "0.7", gain: "0" }))
      .not.toBe(filterCurvePath("eq", { type: "peak", freq: "1000", q: "0.7", gain: "6" }));
    expect(filterFaceModel("eq", { type: "highshelf", freq: "4800", q: "0.65", gain: "3" }).mode).toBe("highshelf");
  });

  it("does not eval formulas; a knob alias uses the ChipSpec default", () => {
    expect(filterFaceModel("filter", { type: "lowpass", cutoff: "b", resonance: "0.3" }).fc).toBe(1000);
  });
});
