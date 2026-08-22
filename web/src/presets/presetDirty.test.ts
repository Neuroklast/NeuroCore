import { describe, expect, it } from "vitest";
import { needsDiscardConfirm, originForStep, presetTitle } from "./presetDirty";
import { stepPresetName } from "./presetStep";

describe("preset dirty title", () => {
  it("marks an edited factory with a star and keeps untitled when empty", () => {
    expect(presetTitle("Blues Break OD", false)).toBe("Blues Break OD");
    expect(presetTitle("Blues Break OD", true)).toBe("Blues Break OD*");
    expect(presetTitle("", true)).toBe("untitled*");
  });

  it("prompts only when dirty and the setting is on", () => {
    expect(needsDiscardConfirm(true, true)).toBe(true);
    expect(needsDiscardConfirm(true, false)).toBe(false);
    expect(needsDiscardConfirm(false, true)).toBe(false);
  });

  it("next/prev walk from the origin factory, not a dirty alias", () => {
    const rows = [
      { name: "Airy Clean", category: "Guitar" },
      { name: "Blues Break OD", category: "Guitar" },
      { name: "Crunch", category: "Guitar" },
    ];
    const origin = originForStep("Blues Break OD", "Blues Break OD*");
    expect(stepPresetName(rows, origin, 1)).toBe("Crunch");
  });
});
