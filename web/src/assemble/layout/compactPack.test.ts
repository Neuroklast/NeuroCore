import { describe, expect, it } from "vitest";
import { BOARD_GRID } from "../grid";
import { chipChipGap } from "../elkArrange";
import { packRows, rowCount, WRAP_AIR, wrapFits } from "./compactPack";
import type { LayoutEdge, LayoutNode } from "./types";

function chain(n: number): { nodes: LayoutNode[]; edges: LayoutEdge[] } {
  const jack = (id: string, y: number) => ({ id, y });
  const nodes: LayoutNode[] = [
    { id: "IN", w: 128, h: 64, ins: [], outs: [jack("out", 48)] },
  ];
  const edges: LayoutEdge[] = [];
  let prev = "IN";
  for (let i = 0; i < n; i += 1) {
    const id = `c${i}`;
    nodes.push({
      id,
      w: 256,
      h: 96,
      ins: [jack("in", 48)],
      outs: [jack("out", 48)],
    });
    edges.push({
      id: `e${i}`,
      source: prev,
      target: id,
      fromJack: "out",
      toJack: "in",
    });
    prev = id;
  }
  nodes.push({ id: "OUT", w: 128, h: 64, ins: [jack("in", 48)], outs: [] });
  edges.push({
    id: `e${n}`,
    source: prev,
    target: "OUT",
    fromJack: "out",
    toJack: "in",
  });
  return { nodes, edges };
}

describe("compact packRows", () => {
  it("wraps L→R when the chain is wider than the view, no snake", () => {
    expect(wrapFits([128, 256, 256, 256], 400, 64, 32)).toBe(1);
    expect(wrapFits([128, 256], 2000, 64, 32)).toBe(2);
    const { nodes, edges } = chain(5);
    const packed = packRows(nodes, edges, { w: 700, h: 420 });
    expect(rowCount(packed)).toBeGreaterThan(1);
    expect(packed.IN!.y).toBeLessThan(packed.OUT!.y);
    expect(packed.IN!.x).toBeLessThanOrEqual(packed.c1!.x);
    expect(packed.c1!.x).toBeLessThanOrEqual(packed.c3!.x);
    expect(packed.IN!.x).toBe(packed.c1!.x);
    const gap = packed.c0!.x - (packed.IN!.x + packed.IN!.w);
    expect(gap).toBeGreaterThanOrEqual(BOARD_GRID * 2);
  });

  it("keeps two grids of air horizontally and one grid vertically", () => {
    const { nodes, edges } = chain(5);
    nodes[2]!.h = 192;
    nodes[2]!.ins = [{ id: "in", y: 16 }];
    nodes[2]!.outs = [{ id: "out", y: 16 }];
    nodes[4]!.ins = [{ id: "in", y: 80 }];
    nodes[4]!.outs = [{ id: "out", y: 80 }];
    const packed = packRows(nodes, edges, { w: 960, h: 420 });
    const rects = Object.entries(packed).map(([id, p]) => ({ id, ...p }));
    for (let i = 0; i < rects.length; i += 1) {
      for (let j = i + 1; j < rects.length; j += 1) {
        const a = rects[i]!;
        const b = rects[j]!;
        const ovY = Math.min(a.y + a.h, b.y + b.h) - Math.max(a.y, b.y);
        const need = ovY > 0 ? BOARD_GRID * 2 : BOARD_GRID;
        expect(chipChipGap(a, b), `${a.id}↔${b.id}`).toBeGreaterThanOrEqual(need);
      }
    }
  });

  it("leaves pad+rail+pad between wrapped rows so a cable can sit off both chips", () => {
    const { nodes, edges } = chain(5);
    const packed = packRows(nodes, edges, { w: 700, h: 420 });
    expect(rowCount(packed)).toBeGreaterThan(1);
    const list = Object.values(packed);
    const first = [...list].sort((a, b) => a.y - b.y)[0]!;
    const upper = list.filter((p) => Math.min(p.y + p.h, first.y + first.h) - Math.max(p.y, first.y) > 0);
    const upperBottom = Math.max(...upper.map((p) => p.y + p.h));
    const lower = list.filter((p) => p.y >= upperBottom);
    expect(lower.length).toBeGreaterThan(0);
    const gap = Math.min(...lower.map((p) => p.y)) - upperBottom;
    expect(gap, `row gap ${gap}`).toBeGreaterThanOrEqual(WRAP_AIR);
  });

  it("keeps a short chain on one row and snaps to 32", () => {
    const { nodes, edges } = chain(1);
    const packed = packRows(nodes, edges, { w: 1200, h: 420 });
    expect(rowCount(packed)).toBe(1);
    expect(packed.IN!.x).toBeLessThan(packed.c0!.x);
    expect(packed.c0!.x).toBeLessThan(packed.OUT!.x);
    for (const n of Object.values(packed)) {
      expect(n.x % BOARD_GRID).toBe(0);
      expect(n.y % BOARD_GRID).toBe(0);
    }
  });
});
