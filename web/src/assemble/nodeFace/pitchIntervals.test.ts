import { describe, expect, it } from "vitest";
import { pitchFaceModel, pitchIntervalsPath, pitchStems } from "./pitchIntervals";

describe("calculated pitch intervals", () => {
  it("places octaver sub / dry / up stems from mix", () => {
    const stems = pitchStems("octaver", { sub: "1", up: "0", mix: "0.3" });
    expect(stems).toHaveLength(3);
    expect(stems[0]!.x).toBe(18);
    expect(stems[1]!.x).toBe(50);
    expect(stems[2]!.x).toBe(82);
    expect(stems[0]!.y1).toBeCloseTo(40 - 40 * (1 * 0.3 / 1.5), 5);
    expect(stems[1]!.y1).toBeCloseTo(40 - 40 * 0.7, 5);
    expect(stems[2]!.y1).toBe(40);
    expect(pitchIntervalsPath("octaver", { sub: "1", up: "1.5", mix: "1" }))
      .not.toBe(pitchIntervalsPath("octaver", { sub: "1", up: "0", mix: "0.3" }));
  });

  it("marks pitch semitones and vocoder band count without a spectrum", () => {
    const unison = pitchStems("pitch", { semitones: "0", mix: "1" });
    const fifth = pitchStems("pitch", { semitones: "7", mix: "1" });
    expect(unison).toHaveLength(1);
    expect(unison[0]!.x).toBeCloseTo(50, 5);
    expect(fifth[0]!.x).toBeCloseTo(100 * (7 + 24) / 48, 5);
    expect(pitchStems("vocoder", { bands: "8", mix: "0.85" })).toHaveLength(8);
    expect(pitchFaceModel("octaver", { mix: "a" }).mix).toBe(0.3);
    expect(pitchFaceModel("vocoder", { bands: "q" }).bands).toBe(16);
    expect(pitchFaceModel("pitch", { semitones: "b" }).semitones).toBe(0);
  });
});
