import { describe, expect, it } from "vitest";
import { cabFaceModel, cabMixPath, cabStems } from "./cabMix";

describe("calculated cabinet mix", () => {
  it("places dry and wet stems from mix, without inventing an IR name", () => {
    const mesa = cabStems({ mix: "0.45" });
    expect(mesa).toHaveLength(2);
    expect(mesa[0]!.x).toBe(28);
    expect(mesa[1]!.x).toBe(72);
    expect(mesa[0]!.y1).toBeCloseTo(40 - 40 * 0.55, 5);
    expect(mesa[1]!.y1).toBeCloseTo(40 - 40 * 0.45, 5);
    expect(cabMixPath({ mix: "0.2" })).not.toBe(cabMixPath({ mix: "0.8" }));
    expect(cabFaceModel({ mix: "c" }).mix).toBe(0.3);
    expect(cabFaceModel({ mix: "c" }, { knobs: [{ id: "c", value: 1, min: 0, max: 1 }] }).mix).toBe(1);
    expect(cabFaceModel({ gain: "b" }).gain).toBe(0);
    expect(cabFaceModel({ mix: "0.45", gain: "0" }).mix).toBe(0.45);
  });
});
