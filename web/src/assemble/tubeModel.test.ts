import { describe, expect, it } from "vitest";
import { jackDiameter, plasmaAmp, plasmaGlow, plasmaWindows, PLASMA_PITCH, PLASMA_SPAN, shellGlow, TUBE } from "./tubeModel";

describe("glass tubes and jacks", () => {
  it("audio tubes are wide enough to hold a waveform", () => {
    expect(TUBE.audioOuter).toBeGreaterThanOrEqual(18);
    expect(TUBE.audioBore).toBeGreaterThanOrEqual(10);
    expect(TUBE.audioBore).toBeLessThan(TUBE.audioOuter);
  });

  it("only the inner wave glows, log in amplitude", () => {
    expect(shellGlow()).toBe(0);
    expect(plasmaGlow(1, "off", false)).toBe(0);
    expect(plasmaGlow(0.1, "full", false)).toBeGreaterThan(plasmaGlow(0, "full", false));
    expect(plasmaGlow(1, "full", false) - plasmaGlow(0.5, "full", false))
      .toBeLessThan(plasmaGlow(0.5, "full", false) - plasmaGlow(0, "full", false));
    expect(plasmaAmp(1, TUBE.audioBore)).toBeGreaterThan(plasmaAmp(0, TUBE.audioBore));
  });

  it("node jacks are large, visible ports", () => {
    expect(jackDiameter()).toBeGreaterThanOrEqual(14);
  });

  it("cuts the tube wave into short beads with a fixed pitch", () => {
    const wins = plasmaWindows(220);
    expect(wins.length).toBeGreaterThan(4);
    for (const w of wins) {
      expect(w.s1 - w.s0).toBeCloseTo(PLASMA_SPAN);
    }
    if (wins.length >= 2) {
      expect(wins[1].s0 - wins[0].s0).toBeCloseTo(PLASMA_PITCH);
    }
    expect(wins[wins.length - 1].s1).toBeLessThanOrEqual(220 + 0.02);
  });
});
