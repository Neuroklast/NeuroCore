import { describe, expect, it } from "vitest";
import { utilityFaceModel, utilityMarks, utilityParamsPath } from "./utilityParams";

describe("calculated utility params", () => {
  it("marks gain on a vertical tick and pan on a left-right baseline", () => {
    const zero = utilityMarks({ gain: "0", pan: "0" });
    expect(zero.gain.x).toBe(28);
    expect(zero.gain.y).toBe(20);
    expect(zero.pan.x).toBe(50);
    expect(zero.pan.y).toBe(28);
    const hot = utilityMarks({ gain: "24", pan: "1" });
    expect(hot.gain.y).toBe(6);
    expect(hot.pan.x).toBe(86);
    expect(utilityParamsPath({ gain: "-12" })).not.toBe(utilityParamsPath({ gain: "12" }));
    expect(utilityFaceModel({ gain: "a", pan: "b" }).gain).toBe(0);
    expect(utilityFaceModel({ polarity: "on" }).polarity).toBe(true);
    expect(utilityFaceModel({ polarity: "f" }).polarity).toBe(false);
  });
});
