import { describe, expect, it } from "vitest";
import { chipHeight } from "./flowFromAst";
import {
  countCurves,
  inflate,
  isOctilinearDelta,
  midHits,
  TUBE_CLEAR,
  TUBE_STUB,
  minRunOk,
  plugsHorizontal,
  tubePath,
  polylinesCross,
} from "./tubePath";

describe("tube path — rounded circuit maze", () => {
  it("is a straight run when jacks share a row or column", () => {
    const h = tubePath(0, 40, 200, 40);
    expect(h.corners).toBe(0);
    expect(countCurves(h.d)).toBe(0);
    const v = tubePath(80, 0, 80, 160);
    expect(plugsHorizontal(v.points)).toBe(true);
  });

  it("enters and leaves audio jacks on a horizontal stub", () => {
    const cases = [
      tubePath(10, 50, 260, 140),
      tubePath(0, 40, 80, 70),
      tubePath(80, 0, 80, 160),
    ];
    for (const p of cases) {
      expect(plugsHorizontal(p.points)).toBe(true);
    }
  });

  it("plugs straight into the jack, then turns", () => {
    const p = tubePath(10, 50, 260, 140);
    expect(p.points[0]).toEqual({ x: 10, y: 50 });
    expect(p.points[p.points.length - 1]).toEqual({ x: 260, y: 140 });
    expect(p.points[1]?.y).toBeCloseTo(50);
    expect(Math.abs((p.points[1]?.x ?? 0) - 10)).toBeGreaterThanOrEqual(TUBE_STUB - 1);
    expect(p.d.startsWith("M 10.00 50.00 L")).toBe(true);
  });

  it("refuses a turn until the run is at least TUBE_MIN_RUN", () => {
    const cases = [
      tubePath(10, 50, 260, 140),
      tubePath(0, 40, 240, 180),
      tubePath(0, 40, 240, 40, {
        obstacles: [{ id: "chip", x: 80, y: 20, w: 80, h: 40 }],
        sourceId: "src",
        targetId: "dst",
      }),
    ];
    for (const p of cases) {
      expect(minRunOk(p.points)).toBe(true);
    }
  });

  it("uses at most two rounded corners on a clear board", () => {
    const p = tubePath(0, 40, 240, 180);
    expect(p.corners).toBeLessThanOrEqual(2);
    expect(p.d.includes("C")).toBe(false);
  });

  it("takes one diagonal for a near-row hop instead of a 4-corner stair", () => {
    const p = tubePath(140, 140, 280, 163, {
      obstacles: [
        { id: "IN", x: 16, y: 112, w: 124, h: 56 },
        { id: "filter1", x: 280, y: 112, w: 236, h: 102 },
        { id: "OUT", x: 1200, y: 16, w: 124, h: 56 },
      ],
      sourceId: "IN",
      targetId: "filter1",
    });
    expect(p.corners).toBeLessThanOrEqual(2);
    expect(plugsHorizontal(p.points)).toBe(true);
    const lo = Math.min(140, 163);
    const hi = Math.max(140, 163);
    expect(p.points.every((pt) => pt.y >= lo - 1 && pt.y <= hi + 1)).toBe(true);
    let diagonals = 0;
    for (let i = 1; i < p.points.length; i += 1) {
      const dx = p.points[i].x - p.points[i - 1].x;
      const dy = p.points[i].y - p.points[i - 1].y;
      if (Math.abs(dx) > 2 && Math.abs(dy) > 2) {
        diagonals += 1;
      }
    }
    expect(diagonals).toBeLessThanOrEqual(1);
  });

  it("only walks horizontal, vertical, or 45° diagonal", () => {
    const cases = [
      tubePath(0, 40, 240, 180),
      tubePath(10, 50, 80, 200),
      tubePath(0, 0, 120, 120),
      tubePath(300, 80, 40, 200),
    ];
    for (const p of cases) {
      for (let i = 1; i < p.points.length; i += 1) {
        const dx = p.points[i].x - p.points[i - 1].x;
        const dy = p.points[i].y - p.points[i - 1].y;
        expect(isOctilinearDelta(dx, dy)).toBe(true);
      }
    }
  });

  it("goes around a chip instead of through it", () => {
    const block = { id: "chip", x: 80, y: 20, w: 80, h: 40 };
    const p = tubePath(0, 40, 240, 40, {
      obstacles: [block],
      sourceId: "src",
      targetId: "dst",
    });
    const pad = inflate(block, TUBE_CLEAR);
    expect(midHits(p.points, [pad])).toBe(false);
    expect(p.points.some((pt) => Math.abs(pt.y - 40) > 8)).toBe(true);
  });

  it("detours so two tubes do not cross when a rail exists", () => {
    const first = tubePath(0, 20, 240, 80);
    const second = tubePath(0, 80, 240, 20, { reserved: [first.points] });
    expect(polylinesCross(first.points, second.points)).toBe(false);
  });

  it("grows the chip when there are several jacks", () => {
    expect(chipHeight(1, 1)).toBe(80);
    expect(chipHeight(3, 1)).toBeGreaterThan(chipHeight(1, 1));
    expect(chipHeight(2, 4)).toBeGreaterThan(chipHeight(2, 1));
  });
});
