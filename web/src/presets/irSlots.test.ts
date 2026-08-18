import { describe, expect, it } from "vitest";
import { chipOverlay, irSlotFromLine, irSlotsFromScript, isIrSlotId, mergeIrSlots } from "./irSlots";

describe("IR slot ids", () => {
  it("accepts ir / irN / convolve, never iron or stage", () => {
    expect(isIrSlotId("ir1")).toBe(true);
    expect(isIrSlotId("IR2")).toBe(true);
    expect(isIrSlotId("ir")).toBe(true);
    expect(isIrSlotId("convolve")).toBe(true);
    expect(isIrSlotId("convolve2")).toBe(true);
    expect(isIrSlotId("iron")).toBe(false);
    expect(isIrSlotId("stage1")).toBe(false);
    expect(isIrSlotId("filter1")).toBe(false);
  });

  it("reads slot ids from formula lines, skips comments", () => {
    expect(irSlotFromLine("ir1: mix = 0.3; gain = 0")).toBe("ir1");
    expect(irSlotFromLine("  ir2: mix = 1")).toBe("ir2");
    expect(irSlotFromLine("# ir1: mix = 0.3")).toBe(null);
    expect(irSlotFromLine("stage1: y = x")).toBe(null);
    expect(irSlotsFromScript("stage1: y = x\nir1: mix = 0.3\n# ir2: mix = 1\nir1: mix = 0.4\n")).toEqual(["ir1"]);
  });
});

describe("IR overlay target", () => {
  it("opens Impulse for an IR chip and Inspect for everything else", () => {
    expect(chipOverlay("ir1", "ir")).toEqual({ overlay: "ir", inspectId: "ir1" });
    expect(chipOverlay("filter1", "filter")).toEqual({ overlay: "inspect", inspectId: "filter1" });
    expect(chipOverlay("stage1", "stage")).toEqual({ overlay: "inspect", inspectId: "stage1" });
  });

  it("keeps script chips even when the host bank is empty (dry slot)", () => {
    const rows = mergeIrSlots(["ir1"], []);
    expect(rows).toEqual([{ slot: "ir1", name: "", loaded: false }]);
    const loaded = mergeIrSlots(["ir1"], [{ slot: "ir1", name: "American IR 01.wav", loaded: true }]);
    expect(loaded[0]).toEqual({ slot: "ir1", name: "American IR 01.wav", loaded: true });
  });
});
