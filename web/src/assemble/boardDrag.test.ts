import { describe, expect, it } from "vitest";
import { applyChipDragStyle, chipDragTranslate, nodeWithDrag, type ChipDrag } from "./boardDrag";
import { snapToGrid } from "./grid";

describe("chip drag lives in a ref until pointerup", () => {
  it("translates the chip from its store origin, and does not rewrite store xy", () => {
    const origin = { x: 320, y: 160 };
    const drag: ChipDrag = { id: "stage1", x: 384, y: 192 };
    expect(chipDragTranslate(drag, origin)).toBe("translate(64px, 32px)");
    const store = { id: "stage1", x: origin.x, y: origin.y };
    const live = nodeWithDrag(store, drag);
    expect(live.x).toBe(384);
    expect(live.y).toBe(192);
    expect(store.x).toBe(320);
  });

  it("snaps the live point the same as a committed move", () => {
    const drag: ChipDrag = { id: "filter1", x: snapToGrid(333), y: snapToGrid(171) };
    expect(drag.x % 32).toBe(0);
    expect(drag.y % 32).toBe(0);
  });

  it("clears the CSS translate when the drag ends", () => {
    const el = { style: { transform: "translate(64px, 32px)" } };
    applyChipDragStyle(el, { id: "stage1", x: 384, y: 192 }, { x: 320, y: 160 }, "stage1");
    expect(el.style.transform).toBe("translate(64px, 32px)");
    applyChipDragStyle(el, null, { x: 320, y: 160 }, "stage1");
    expect(el.style.transform).toBe("");
  });
});
