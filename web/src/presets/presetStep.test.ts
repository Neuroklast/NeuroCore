import { describe, expect, it } from "vitest";
import { sortPresetStep, stepPresetName } from "./presetStep";

const rows = [
  { name: "Zebra", category: "Club" },
  { name: "Alpha", category: "Club" },
  { name: "Mid", category: "Dynamics" },
  { name: "Airy", category: "Amp" },
];

describe("preset arrow walk", () => {
  it("sorts by category folder, then name", () => {
    expect(sortPresetStep(rows).map((r) => r.name)).toEqual(["Airy", "Alpha", "Zebra", "Mid"]);
  });

  it("finishes the selected folder, then jumps to the next category", () => {
    expect(stepPresetName(rows, "Alpha", 1, "Club")).toBe("Zebra");
    expect(stepPresetName(rows, "Zebra", 1, "Club")).toBe("Mid");
  });

  it("starts at the selected folder when the current preset is outside it", () => {
    expect(stepPresetName(rows, "Airy", 1, "Club")).toBe("Alpha");
    expect(stepPresetName(rows, "Airy", -1, "Club")).toBe("Zebra");
  });
});
