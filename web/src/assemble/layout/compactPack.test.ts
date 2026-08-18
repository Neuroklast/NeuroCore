import { describe, expect, it } from "vitest";
import { BOARD_GRID } from "../grid";
import { chipChipGap } from "../elkArrange";
import { packRows, rowCount } from "./compactPack";
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
  it("wraps a long chain into two column-aligned rows", () => {
    const { nodes, edges } = chain(5);
    const packed = packRows(nodes, edges, { w: 2400, h: 420 });
    expect(packed.IN!.y).toBeLessThan(packed.OUT!.y);
    expect(packed.OUT!.y - packed.IN!.y).toBeGreaterThanOrEqual(BOARD_GRID * 3);
    expect(packed.IN!.x).toBe(packed.c3!.x);
    expect(packed.c0!.x).toBe(packed.c4!.x);
    expect(packed.c1!.x).toBe(packed.OUT!.x);
    const gap = packed.c0!.x - (packed.IN!.x + packed.IN!.w);
    expect(gap).toBeGreaterThanOrEqual(BOARD_GRID);
  });

  it("keeps at least one grid of air on every side, including stacked rows", () => {
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
        expect(chipChipGap(a, b), `${a.id}↔${b.id}`).toBeGreaterThanOrEqual(BOARD_GRID);
      }
    }
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
