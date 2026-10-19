import { describe, expect, it } from "vitest";
import { chipBox } from "./chipLayout";
import { liveArg } from "./liveArg";

describe("live node values and chip box", () => {
  const knobs = [
    { id: "a", value: 1, min: 0.05, max: 6 },
    { id: "b", value: 0.3, min: 200, max: 2500 },
  ];

  it("resolves a bound letter to the mapped live number", () => {
    const a = liveArg("a", knobs);
    expect(a.formula).toBe("a");
    expect(Number(a.live)).toBeCloseTo(6, 2);
    const f = liveArg("c + lfo1 * b", knobs);
    expect(f.formula).toContain("lfo1");
    expect(f.live).toContain("lfo1");
    expect(f.live).not.toMatch(/\bb\b/);
  });

  it("keeps expand from growing the box; extra side jacks can still raise it", () => {
    const one = chipBox("filter", [
      { id: "in", label: "in", output: false, kind: "audio" },
      { id: "out", label: "out", output: true, kind: "audio" },
    ], false, { type: "lp" });
    const many = chipBox("filter", [
      { id: "in", label: "in", output: false, kind: "audio" },
      ...["m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8"].map((id) => (
        { id, label: id, output: false, kind: "mod" as const }
      )),
      { id: "out", label: "out", output: true, kind: "audio" },
    ], false, { type: "lp" });
    const open = chipBox("filter", [
      { id: "in", label: "in", output: false, kind: "audio" },
      { id: "out", label: "out", output: true, kind: "audio" },
    ], true, { type: "lp" });
    expect(many.h).toBeGreaterThan(one.h);
    expect(open.h).toBe(one.h);
  });
});
