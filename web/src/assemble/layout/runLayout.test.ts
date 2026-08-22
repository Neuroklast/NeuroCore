import { describe, expect, it } from "vitest";
import { BOARD_GRID } from "../grid";
import { chipBox, jackAnchor } from "../chipLayout";
import { flowFromAst } from "../flowFromAst";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { findFactory } from "../../presets/factoryCatalog";
import { handleId } from "../handles";
import { firstLastHorizontal, hasLightning } from "./chamfer";
import { packRows, rowCount } from "./compactPack";
import { arrange, bboxArea, compact, pathLength, pinJackStubs, reroute } from "./runLayout";
import { requestLayout } from "./layoutClient";
import type { LayoutEdge, LayoutNode } from "./types";

function chain(): { nodes: LayoutNode[]; edges: LayoutEdge[] } {
  const jack = (id: string, y: number) => ({ id, y });
  const nodes: LayoutNode[] = [
    { id: "IN", w: 128, h: 64, ins: [], outs: [jack("out", 16)] },
    { id: "a", w: 256, h: 96, ins: [jack("in", 48)], outs: [jack("out", 48)] },
    { id: "OUT", w: 128, h: 64, ins: [jack("in", 16)], outs: [] },
  ];
  const edges: LayoutEdge[] = [
    { id: "e0", source: "IN", target: "a", fromJack: "out", toJack: "in" },
    { id: "e1", source: "a", target: "OUT", fromJack: "out", toJack: "in" },
  ];
  return { nodes, edges };
}

function hasWestThenEastHook(pts: Array<{ x: number; y: number }>): boolean {
  for (let i = 0; i < pts.length - 2; i += 1) {
    const a = pts[i]!;
    const b = pts[i + 1]!;
    const c = pts[i + 2]!;
    const sameY = Math.abs(a.y - b.y) < 0.6 && Math.abs(b.y - c.y) < 0.6;
    if (sameY && b.x < a.x - 0.5 && c.x > b.x + 0.5) {
      return true;
    }
  }
  return false;
}

describe("dest jack stubs", () => {
  it("pins the screenshot OUT hook so the last step is 32 px east and never past dest", () => {
    const from = { x: 608, y: 560 };
    const to = { x: 736, y: 528 };
    const hooked = [
      from,
      { x: 720, y: 560 },
      { x: 720, y: 528 },
      { x: 704, y: 528 },
      to,
    ];
    expect(hasWestThenEastHook(hooked)).toBe(true);
    const pts = pinJackStubs(from, to, hooked);
    expect(hasWestThenEastHook(pts)).toBe(false);
    expect(firstLastHorizontal(pts)).toBe(true);
    expect(Math.max(...pts.map((p) => p.x))).toBeCloseTo(to.x, 5);
    expect(pts[pts.length - 1]).toEqual(to);
    expect(pts[0]).toEqual(from);
    expect(pts[1]!.x - pts[0]!.x).toBeGreaterThanOrEqual(BOARD_GRID - 0.5);
    expect(pts[pts.length - 1]!.x - pts[pts.length - 2]!.x).toBeGreaterThanOrEqual(BOARD_GRID - 0.5);
    expect(pts.every((p) => p.x <= to.x + 0.6)).toBe(true);
  });

  it("does not U-turn when A* arrives at OUT from the south", async () => {
    const nodes: LayoutNode[] = [
      { id: "a", x: 0, y: 32, w: 256, h: 96, ins: [], outs: [{ id: "out", y: 80 }] },
      { id: "OUT", x: 320, y: 0, w: 128, h: 96, ins: [{ id: "in", y: 48 }], outs: [] },
    ];
    const edges: LayoutEdge[] = [
      { id: "e", source: "a", target: "OUT", fromJack: "out", toJack: "in" },
    ];
    const r = await reroute(nodes, edges);
    const pts = parsePath(r.edgePaths.e!);
    expect(pts.length).toBeGreaterThanOrEqual(2);
    expect(firstLastHorizontal(pts), r.edgePaths.e).toBe(true);
    expect(hasWestThenEastHook(pts), r.edgePaths.e).toBe(false);
    expect(Math.max(...pts.map((p) => p.x)), r.edgePaths.e).toBeLessThanOrEqual(320.6);
    expect(Math.abs(pts[pts.length - 1]!.x - 320)).toBeLessThan(1);
  });
});

