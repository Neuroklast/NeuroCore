import { describe, expect, it } from "vitest";
import { chamferWaypoints, waypointToSvgPath } from "./chamfer";

describe("chamferWaypoints", () => {
  it("cuts two 45° corners on a tall HVH and keeps a vertical middle", () => {
    const pts = chamferWaypoints([
      { x: 16, y: 80 },
      { x: 80, y: 80 },
      { x: 80, y: 208 },
      { x: 240, y: 208 },
    ]);
    const d = waypointToSvgPath(pts);
    expect(d.includes("Q") || d.includes("C")).toBe(false);
    expect(pts.some((p) => p.x === 80 && p.y > 80 && p.y < 208)).toBe(true);
    const diags = [];
    for (let i = 1; i < pts.length; i += 1) {
      const dx = Math.abs(pts[i].x - pts[i - 1].x);
      const dy = Math.abs(pts[i].y - pts[i - 1].y);
      if (dx > 1 && dy > 1) {
        diags.push({ dx, dy });
      }
    }
    expect(diags.length).toBe(2);
    for (const g of diags) {
      expect(g.dx).toBe(32);
      expect(g.dy).toBe(32);
    }
  });

  it("does not turn a one-cell hop into a lightning bolt", () => {
    const pts = chamferWaypoints([
      { x: 16, y: 80 },
      { x: 80, y: 80 },
      { x: 80, y: 112 },
      { x: 176, y: 112 },
    ]);
    const diags = [];
    for (let i = 1; i < pts.length; i += 1) {
      const dx = Math.abs(pts[i].x - pts[i - 1].x);
      const dy = Math.abs(pts[i].y - pts[i - 1].y);
      if (dx > 1 && dy > 1) {
        diags.push(i);
      }
    }
    for (let i = 1; i < diags.length; i += 1) {
      expect(diags[i]! - diags[i - 1]!).toBeGreaterThan(1);
    }
  });

  it("writes only M/L", () => {
    const d = waypointToSvgPath([{ x: 0, y: 0 }, { x: 32, y: 0 }]);
    expect(d.startsWith("M")).toBe(true);
    expect(d.includes("L")).toBe(true);
    expect(/[QC]/.test(d)).toBe(false);
  });
});
