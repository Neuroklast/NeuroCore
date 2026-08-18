import { describe, expect, it } from "vitest";
import { lfoPeriodMs, lfoWave, parseLfoShape } from "./lfoLamp";

describe("LFO lamp", () => {
  it("blinks at the LFO rate and follows the oscillator shape", () => {
    expect(parseLfoShape("sine")).toBe("sine");
    expect(parseLfoShape("sawtooth")).toBe("saw");
    expect(parseLfoShape("softsquare")).toBe("softsquare");
    expect(parseLfoShape("triangle")).toBe("triangle");
    expect(lfoPeriodMs(2)).toBe(500);
    expect(lfoPeriodMs(0)).toBe(1000);
    expect(lfoWave(0.25, "sine")).toBeGreaterThan(0.9);
    expect(lfoWave(0.2, "square")).toBe(1);
    expect(lfoWave(0.7, "square")).toBe(0);
    expect(lfoWave(0, "saw")).toBeCloseTo(0);
    expect(lfoWave(1, "saw")).toBeCloseTo(0);
    expect(lfoWave(0.5, "triangle")).toBeCloseTo(1);
    expect(lfoWave(0, "triangle")).toBeCloseTo(0);
  });
});
