import { describe, expect, it } from "vitest";
import { DISPLAY_AMP_FLOOR } from "../bridge/telemetry";
import {
  crestFactorDb,
  dominantBand,
  formatUnavailable,
  measureFeatures,
  peakAbs,
  rmsAbs,
  stereoBalance,
} from "./audioFeatures";
import { gonioFromLr, silence, tone } from "./audioSignals";

describe("audio feature contracts", () => {
  it("peak and rms of a full-scale sine match the analytic values", () => {
    const x = tone(1000, 48000, 2048, 1);
    expect(peakAbs(x)).toBeGreaterThan(0.99);
    expect(rmsAbs(x)).toBeCloseTo(Math.SQRT1_2, 2);
    const crest = crestFactorDb(peakAbs(x), rmsAbs(x));
    expect(crest).not.toBeNull();
    expect(crest!).toBeCloseTo(3, 0);
  });

  it("silence has no crest and no invented pan", () => {
    expect(crestFactorDb(0, 0)).toBeNull();
    expect(crestFactorDb(DISPLAY_AMP_FLOOR, 0)).toBeNull();
    expect(stereoBalance(0, 0)).toBe(0);
    expect(formatUnavailable("unavailable", "0.0")).toBe("—");
    expect(formatUnavailable("live", "0.0")).toBe("0.0");
  });

  it("hard left is negative balance; equal L/R is centred", () => {
    expect(stereoBalance(1, 0)).toBeCloseTo(-1, 8);
    expect(stereoBalance(0, 1)).toBeCloseTo(1, 8);
    expect(stereoBalance(0.4, 0.4)).toBeCloseTo(0, 8);
  });

  it("a 50 Hz tone lights low; 8 kHz lights high", () => {
    const sr = 48000;
    const bass = measureFeatures(tone(50, sr, 2048, 0.4), [], [], sr, { validity: "live" });
    const air = measureFeatures(tone(8000, sr, 2048, 0.4), [], [], sr, { validity: "live" });
    expect(dominantBand(bass.bands)).toBe("low");
    expect(dominantBand(air.bands)).toBe("high");
    expect(bass.validity).toBe("live");
  });

  it("unavailable measurement is not a live zero", () => {
    const x = tone(100, 48000, 256, 1);
    const dead = measureFeatures(x, [], [], 48000, { validity: "unavailable" });
    expect(dead.validity).toBe("unavailable");
    expect(dead.peak).toBe(0);
    expect(dead.crestDb).toBeNull();
  });

  it("mono gonio has high correlation and no side energy", () => {
    const m = tone(400, 48000, 128, 0.5);
    const g = gonioFromLr(m, m, 64);
    const f = measureFeatures(m, g.x, g.y, 48000);
    expect(f.corr).toBeGreaterThan(0.95);
    expect(Math.abs(f.balance)).toBeLessThan(0.05);
  });
});