describe("arrange vs compact", () => {
  it("snaps every origin and size to 32 and keeps compact smaller", async () => {
    const { nodes, edges } = chain();
    const wide = await arrange(nodes, edges);
    const tight = await compact(nodes, edges);
    for (const pack of [wide, tight]) {
      for (const n of Object.values(pack.nodes)) {
        expect(n.x % BOARD_GRID).toBe(0);
        expect(n.y % BOARD_GRID).toBe(0);
        expect(n.w % BOARD_GRID).toBe(0);
        expect(n.h % BOARD_GRID).toBe(0);
      }
      for (const d of Object.values(pack.edgePaths)) {
        expect(d.startsWith("M")).toBe(true);
        expect(/[QC]/.test(d)).toBe(false);
      }
    }
    expect(bboxArea(tight.nodes)).toBeLessThanOrEqual(bboxArea(wide.nodes));
  });

  it("still emits a path when every channel is tight", async () => {
    const { nodes, edges } = chain();
    const r = await compact(nodes, edges);
    expect(r.edgePaths.e0).toBeTruthy();
    expect(r.edgePaths.e1).toBeTruthy();
  });
});

describe("compact wrap rail", () => {
  it("sends the wrap cable between the two rows, not around the hull", async () => {
    const jack = (id: string, y: number) => ({ id, y });
    const nodes: LayoutNode[] = [
      { id: "IN", w: 128, h: 64, ins: [], outs: [jack("out", 48)] },
    ];
    const edges: LayoutEdge[] = [];
    let prev = "IN";
    for (let i = 0; i < 5; i += 1) {
      const id = `c${i}`;
      nodes.push({
        id,
        w: 256,
        h: 96,
        ins: [jack("in", 48)],
        outs: [jack("out", 48)],
      });
      edges.push({ id: `e${i}`, source: prev, target: id, fromJack: "out", toJack: "in" });
      prev = id;
    }
    nodes.push({ id: "OUT", w: 128, h: 64, ins: [jack("in", 48)], outs: [] });
    edges.push({ id: "e5", source: prev, target: "OUT", fromJack: "out", toJack: "in" });
    const view = { w: 700, h: 420 };
    const packed = packRows(nodes, edges, view);
    expect(rowCount(packed)).toBeGreaterThan(1);
    const r = await compact(nodes, edges, view);
    const wrap = edges.find((e) => {
      const s = r.nodes[e.source];
      const t = r.nodes[e.target];
      return s && t && t.x < s.x && t.y > s.y + s.h * 0.5;
    });
    expect(wrap, "a wrap edge").toBeTruthy();
    const pts = parsePath(r.edgePaths[wrap!.id]!);
    const s = r.nodes[wrap!.source]!;
    const t = r.nodes[wrap!.target]!;
    const minX = Math.min(...Object.values(r.nodes).map((n) => n.x));
    const maxY = Math.max(...Object.values(r.nodes).map((n) => n.y + n.h));
    const railHits = pts.filter((p) => p.y >= s.y + s.h && p.y <= t.y);
    expect(railHits.length, r.edgePaths[wrap!.id]).toBeGreaterThan(0);
    expect(Math.min(...pts.map((p) => p.x))).toBeGreaterThanOrEqual(minX - BOARD_GRID);
    expect(Math.max(...pts.map((p) => p.y))).toBeLessThanOrEqual(maxY + BOARD_GRID);
    expect(packed.IN!.x).toBe(packed[wrap!.target]?.x ?? packed.IN!.x);
  });
});

function parsePath(d: string): Array<{ x: number; y: number }> {
  const pts: Array<{ x: number; y: number }> = [];
  const re = /[ML]\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)/g;
  let m: RegExpExecArray | null = re.exec(d);
  while (m) {
    pts.push({ x: Number(m[1]), y: Number(m[2]) });
    m = re.exec(d);
  }
  return pts;
}

