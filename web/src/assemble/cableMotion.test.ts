import { describe, expect, it } from "vitest";
import {
  cableLayer,
  dashPatternLength,
  dotPeriodMs,
  lfoChaseMs,
  lfoDash,
  waveAnimMs,
  waveDash,
  waveDashFromScope,
  WAVE_MS_PER_PITCH,
} from "./cableMotion";
import { PLASMA_PITCH } from "./tubeModel";

describe("cable traffic", () => {
  it("sends one LFO pulse down the tube each cycle of the rate", () => {
    expect(lfoChaseMs(2)).toBe(500);
    expect(lfoChaseMs(10)).toBe(100);
    expect(lfoChaseMs(0)).toBe(1000);
    const { dash, cycle } = lfoDash(200, 14);
    expect(dash.startsWith("14 ")).toBe(true);
    expect(cycle).toBeGreaterThan(200);
  });

  it("keeps the audio wave on a fixed pixel pitch", () => {
    const dash = waveDash();
    expect(dashPatternLength(dash)).toBeCloseTo(PLASMA_PITCH);
    expect(waveAnimMs(PLASMA_PITCH)).toBe(WAVE_MS_PER_PITCH);
    expect(waveAnimMs(PLASMA_PITCH * 2)).toBe(WAVE_MS_PER_PITCH * 2);
  });

  it("prints the DSP scope as lit/gap pairs on that pitch", () => {
    const loud = waveDashFromScope(new Float32Array([1, 1, 1, 1]), PLASMA_PITCH, 4);
    const quiet = waveDashFromScope(new Float32Array([0, 0, 0, 0]), PLASMA_PITCH, 4);
    expect(dashPatternLength(loud)).toBeCloseTo(PLASMA_PITCH * 4);
    expect(dashPatternLength(quiet)).toBeCloseTo(PLASMA_PITCH * 4);
    const firstLoud = Number(loud.split(" ")[0]);
    const firstQuiet = Number(quiet.split(" ")[0]);
    expect(firstLoud).toBeGreaterThan(firstQuiet);
  });

  it("speeds dots up with loudness and leaves LFO independent of wave/dots", () => {
    expect(dotPeriodMs(1)).toBeLessThan(dotPeriodMs(0.05));
    expect(dotPeriodMs(1)).toBeGreaterThan(150);
    expect(cableLayer("mod", "dots", true)).toBe("lfo");
    expect(cableLayer("mod", "wave", true)).toBe("lfo");
    expect(cableLayer("audio", "wave", true)).toBe("wave");
    expect(cableLayer("audio", "dots", true)).toBe("dots");
    expect(cableLayer("audio", "wave", false)).toBe("still");
  });
});
