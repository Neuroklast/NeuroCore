import { describe, expect, it } from "vitest";
import { lfoChaseMs } from "./cableMotion";
import { noteSteps } from "../chrome/noteValue";
import { lfoPeriodMs, lfoWave, parseLfoShape, resolveLfoHz } from "./lfoLamp";

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

  it("sync 1/4 at 120 BPM is 2 Hz for lamp and cable, not the free-run freq", () => {
    const hz = resolveLfoHz({ freq: "1", sync: "1/4" }, [], 120);
    expect(hz).toBeCloseTo(2);
    expect(lfoPeriodMs(hz)).toBe(500);
    expect(lfoChaseMs(hz)).toBe(500);
    expect(resolveLfoHz({ freq: "1", sync: "1/8" }, [], 120)).toBeCloseTo(4);
    expect(lfoPeriodMs(4)).toBe(250);
    expect(resolveLfoHz({ freq: "1", sync: "off" }, [], 120)).toBeCloseTo(1);
    expect(resolveLfoHz({ freq: "3", sync: "off" }, [], 120)).toBeCloseTo(3);
  });

  it("note-knob freq or sync is one cycle per note, not the knob norm as Hz", () => {
    const steps = noteSteps(1, 0.0625);
    const i = steps.findIndex((s) => Math.abs(s.whole - 0.25) < 1e-3);
    const knobs = [{ id: "a", value: i / Math.max(1, steps.length - 1), min: 1, max: 0.0625, isNote: true }];
    const hz = resolveLfoHz({ freq: "a", sync: "off" }, knobs, 120);
    expect(hz).toBeCloseTo(2);
    expect(lfoPeriodMs(hz)).toBe(lfoChaseMs(hz));
    expect(resolveLfoHz({ freq: "1", sync: "a" }, knobs, 120)).toBeCloseTo(2);
  });
});
