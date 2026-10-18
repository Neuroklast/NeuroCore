import { describe, expect, it } from "vitest";
import { gonioFromLr, impulse, kickDecay, noise, periodicKick, silence, tone, twoImpulses } from "./audioSignals";

describe("analytic audio fixtures", () => {
  it("silence is zero; a 50 Hz tone at 48 kHz has a known period", () => {
    const z = silence(64);
    expect(z.every((v) => v === 0)).toBe(true);
    const sr = 48000;
    const x = tone(50, sr, 960, 1);
    expect(x[0]).toBeCloseTo(0, 8);
    expect(x[sr / 50 / 4]).toBeCloseTo(1, 5);
  });

  it("an isolated impulse is one sample; two clicks keep a gap", () => {
    const one = impulse(32, 8, 1);
    expect(one.filter((v) => v !== 0)).toHaveLength(1);
    expect(one[8]).toBe(1);
    const two = twoImpulses(64, 5, 1);
    const hits = [...two].map((v, i) => (v !== 0 ? i : -1)).filter((i) => i >= 0);
    expect(hits).toHaveLength(2);
    expect(hits[1]! - hits[0]!).toBe(5);
  });

  it("kick decay falls; periodic kicks land on the beat", () => {
    const sr = 44100;
    const k = kickDecay(sr, sr, 55, 0.05, 1);
    expect(Math.abs(k[0])).toBeLessThan(0.05);
    expect(Math.max(...k)).toBeGreaterThan(0.5);
    expect(Math.abs(k[Math.floor(0.4 * sr)]!)).toBeLessThan(0.02);
    const loop = periodicKick(sr, sr, 120, 55);
    expect(Math.max(...loop.subarray(0, 200))).toBeGreaterThan(0.3);
  });

  it("hard-left gonio is side energy without inventing right", () => {
    const n = 16;
    const l = tone(1000, 48000, n, 0.5);
    const r = silence(n);
    const g = gonioFromLr(l, r, n);
    expect(Math.max(...g.x.map(Math.abs))).toBeGreaterThan(0.1);
    const noiseFloor = noise(8, 0.2, 4);
    expect(noiseFloor.some((v) => v < 0) && noiseFloor.some((v) => v > 0)).toBe(true);
  });
});
