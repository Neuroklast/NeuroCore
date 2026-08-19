import { describe, expect, it } from "vitest";
import { ADDABLE_BLOCKS, ADD_CATEGORIES } from "../assemble/addBlock";
import { addMenuOverflow, addPickerBlocks, addPickerListsEveryBlock, addPickerSize } from "./addPicker";

describe("circuit add picker", () => {
  it("lists every addable chip in one panel — no nested flyout", () => {
    const labels = addPickerListsEveryBlock();
    expect(labels.length).toBe(ADDABLE_BLOCKS.length);
    expect(labels).toEqual(expect.arrayContaining(["Drive", "Filter", "Delay", "LFO", "Split Mid/Side", "Join L/R", "Custom"]));
    expect(addPickerBlocks("Routing").map((b) => b.label)).toEqual(expect.arrayContaining([
      "Split Mid/Side",
      "Join Mid/Side",
      "Split L/R",
      "Join L/R",
      "Multiband Split",
      "Send",
      "Width",
    ]));
    expect(addPickerBlocks("Routing").map((b) => b.label).includes("Xover")).toBe(false);
    expect(ADDABLE_BLOCKS.some((b) => b.label === "Octaver")).toBe(true);
    expect(addMenuOverflow("root")).toBe("visible");
    expect(addMenuOverflow("list")).toBe("auto");
  });

  it("sizes the picker to the categories, not a clipped flyout", () => {
    const size = addPickerSize("Dynamics");
    expect(size.h).toBeGreaterThanOrEqual(ADD_CATEGORIES.length * 28);
    expect(size.w).toBeGreaterThan(200);
    expect(size.h).toBeLessThan(420);
  });
});
