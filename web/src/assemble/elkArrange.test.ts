import { describe, expect, it } from "vitest";
import type { Edge, Node } from "./flowTypes";
import { arrangeElk, ARR_CHIP_GAP, ARR_LAYER_GAP, chipChipGap } from "./elkArrange";
import { chipBox } from "./chipLayout";
import { inflate, midHits } from "./tubePath";
import { requestLayout } from "./layout/layoutClient";
import type { ChipData } from "./flowFromAst";
import { BOARD_GRID, BOARD_PAD } from "./grid";

const jack = (id: string, output: boolean) => ({ id, label: id, output, kind: "audio" });

function chip(id: string, type: string, extra?: Partial<ChipData>): Node<ChipData> {
  return {
    id,
    type: type === "in" || type === "out" ? "io" : "chip",
    position: { x: 0, y: 0 },
    data: {
      label: id,
      type,
      jacks: extra?.jacks ?? [jack("in", false), jack("out", true)],
      letters: "",
      args: {},
      channel: "",
      summary: "",
      nodeId: id,
      ...extra,
    },
  };
}

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

describe("ELK arrange", () => {
  it("lays a short chain left to right on one row", async () => {
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      chip("stage1", "stage"),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const edges: Edge[] = [
      { id: "e1", source: "IN", target: "stage1", sourceHandle: "src::out", targetHandle: "dst::in" },
      { id: "e2", source: "stage1", target: "OUT", sourceHandle: "src::out", targetHandle: "dst::in" },
    ];
    const pos = await arrangeElk(nodes, edges);
    expect(pos.IN.x).toBeLessThan(pos.stage1.x);
    expect(pos.stage1.x).toBeLessThan(pos.OUT.x);
    expect(pos.IN.x % BOARD_GRID).toBe(0);
    expect(pos.stage1.x % BOARD_GRID).toBe(0);
    expect(Math.abs(pos.IN.y - pos.stage1.y)).toBeLessThanOrEqual(128);
    expect(Math.abs(pos.stage1.y - pos.OUT.y)).toBeLessThanOrEqual(128);
  });

  it("lays a long serial chain left to right, not a column wrap", async () => {
    const ids = ["filter1", "stage1", "eq1", "stage2", "filter2"];
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      ...ids.map((id) => chip(id, id.startsWith("eq") ? "eq" : id.startsWith("filter") ? "filter" : "stage")),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const chain = ["IN", ...ids, "OUT"];
    const edges: Edge[] = chain.slice(0, -1).map((src, i) => ({
      id: `e${i}`,
      source: src,
      target: chain[i + 1]!,
      sourceHandle: "src::out",
      targetHandle: "dst::in",
    }));
    const laid = await requestLayout("ARRANGE", nodes, edges);
    const pos = laid.nodes;
    expect(pos.filter1!.x).toBeLessThan(pos.eq1!.x);
    expect(pos.eq1!.x).toBeLessThan(pos.filter2!.x);
    expect(pos.IN!.x).toBeLessThan(pos.filter1!.x);
    expect(pos.OUT!.x).toBeGreaterThan(pos.filter2!.x);
    const fb = chipBox("filter", nodes.find((n) => n.id === "filter1")!.data.jacks, false, {});
    const layerGap = pos.stage1!.x - (pos.filter1!.x + fb.w);
    expect(layerGap).toBeGreaterThanOrEqual(ARR_LAYER_GAP);
    expect(pos.stage1!.x).not.toBe(pos.filter1!.x);
    for (const d of Object.values(laid.edgePaths)) {
      expect(d.startsWith("M")).toBe(true);
      expect(/[QC]/.test(d)).toBe(false);
    }
  });

  it("keeps chip↔chip clearance and never routes a tube through a foreign body", async () => {
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      chip("a", "stage"),
      chip("b", "filter"),
      chip("c", "stage"),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const edges: Edge[] = [
      { id: "e0", source: "IN", target: "a", sourceHandle: "src::out", targetHandle: "dst::in" },
      { id: "e1", source: "a", target: "b", sourceHandle: "src::out", targetHandle: "dst::in" },
      { id: "e2", source: "b", target: "c", sourceHandle: "src::out", targetHandle: "dst::in" },
      { id: "e3", source: "c", target: "OUT", sourceHandle: "src::out", targetHandle: "dst::in" },
    ];
    const laid = await requestLayout("ARRANGE", nodes, edges);
    const box = (id: string) => {
      const n = nodes.find((x) => x.id === id)!;
      const b = chipBox(n.data.type, n.data.jacks, false, n.data.args);
      const p = laid.nodes[id]!;
      return { id, x: p.x, y: p.y, w: p.w || b.w, h: p.h || b.h };
    };
    const A = box("a");
    const B = box("b");
    const C = box("c");
    expect(chipChipGap(A, B)).toBeGreaterThanOrEqual(ARR_CHIP_GAP);
    expect(chipChipGap(B, C)).toBeGreaterThanOrEqual(ARR_CHIP_GAP);
    for (const [id, d] of Object.entries(laid.edgePaths)) {
      const e = edges.find((x) => x.id === id);
      if (! e) {
        continue;
      }
      const pts = parsePath(d);
      const foreign = [A, B, C]
        .filter((n) => n.id !== e.source && n.id !== e.target)
        .map((n) => inflate(n, BOARD_PAD));
      expect(midHits(pts, foreign), `${id} through chip ${d}`).toBe(false);
    }
  });

  it("compact stacks a long chain into rows instead of one sausage", async () => {
    const ids = ["filter1", "stage1", "eq1", "stage2", "filter2"];
    const nodes = [
      chip("IN", "in", { jacks: [jack("out", true)] }),
      ...ids.map((id) => chip(id, id.startsWith("eq") ? "eq" : id.startsWith("filter") ? "filter" : "stage")),
      chip("OUT", "out", { jacks: [jack("in", false)] }),
    ];
    const chain = ["IN", ...ids, "OUT"];
    const edges: Edge[] = chain.slice(0, -1).map((src, i) => ({
      id: `e${i}`,
      source: src,
      target: chain[i + 1]!,
      sourceHandle: "src::out",
      targetHandle: "dst::in",
    }));
    const view = { w: 960, h: 420 };
    const wide = await requestLayout("ARRANGE", nodes, edges, view);
    const tight = await requestLayout("COMPACT", nodes, edges, view);
    const compactYs = new Set(Object.values(tight.nodes).map((n) => n.y));
    expect(compactYs.size).toBeGreaterThan(1);
    const arrangeW = Math.max(...Object.values(wide.nodes).map((n) => n.x + n.w));
    const compactW = Math.max(...Object.values(tight.nodes).map((n) => n.x + n.w));
    expect(compactW).toBeLessThan(arrangeW);
    expect(wide.nodes.IN!.x).toBeLessThan(wide.nodes.filter2!.x);
    expect(wide.nodes.filter2!.x).toBeLessThan(wide.nodes.OUT!.x);
    for (const d of Object.values(tight.edgePaths)) {
      expect(d.startsWith("M")).toBe(true);
      expect(/[QC]/.test(d)).toBe(false);
      const pts = parsePath(d);
      let len = 0;
      for (let i = 1; i < pts.length; i += 1) {
        len += Math.abs(pts[i]!.x - pts[i - 1]!.x) + Math.abs(pts[i]!.y - pts[i - 1]!.y);
      }
      expect(len, d).toBeGreaterThanOrEqual(BOARD_GRID);
    }
  });
});
