import { Position } from "@xyflow/react";
import { describe, expect, it } from "vitest";
import {
  audioStepPath,
  countCorners,
  diagLongEnough,
  firstLast,
  hasDiagonal,
  isOctilinearPoints,
  railsOverlap,
  stubGoesEast,
  turnsAreOctilinear,
  verticalRails,
} from "./audioStep";
import { BOARD_GRID, onCellCenter } from "./grid";
import { inflate, midHits, TUBE_CLEAR } from "./tubePath";

function dump(pts: Array<{ x: number; y: number }>): string {
  return pts.map((p) => `(${p.x.toFixed(1)},${p.y.toFixed(1)})`).join(" ");
}

describe("audioStepPath — circuit traces, not stairs", () => {
  it("does not stair-step a near-row hop (screenshot 104942)", () => {
    const p = audioStepPath(144, 48, 272, 80);
    expect(countCorners(p.points), dump(p.points)).toBeLessThanOrEqual(2);
    expect(hasDiagonal(p.points), dump(p.points)).toBe(true);
    expect(diagLongEnough(p.points), dump(p.points)).toBe(true);
    expect(isOctilinearPoints(p.points), dump(p.points)).toBe(true);
    expect(stubGoesEast(p.points[0]!, p.points[1]!)).toBe(true);
  });

  it("is a straight run when jacks share a row", () => {
    const p = audioStepPath(16, 80, 240, 80);
    expect(countCorners(p.points), dump(p.points)).toBe(0);
  });

  it("starts and ends on the handle coordinates", () => {
    const p = audioStepPath(100, 40, 400, 180);
    const { first, last } = firstLast(p.points);
    expect(first, dump(p.points)).toEqual({ x: 100, y: 40 });
    expect(last, dump(p.points)).toEqual({ x: 400, y: 180 });
  });

  it("leaves an output east and enters an input from the west", () => {
    const cases = [
      audioStepPath(100, 40, 400, 180),
      audioStepPath(400, 40, 100, 180),
      audioStepPath(200, 40, 200, 200),
      audioStepPath(140, 140, 280, 163),
    ];
    for (const p of cases) {
      const pts = p.points;
      expect(stubGoesEast(pts[0]!, pts[1]!), dump(pts)).toBe(true);
      expect(stubGoesEast(pts[pts.length - 2]!, pts[pts.length - 1]!), dump(pts)).toBe(true);
      expect(pts[1]!.x - pts[0]!.x, dump(pts)).toBeGreaterThanOrEqual(8);
    }
  });

  it("walks only H, V, or 45° on the board cell — no off-angle stairs", () => {
    const cases = [
      audioStepPath(96, 48, 400, 176),
      audioStepPath(400, 80, 112, 112),
      audioStepPath(144, 144, 272, 160),
      audioStepPath(320, 80, 96, 80),
    ];
    for (const p of cases) {
      expect(isOctilinearPoints(p.points), dump(p.points)).toBe(true);
      expect(turnsAreOctilinear(p.points), dump(p.points)).toBe(true);
    }
  });

  it("keeps every interior vertex on the cell when the jacks sit on the cell", () => {
    const p = audioStepPath(16, 80, 240, 176);
    expect(p.points[0], dump(p.points)).toEqual({ x: 16, y: 80 });
    expect(p.points[p.points.length - 1], dump(p.points)).toEqual({ x: 240, y: 176 });
    for (let i = 1; i < p.points.length - 1; i += 1) {
      expect(onCellCenter(p.points[i]!.x), dump(p.points)).toBe(true);
      expect(onCellCenter(p.points[i]!.y), dump(p.points)).toBe(true);
    }
    expect(BOARD_GRID).toBe(32);
  });

  it("prefers a 45° run over HVH when the stubs have a square", () => {
    const p = audioStepPath(16, 80, 240, 176);
    expect(hasDiagonal(p.points), dump(p.points)).toBe(true);
    expect(isOctilinearPoints(p.points), dump(p.points)).toBe(true);
  });

  it("puts a second mid/side tube on a parallel rail, not on top of the first", () => {
    const mid = audioStepPath(236, 40, 500, 200);
    const side = audioStepPath(236, 72, 500, 240, {
      reservedXs: verticalRails(mid.points),
      reservedPaths: [mid.points],
    });
    expect(railsOverlap(mid.points, side.points), `${dump(mid.points)} || ${dump(side.points)}`).toBe(false);
    expect(stubGoesEast(side.points[0]!, side.points[1]!)).toBe(true);
  });

  it("does not stack two same-row tubes on one glow", () => {
    const a = audioStepPath(0, 80, 400, 80);
    const b = audioStepPath(0, 80, 400, 200, { reservedPaths: [a.points], reservedXs: verticalRails(a.points) });
    expect(railsOverlap(a.points, b.points), `${dump(a.points)} || ${dump(b.points)}`).toBe(false);
  });

  it("does not Z-jog when the handles sit closer than two stubs", () => {
    const p = audioStepPath(144, 176, 272, 208);
    expect(isOctilinearPoints(p.points), dump(p.points)).toBe(true);
    expect(turnsAreOctilinear(p.points), dump(p.points)).toBe(true);
    expect(hasDiagonal(p.points), dump(p.points)).toBe(true);
    const xs = p.points.map((pt) => pt.x);
    expect(Math.min(...xs), dump(p.points)).toBeGreaterThanOrEqual(142 - 0.5);
    expect(stubGoesEast(p.points[0]!, p.points[1]!)).toBe(true);
    expect(stubGoesEast(p.points[p.points.length - 2]!, p.points[p.points.length - 1]!)).toBe(true);
  });

  it("goes around a chip instead of through it", () => {
    const block = { id: "chip", x: 80, y: 20, w: 80, h: 40 };
    const p = audioStepPath(0, 40, 240, 40, {
      obstacles: [block],
      sourceId: "src",
      targetId: "dst",
    });
    expect(midHits(p.points, [inflate(block, TUBE_CLEAR)]), dump(p.points)).toBe(false);
    expect(p.points.some((pt) => Math.abs(pt.y - 40) > 8), dump(p.points)).toBe(true);
    expect(stubGoesEast(p.points[0]!, p.points[1]!), dump(p.points)).toBe(true);
    expect(isOctilinearPoints(p.points), dump(p.points)).toBe(true);
  });

  it("goes around a tall chip when the dest jack sits below-left", () => {
    const filter = { id: "filter", x: 200, y: 40, w: 236, h: 200 };
    const drive = { id: "drive", x: 200, y: 280, w: 236, h: 80 };
    const p = audioStepPath(436, 80, 200, 320, {
      obstacles: [filter, drive],
      sourceId: "filter",
      targetId: "drive",
    });
    expect(midHits(p.points, [inflate(filter, TUBE_CLEAR), inflate(drive, TUBE_CLEAR)]), dump(p.points)).toBe(false);
    expect(stubGoesEast(p.points[0]!, p.points[1]!), dump(p.points)).toBe(true);
    expect(stubGoesEast(p.points[p.points.length - 2]!, p.points[p.points.length - 1]!), dump(p.points)).toBe(true);
  });

  it("uses the handle Position React Flow already knows", () => {
    const p = audioStepPath(80, 20, 80, 200, {
      sourcePosition: Position.Right,
      targetPosition: Position.Left,
    });
    expect(p.d.startsWith("M")).toBe(true);
    expect(firstLast(p.points).first).toEqual({ x: 80, y: 20 });
    expect(firstLast(p.points).last).toEqual({ x: 80, y: 200 });
  });
});
