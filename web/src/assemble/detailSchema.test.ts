import { describe, expect, it } from "vitest";
import { detailArgs, isCustomNode, nextCustomInput } from "./detailSchema";

describe("detail schema", () => {
  it("gives every filter a cutoff row even when args are empty", () => {
    const rows = detailArgs("filter", {});
    expect(rows.map((r) => r.key)).toEqual(expect.arrayContaining(["type", "cutoff", "resonance"]));
  });

  it("keeps extra custom inputs after the formula", () => {
    const rows = detailArgs("custom", { y: "x * a", in2: "0" });
    expect(rows[0]).toEqual({ key: "y", value: "x * a" });
    expect(rows.some((r) => r.key === "in2" && r.value === "0")).toBe(true);
    expect(nextCustomInput({ y: "x", in2: "0" })).toBe("in3");
    expect(isCustomNode("stage", "custom1")).toBe(true);
  });
});
