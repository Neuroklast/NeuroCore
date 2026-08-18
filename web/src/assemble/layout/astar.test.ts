import { describe, expect, it } from "vitest";
import { BOARD_GRID } from "../grid";
import { astarRoute, hvhFallback } from "./astar";
import { chamferWaypoints, waypointToSvgPath } from "./chamfer";
import { GridMap, portInCell, portOutCell } from "./gridMap";
import { DIR_E } from "./types";

describe("astar grid router", () => {
  it("routes a same-row east-west hop as one horizontal through midlines", () => {
    const map = new GridMap();
    map.expandTo(0, 0);
    map.expandTo(12, 4);
    map.finishHalo();
    const start = { c: 2, r: 2 };
    const goal = { c: 10, r: 2 };
    const cells = astarRoute(map, start, goal, DIR_E);
    expect(cells).toBeTruthy();
    expect(cells!.every((c) => c.r === 2)).toBe(true);
    const turns = cells!.filter((_, i) => {
      if (i === 0 || i === cells!.length - 1) {
        return false;
      }
      return cells![i].c - cells![i - 1].c !== cells![i + 1].c - cells![i].c
        || cells![i].r - cells![i - 1].r !== cells![i + 1].r - cells![i].r;
    });
    expect(turns.length).toBe(0);
  });

  it("goes around a solid chip instead of through it", () => {
    const map = new GridMap();
    map.markNode({ id: "b", w: 96, h: 64, ins: [], outs: [] }, 96, 32);
    const start = portOutCell(0, 64, 48);
    const goal = portInCell(224, 48);
    map.markExitStub(start.c, start.r);
    map.markEntryStub(goal.c, goal.r);
    map.finishHalo();
    const cells = astarRoute(map, start, goal, DIR_E);
    expect(cells).toBeTruthy();
    expect(cells!.some((c) => map.solid.has(map.key(c.c, c.r)))).toBe(false);
    expect(cells!.some((c) => c.r !== start.r)).toBe(true);
  });

  it("keeps a second cable on a parallel cell, not the same cell", () => {
    const map = new GridMap();
    map.expandTo(0, 0);
    map.expandTo(14, 8);
    map.finishHalo();
    const a = astarRoute(map, { c: 1, r: 2 }, { c: 12, r: 2 }, DIR_E)!;
    map.occupy(a, []);
    const b = astarRoute(map, { c: 1, r: 3 }, { c: 12, r: 5 }, DIR_E)!;
    const shared = b.filter((c) => a.some((p) => p.c === c.c && p.r === c.r));
    expect(shared.length).toBe(0);
    const ay = a[Math.floor(a.length / 2)]!.r;
    const by = b[Math.floor(b.length / 2)]!.r;
    expect(Math.abs(ay - by)).toBeGreaterThanOrEqual(1);
  });

  it("falls back to HVH when boxed in", () => {
    const pts = hvhFallback({ x: 64, y: 48 }, { x: 32, y: 80 });
    expect(pts[0]).toEqual({ x: 64, y: 48 });
    expect(pts[pts.length - 1]).toEqual({ x: 32, y: 80 });
    const d = waypointToSvgPath(chamferWaypoints(pts));
    expect(d.startsWith("M")).toBe(true);
  });

  it("treats the one-cell ring around a chip as solid except port stubs", () => {
    const map = new GridMap();
    map.markNode({ id: "b", w: 96, h: 64, ins: [], outs: [] }, 96, 32);
    const start = portOutCell(0, 64, 48);
    const goal = portInCell(224, 48);
    map.markExitStub(start.c, start.r);
    map.markEntryStub(goal.c, goal.r);
    map.finishHalo();
    expect(map.solid.has(map.key(3, 0))).toBe(true);
    expect(map.solid.has(map.key(start.c, start.r))).toBe(false);
    const cells = astarRoute(map, start, goal, DIR_E);
    expect(cells).toBeTruthy();
    expect(cells!.some((c) => map.solid.has(map.key(c.c, c.r)))).toBe(false);
  });

  it("keeps two port runways from stacking or T-joining", () => {
    const map = new GridMap();
    map.expandTo(0, 0);
    map.expandTo(16, 8);
    const a0 = { c: 2, r: 2 };
    const a1 = { c: 12, r: 2 };
    const b0 = { c: 2, r: 3 };
    const b1 = { c: 12, r: 5 };
    map.reserveRunway("A", a0, 1, 3);
    map.reserveRunway("A", a1, -1, 3);
    map.reserveRunway("B", b0, 1, 3);
    map.reserveRunway("B", b1, -1, 3);
    map.finishHalo();
    const a = astarRoute(map, a0, a1, DIR_E, false, "A")!;
    expect(a).toBeTruthy();
    map.occupy(a, []);
    const b = astarRoute(map, b0, b1, DIR_E, false, "B")!;
    expect(b).toBeTruthy();
    const shared = b.filter((c) => a.some((p) => p.c === c.c && p.r === c.r));
    expect(shared).toEqual([]);
    expect(b.slice(0, 3).every((c) => c.r === 3)).toBe(true);
  });

  it("maps ports to the cell just outside the face", () => {
    expect(portOutCell(0, 64, 48)).toEqual({ c: 2, r: 1 });
    expect(portInCell(192, 48)).toEqual({ c: 5, r: 1 });
    expect(BOARD_GRID).toBe(32);
  });
});
