import { describe, expect, it } from "vitest";
import { computeGrDb, dynamicsCurvePath, dynamicsFaceModel, dynamicsOutDb } from "./dynamicsCurve";

describe("calculated dynamics curve", () => {
  it("follows Comp::computeGrDb so threshold and ratio move the path", () => {
    expect(computeGrDb(-30, -18, 4, 0)).toBe(0);
    expect(computeGrDb(-6, -18, 4, 0)).toBeCloseTo(9, 5);
    expect(dynamicsOutDb("comp", { threshold: "-18", ratio: "4", mix: "1" }, -6))
      .toBeCloseTo(-15, 5);
    expect(dynamicsCurvePath("comp", { threshold: "-24", ratio: "4", mix: "1" }))
      .not.toBe(dynamicsCurvePath("comp", { threshold: "-6", ratio: "4", mix: "1" }));
    expect(dynamicsCurvePath("comp", { threshold: "-18", ratio: "2", mix: "1" }))
      .not.toBe(dynamicsCurvePath("comp", { threshold: "-18", ratio: "8", mix: "1" }));
  });

  it("limits at ceiling, gates below threshold, and leaves factory formulas on defaults", () => {
    expect(dynamicsOutDb("limit", { ceiling: "-0.3" }, 0)).toBeCloseTo(-0.3, 5);
    expect(dynamicsOutDb("limit", { ceiling: "-0.3" }, -12)).toBeCloseTo(-12, 5);
    expect(dynamicsOutDb("noisegate", { threshold: "-18", range: "-80" }, -40))
      .toBeCloseTo(-120, 5);
    expect(dynamicsOutDb("noisegate", { threshold: "-18", range: "-80" }, -6))
      .toBeCloseTo(-6, 5);
    expect(dynamicsFaceModel("comp", { threshold: "a", ratio: "b" }).threshold).toBe(-18);
    expect(dynamicsCurvePath("ott", { depth: "0.2" }))
      .not.toBe(dynamicsCurvePath("ott", { depth: "1" }));
    expect(dynamicsCurvePath("transient", { attack: "1", sustain: "0" }))
      .not.toBe(dynamicsCurvePath("transient", { attack: "-1", sustain: "0" }));
  });
});