describe("layout lands on jacks", () => {
  it("starts and ends every audio path on the painted jack", async () => {
    const preset = findFactory("Airy Clean");
    expect(preset).toBeTruthy();
    const { doc } = parseDslSketch(preset!.script);
    const { nodes, edges } = flowFromAst(doc);
    const laid = await requestLayout("ARRANGE", nodes, edges, { w: 960, h: 420 });
    for (const e of edges.filter((x) => x.className !== "temp")) {
      const d = laid.edgePaths[e.id];
      expect(d, e.id).toBeTruthy();
      const pts = parsePath(d!);
      expect(pts.length, e.id).toBeGreaterThanOrEqual(2);
      expect(pathLength(pts), d).toBeGreaterThanOrEqual(BOARD_GRID);
      expect(hasLightning(pts), d).toBe(false);
      expect(firstLastHorizontal(pts), d).toBe(true);
      expect(hasWestThenEastHook(pts), d).toBe(false);
      expect(Math.max(...pts.map((p) => p.x)), d).toBeLessThanOrEqual(pts[pts.length - 1]!.x + 0.6);
      const src = nodes.find((n) => n.id === e.source)!;
      const dst = nodes.find((n) => n.id === e.target)!;
      const sp = laid.nodes[src.id]!;
      const dp = laid.nodes[dst.id]!;
      const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
      const db = chipBox(dst.data.type, dst.data.jacks, false, dst.data.args);
      const a = jackAnchor(sp, src.data.type, src.data.jacks, e.sourceHandle, true, sp.h || sb.h, sp.w || sb.w);
      const b = jackAnchor(dp, dst.data.type, dst.data.jacks, e.targetHandle, false, dp.h || db.h, dp.w || db.w);
      expect(Math.abs(pts[0]!.x - a.x) + Math.abs(pts[0]!.y - a.y), `src ${e.id} ${d}`).toBeLessThan(2);
      expect(Math.abs(pts[pts.length - 1]!.x - b.x) + Math.abs(pts[pts.length - 1]!.y - b.y), `dst ${e.id} ${d}`).toBeLessThan(2);
    }
  });

  it("routes the Classic Tremolo LFO onto the dest mod jack", async () => {
    const preset = findFactory("Classic Tremolo");
    expect(preset).toBeTruthy();
    const { doc } = parseDslSketch(preset!.script);
    const { nodes, edges } = flowFromAst(doc);
    const mod = edges.find((e) => (e.data as { kind?: string } | undefined)?.kind === "mod");
    expect(mod, "LFO edge").toBeTruthy();
    const laid = await requestLayout("COMPACT", nodes, edges, { w: 960, h: 420 });
    const d = laid.edgePaths[mod!.id];
    expect(d).toBeTruthy();
    expect(/[QC]/.test(d!)).toBe(false);
    const pts = parsePath(d!);
    expect(pathLength(pts)).toBeGreaterThanOrEqual(BOARD_GRID);
    expect(hasLightning(pts)).toBe(false);
    expect(firstLastHorizontal(pts)).toBe(true);
    const src = nodes.find((n) => n.id === mod!.source)!;
    const dst = nodes.find((n) => n.id === mod!.target)!;
    const sp = laid.nodes[src.id]!;
    const dp = laid.nodes[dst.id]!;
    const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
    const db = chipBox(dst.data.type, dst.data.jacks, false, dst.data.args);
    const a = jackAnchor(sp, src.data.type, src.data.jacks, mod!.sourceHandle, true, sp.h || sb.h, sp.w || sb.w);
    const b = jackAnchor(dp, dst.data.type, dst.data.jacks, mod!.targetHandle, false, dp.h || db.h, dp.w || db.w);
    expect(Math.abs(pts[0]!.x - a.x) + Math.abs(pts[0]!.y - a.y)).toBeLessThan(2);
    expect(Math.abs(pts[pts.length - 1]!.x - b.x) + Math.abs(pts[pts.length - 1]!.y - b.y)).toBeLessThan(2);
    expect(handleId("mod", true) === mod!.sourceHandle || String(mod!.sourceHandle).includes("mod")).toBe(true);
  });

  it("routes MS encode mid and side as parallel rails onto the jacks", async () => {
    const preset = findFactory("MS Side Air");
    expect(preset).toBeTruthy();
    const { doc } = parseDslSketch(preset!.script);
    const { nodes, edges } = flowFromAst(doc);
    const mid = edges.find((e) => String(e.sourceHandle).includes("mid") || String(e.targetHandle).includes("mid"));
    const side = edges.find((e) => String(e.sourceHandle).includes("side") || String(e.targetHandle).includes("side"));
    expect(mid, "mid rail").toBeTruthy();
    expect(side, "side rail").toBeTruthy();
    const laid = await requestLayout("ARRANGE", nodes, edges, { w: 960, h: 420 });
    for (const e of [mid!, side!]) {
      const d = laid.edgePaths[e.id];
      expect(d, e.id).toBeTruthy();
      const pts = parsePath(d!);
      expect(hasLightning(pts), d).toBe(false);
      expect(firstLastHorizontal(pts), d).toBe(true);
      const src = nodes.find((n) => n.id === e.source)!;
      const dst = nodes.find((n) => n.id === e.target)!;
      const sp = laid.nodes[src.id]!;
      const dp = laid.nodes[dst.id]!;
      const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
      const db = chipBox(dst.data.type, dst.data.jacks, false, dst.data.args);
      const a = jackAnchor(sp, src.data.type, src.data.jacks, e.sourceHandle, true, sp.h || sb.h, sp.w || sb.w);
      const b = jackAnchor(dp, dst.data.type, dst.data.jacks, e.targetHandle, false, dp.h || db.h, dp.w || db.w);
      expect(Math.abs(pts[0]!.x - a.x) + Math.abs(pts[0]!.y - a.y), `src ${e.id} ${d}`).toBeLessThan(2);
      expect(Math.abs(pts[pts.length - 1]!.x - b.x) + Math.abs(pts[pts.length - 1]!.y - b.y), `dst ${e.id} ${d}`).toBeLessThan(2);
    }
    const midPts = parsePath(laid.edgePaths[mid!.id]!);
    const sidePts = parsePath(laid.edgePaths[side!.id]!);
    expect(Math.abs(midPts[0]!.y - sidePts[0]!.y)).toBeGreaterThanOrEqual(BOARD_GRID);
    const cell = (p: { x: number; y: number }) => `${Math.floor(p.x / BOARD_GRID)},${Math.floor(p.y / BOARD_GRID)}`;
    const midCells = new Set(midPts.slice(1, -1).map(cell));
    const shared = sidePts.slice(1, -1).filter((p) => midCells.has(cell(p)));
    expect(shared, "mid/side share a grid cell").toEqual([]);
    const split = nodes.find((n) => n.id === "ms1" || n.data.type === "ms" || n.data.type === "split_ms");
    const outEdges = edges.filter((e) => e.source === split?.id);
    expect(outEdges.length).toBeGreaterThanOrEqual(2);
    const y0 = outEdges.map((e) => parsePath(laid.edgePaths[e.id]!)[1]?.y ?? 0);
    expect(Math.abs(y0[0]! - y0[1]!), "split outs must leave on different rails").toBeGreaterThanOrEqual(BOARD_GRID);
  });

  it("routes a multiband xover to low/mid/high without overlapping rails", async () => {
    const preset = findFactory("De-Ess Shelf");
    expect(preset).toBeTruthy();
    const { doc } = parseDslSketch(preset!.script);
    const { nodes, edges } = flowFromAst(doc);
    const xover = nodes.find((n) => n.data.type.startsWith("xover") || n.id.startsWith("xover"));
    expect(xover, "xover chip").toBeTruthy();
    const outs = edges.filter((e) => e.source === xover!.id);
    const jacks = outs.map((e) => String(e.sourceHandle));
    expect(jacks.some((h) => h.includes("low"))).toBe(true);
    expect(jacks.some((h) => h.includes("high"))).toBe(true);
    const laid = await requestLayout("ARRANGE", nodes, edges, { w: 960, h: 420 });
    const starts: number[] = [];
    for (const e of outs) {
      const d = laid.edgePaths[e.id];
      expect(d, e.id).toBeTruthy();
      const pts = parsePath(d!);
      expect(hasLightning(pts), d).toBe(false);
      expect(firstLastHorizontal(pts), d).toBe(true);
      starts.push(pts[0]!.y);
      const src = nodes.find((n) => n.id === e.source)!;
      const sp = laid.nodes[src.id]!;
      const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
      const a = jackAnchor(sp, src.data.type, src.data.jacks, e.sourceHandle, true, sp.h || sb.h, sp.w || sb.w);
      expect(Math.abs(pts[0]!.x - a.x) + Math.abs(pts[0]!.y - a.y), `xover ${e.sourceHandle} ${d}`).toBeLessThan(2);
    }
    const uniq = [...new Set(starts.map((y) => Math.round(y)))];
    expect(uniq.length).toBeGreaterThanOrEqual(2);
    const bus = nodes.find((n) => n.data.type === "bus");
    if (bus) {
      expect(edges.some((e) => e.source === bus.id || e.target === bus.id), "bus has a cable").toBe(true);
      const hit = edges.filter((e) => e.source === bus.id || e.target === bus.id);
      for (const e of hit) {
        expect(laid.edgePaths[e.id], e.id).toBeTruthy();
      }
    }
  });
});
