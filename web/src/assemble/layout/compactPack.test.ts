import { describe, expect, it } from "vitest";
import { BOARD_GRID } from "../grid";
import { chipChipGap } from "../elkArrange";
import { flowFromAst } from "../flowFromAst";
import { findFactory } from "../../presets/factoryCatalog";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { packRows, rowCount, WRAP_AIR, wrapFits } from "./compactPack";
import { flowToLayout } from "./fromFlow";
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

  it("puts IN.out and the first DSP in on the same global Y so Compact cannot knick the first tube", () => {
    const row = findFactory("Airy Clean");
    expect(row, "missing Airy Clean").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const { nodes, edges } = flowFromAst(doc);
    const layout = flowToLayout(nodes, edges);
    const packed = packRows(layout.nodes, layout.edges, { w: 1200, h: 420 });
    for (const p of Object.values(packed)) {
      expect(p.x % BOARD_GRID).toBe(0);
      expect(p.y % BOARD_GRID).toBe(0);
    }
    const hop = layout.edges.find((e) => e.source === "IN" && e.fromJack === "out");
    expect(hop, "IN.out hop").toBeTruthy();
    const inn = layout.nodes.find((n) => n.id === "IN")!;
    const dsp = layout.nodes.find((n) => n.id === hop!.target)!;
    const inJack = packed.IN!.y + (inn.outs.find((p) => p.id === "out")?.y ?? 0);
    const dspJack = packed[hop!.target]!.y + (dsp.ins.find((p) => p.id === hop!.toJack)?.y ?? dsp.ins[0]!.y);
    expect(inJack, `IN.out ${inJack} vs ${hop!.target}.in ${dspJack}`).toBe(dspJack);
  });

  it("parks OUT as the unique last chip: nothing to the right, last row", () => {
    const { nodes, edges } = chain(5);
    const packed = packRows(nodes, edges, { w: 700, h: 420 });
    const out = packed.OUT!;
    for (const [id, p] of Object.entries(packed)) {
      if (id === "OUT") {
        continue;
      }
      expect(p.x + p.w, `${id} must sit left of OUT`).toBeLessThanOrEqual(out.x);
      expect(p.y, `${id} must not sit below OUT`).toBeLessThanOrEqual(out.y);
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

  it("keeps a send bus on its own row and OUT at the bottom-right", () => {
    const jack = (id: string, y: number) => ({ id, y });
    const n = (
      id: string,
      rail: string,
      w = 256,
      h = 96,
    ): LayoutNode => ({
      id,
      w,
      h,
      rail,
      ins: id === "IN" ? [] : [jack("in", 48)],
      outs: id === "OUT" ? [] : [jack("out", 48)],
    });
    const nodes: LayoutNode[] = [
      n("IN", "main", 128, 64),
      n("stage1", "main"),
      n("dirt", "dirt"),
      n("send", "dirt"),
      n("stage2", "dirt"),
      n("OUT", "out", 128, 96),
    ];
    const edges: LayoutEdge[] = [
      { id: "e0", source: "IN", target: "stage1", fromJack: "out", toJack: "in" },
      { id: "e1", source: "stage1", target: "OUT", fromJack: "out", toJack: "main" },
      { id: "e2", source: "IN", target: "dirt", fromJack: "out", toJack: "in" },
      { id: "e3", source: "dirt", target: "send", fromJack: "out", toJack: "in" },
      { id: "e4", source: "send", target: "stage2", fromJack: "out", toJack: "in" },
      { id: "e5", source: "stage2", target: "OUT", fromJack: "out", toJack: "dirt" },
    ];
    const packed = packRows(nodes, edges, { w: 960, h: 420 });
    expect(packed.dirt!.y).toBeGreaterThan(packed.IN!.y);
    expect(packed.send!.y).toBe(packed.dirt!.y);
    expect(packed.stage2!.y).toBe(packed.dirt!.y);
    expect(packed.stage1!.y).toBe(packed.IN!.y);
    expect(packed.OUT!.x).toBeGreaterThanOrEqual(packed.stage1!.x + packed.stage1!.w);
    expect(packed.OUT!.x).toBeGreaterThanOrEqual(packed.stage2!.x + packed.stage2!.w);
    expect(packed.OUT!.y).toBeGreaterThanOrEqual(packed.IN!.y);
    for (const id of ["IN", "stage1", "dirt", "send", "stage2"]) {
      const p = packed[id]!;
      expect(p.x < packed.OUT!.x || p.y < packed.OUT!.y, `${id} is not left or above OUT`).toBe(true);
    }
  });

  it("compacts Far Plane with the hall send row below main and OUT on the right", () => {
    const row = findFactory("Far Plane");
    expect(row, "missing Far Plane").toBeTruthy();
    const { doc } = parseDslSketch(row!.script);
    const { nodes, edges } = flowFromAst(doc);
    const layout = flowToLayout(nodes, edges);
    const packed = packRows(layout.nodes, layout.edges, { w: 960, h: 420 });
    const send = nodes.find((n) => n.data.type === "send")!.id;
    const bus = nodes.find((n) => n.data.type === "bus")!.id;
    const out = nodes.find((n) => n.data.type === "out")!.id;
    expect(packed[bus]!.y).toBeGreaterThan(packed.IN!.y);
    expect(packed[send]!.y).toBe(packed[bus]!.y);
    expect(packed[out]!.x).toBeGreaterThan(packed.IN!.x);
    expect(packed[out]!.x).toBeGreaterThanOrEqual(packed[send]!.x);
    for (const [id, p] of Object.entries(packed)) {
      if (id === out) {
        continue;
      }
      expect(p.x < packed[out]!.x || p.y < packed[out]!.y, `${id} must sit left or above OUT`).toBe(true);
    }
  });
});
