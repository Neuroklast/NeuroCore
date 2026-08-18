import { describe, expect, it } from "vitest";
import { PRESET_MIN_PX, toolbarFixedMinPx, toolbarSlots } from "./toolbarChrome";

describe("toolbar preset chrome", () => {
  it("has no separate Presets button", () => {
    expect(toolbarSlots().some((s) => String(s.id) === "presets")).toBe(false);
  });

  it("the current name is the title and opens the explorer", () => {
    const now = toolbarSlots().find((s) => s.id === "presetNow");
    expect(now).toEqual({ id: "presetNow", kind: "title", opens: "presets", className: "nk-preset-now" });
  });

  it("prev and next stay around the name", () => {
    const ids = toolbarSlots().map((s) => s.id);
    const i = ids.indexOf("presetNow");
    expect(ids[i - 1]).toBe("presetPrev");
    expect(ids[i + 1]).toBe("presetNext");
  });

  it("keeps validate and optimize off the header", () => {
    const ids = toolbarSlots().map((s) => s.id);
    expect(ids).not.toContain("validate");
    expect(ids).not.toContain("optimize");
  });
});

describe("toolbar budget at 1280", () => {
  it("lets the preset title shrink so BYPASS stays on the 1280 bar", () => {
    expect(PRESET_MIN_PX).toBeLessThanOrEqual(160);
    expect(toolbarFixedMinPx() + PRESET_MIN_PX).toBeLessThanOrEqual(1280);
  });
});

