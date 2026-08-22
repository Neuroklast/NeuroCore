import { describe, expect, it } from "vitest";
import {
  cableLayer,
  dashPatternLength,
  dotPeriodMs,
  lfoChaseMs,
  lfoDash,
  lfoDotGlow,
  LFO_WIRE,
  advancePlasmaDash,
  plasmaSpeedPxPerSec,
  svgPathLength,
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

  it("keeps a single glowing dot on the wire for the whole path", () => {
    const d = "M0 0L100 0L100 80";
    expect(svgPathLength(d)).toBe(180);
    expect(svgPathLength("M320 240L368 240L400 272L400 304L448 304")).toBeGreaterThan(100);
    const len = svgPathLength(d);
    const { dash, cycle, trip } = lfoDash(len);
    const [blob, gap] = dash.split(" ").map(Number);
    expect(blob).toBeLessThan(16);
    expect(gap).toBeGreaterThanOrEqual(len);
    expect(cycle).toBe(blob + gap);
    expect(trip).toBe(len);
    expect(cycle).toBeGreaterThan(len);
    expect(lfoDotGlow(1)).toBeGreaterThan(lfoDotGlow(0.1));
    expect(lfoDotGlow(0)).toBeGreaterThan(0);
    expect(LFO_WIRE).toBeLessThan(3);
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

  it("holds still at silence or -60 dBFS and runs faster as the source chip gets louder", () => {
    expect(plasmaSpeedPxPerSec(0)).toBe(0);
    expect(plasmaSpeedPxPerSec(0.001)).toBe(0);
    expect(plasmaSpeedPxPerSec(10 ** (-60 / 20))).toBe(0);
    expect(plasmaSpeedPxPerSec(10 ** (-59 / 20))).toBeGreaterThan(0);
    expect(plasmaSpeedPxPerSec(1)).toBeGreaterThan(plasmaSpeedPxPerSec(0.1));
    expect(plasmaSpeedPxPerSec(0.1)).toBeGreaterThan(plasmaSpeedPxPerSec(0.01));
    expect(advancePlasmaDash(-20, 0, 0.016)).toBe(-20);
    expect(advancePlasmaDash(-20, 0.001, 0.016)).toBe(-20);
    expect(advancePlasmaDash(0, 0.2, 0.016)).toBeLessThan(0);
    expect(advancePlasmaDash(-20, 1, 0.016)).toBeLessThan(advancePlasmaDash(-20, 0.05, 0.016));
    expect(dotPeriodMs(1)).toBeLessThan(dotPeriodMs(0.05));
    expect(cableLayer("mod", "dots", true)).toBe("lfo");
    expect(cableLayer("mod", "wave", true)).toBe("lfo");
    expect(cableLayer("audio", "wave", true)).toBe("wave");
    expect(cableLayer("audio", "dots", true)).toBe("dots");
    expect(cableLayer("audio", "wave", false)).toBe("wave");
    expect(cableLayer("audio", "dots", false)).toBe("dots");
  });
});
