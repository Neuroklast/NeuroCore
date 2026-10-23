import { describe, expect, it } from "vitest";
import { placeTooltip } from "./tooltipPlace";

const view = { w: 1280, h: 860 };

describe("tooltip placement", () => {
  it("flips below when there is no room above", () => {
    const p = placeTooltip({ x: 100, y: 4, w: 80, h: 32 }, { w: 160, h: 28 }, view);
    expect(p.y).toBeGreaterThan(36);
  });

  it("stays inside the left and right edges", () => {
    const left = placeTooltip({ x: 0, y: 40, w: 32, h: 32 }, { w: 200, h: 28 }, view);
    expect(left.x).toBeGreaterThanOrEqual(4);
    const right = placeTooltip({ x: 1260, y: 40, w: 32, h: 32 }, { w: 240, h: 28 }, view);
    expect(right.x + 240).toBeLessThanOrEqual(view.w - 4);
  });

  it("wraps a long tip inside the plugin width", () => {
    const p = placeTooltip({ x: 640, y: 40, w: 80, h: 32 }, { w: 400, h: 48 }, view);
    expect(p.x).toBeGreaterThanOrEqual(4);
    expect(p.x + 400).toBeLessThanOrEqual(view.w - 4);
  });
});
