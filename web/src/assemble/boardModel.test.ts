import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { assignRanks, graphToLayout, hydrateBoard, portGlobal, portId, portLocal, previewBoardIds } from "./boardModel";
import { BOARD_HALF } from "./grid";

function emptyAst(): AstDocument {
  return {
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [],
    edges: [],
    inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
  };
}

describe("headless board model", () => {
  it("gives IN one east audio port and OUT one west port, never the reverse", () => {
    const g = hydrateBoard(emptyAst());
    const inPorts = Object.values(g.ports).filter((p) => p.nodeId === "IN");
    expect(inPorts.every((p) => p.face === "e")).toBe(true);
    expect(inPorts.map((p) => p.jackId)).toEqual(["out"]);
    expect(Object.values(g.ports).filter((p) => p.nodeId === "IN" && p.face !== "e")).toHaveLength(0);
    const outPorts = Object.values(g.ports).filter((p) => p.nodeId === "OUT" && p.face === "w");
    expect(outPorts).toHaveLength(1);
    expect(outPorts[0]?.jackId).toBe("in");
  });

  it("cable ends are node origin plus local centre, inside the AABB", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "tanh(x * a)" },
        trailingComment: "",
        x: 240,
        y: 32,
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      }],
      edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const inn = g.nodes.IN!;
    const st = g.nodes.stage1!;
    const src = g.ports[portId("IN", "out", true)]!;
    const dst = g.ports[portId("stage1", "in", false)]!;
    const a = portGlobal(inn, src);
    const b = portGlobal(st, dst);
    expect(a.x).toBe(inn.x + inn.w);
    expect(b.x).toBe(st.x);
    expect(a.y).toBeGreaterThanOrEqual(inn.y + BOARD_HALF);
    expect(a.y).toBeLessThanOrEqual(inn.y + inn.h - BOARD_HALF);
    expect(b.y).toBeGreaterThanOrEqual(st.y + BOARD_HALF);
    expect(b.y).toBeLessThanOrEqual(st.y + st.h - BOARD_HALF);
    const ranks = assignRanks(g);
    expect(ranks.IN).toBe(0);
    expect(ranks.stage1).toBeGreaterThanOrEqual(1);
  });

  it("ignores native AST xy so a preset does not sit on the previous board", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "x" },
        trailingComment: "",
        x: 1800,
        y: 900,
        jacks: [
          { id: "in", label: "in", output: false, kind: "audio" },
          { id: "out", label: "out", output: true, kind: "audio" },
        ],
      }],
      edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    expect(g.nodes.stage1!.x).toBeLessThan(800);
    expect(g.nodes.stage1!.y).toBeLessThan(400);
    expect(Object.values(g.edges).every((e) => e.route.length === 0)).toBe(true);
  });

  it("previews board ids without building ports so hydrate runs once", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "stage1",
        type: "stage",
        busName: "main",
        args: { y: "x" },
        trailingComment: "",
        jacks: [],
      }],
      edges: [],
    };
    expect(previewBoardIds(ast).sort()).toEqual(["IN", "MIDI", "OUT", "SC", "stage1"].sort());
    expect(previewBoardIds(null)).toEqual([]);
  });

  it("gives an MS split two east ports and no combined out", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "ms1",
        type: "ms",
        busName: "main",
        args: { mode: "encode" },
        trailingComment: "",
        x: 240,
        y: 32,
        jacks: [],
      }],
      edges: [{ from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const east = Object.values(g.ports).filter((p) => p.nodeId === "ms1" && p.face === "e");
    const west = Object.values(g.ports).filter((p) => p.nodeId === "ms1" && p.face === "w");
    expect(east.map((p) => p.jackId).sort()).toEqual(["mid", "side"]);
    expect(west.map((p) => p.jackId)).toEqual(["in"]);
    expect(east.some((p) => p.jackId === "out")).toBe(false);
  });

  it("counts west, east, and south occupancy on separate faces", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "octaver1",
        type: "octaver",
        busName: "main",
        args: { sub: "1", up: "0", mix: "0.3", tone: "120", thresh: "0.05" },
        trailingComment: "",
        x: 240,
        y: 32,
        jacks: [],
      }],
      edges: [{ from: "IN", to: "octaver1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const node = g.nodes.octaver1!;
    const west = Object.values(g.ports).filter((p) => p.nodeId === "octaver1" && p.face === "w");
    const east = Object.values(g.ports).filter((p) => p.nodeId === "octaver1" && p.face === "e");
    const south = Object.values(g.ports).filter((p) => p.nodeId === "octaver1" && p.face === "s");
    expect(west.map((p) => `${p.jackId}#${p.index}/${p.count}`)).toEqual(["in#0/1"]);
    expect(east.map((p) => `${p.jackId}#${p.index}/${p.count}`)).toEqual(["out#0/1"]);
    expect(south.map((p) => `${p.jackId}#${p.index}/${p.count}`)).toEqual([
      "sub#0/5",
      "up#1/5",
      "mix#2/5",
      "tone#3/5",
      "thresh#4/5",
    ]);
    expect(portLocal(node, west[0]!).y).toBe(portLocal(node, east[0]!).y);
    const layout = graphToLayout(g).nodes.find((n) => n.id === "octaver1")!;
    const layoutWest = layout.ins.filter((p) => p.face === "w");
    expect(layoutWest.map((p) => p.id)).toEqual([portId("octaver1", "in", false)]);
    expect(layoutWest[0]!.y).toBe(layout.outs[0]!.y);
    expect(layout.ins.some((p) => p.face === "s")).toBe(true);
  });

  it("keeps OTT audio in on the west face when a param reuses that jack id", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [{
        id: "ott1",
        type: "ott",
        busName: "main",
        args: { depth: "1", time: "0.3", in: "1", low: "1", mid: "1", high: "1" },
        trailingComment: "",
        x: 240,
        y: 32,
        jacks: [],
      }],
      edges: [{ from: "IN", to: "ott1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const west = Object.values(g.ports).filter((p) => p.nodeId === "ott1" && p.face === "w");
    const south = Object.values(g.ports).filter((p) => p.nodeId === "ott1" && p.face === "s");
    expect(west.map((p) => p.jackId)).toEqual(["in"]);
    expect(south.map((p) => p.jackId)).not.toContain("in");
    const layout = graphToLayout(g).nodes.find((n) => n.id === "ott1")!;
    expect(layout.ins.filter((p) => p.face === "w").map((p) => p.id)).toEqual([
      portId("ott1", "in", false),
    ]);
    expect(layout.ins.some((p) => p.face === "s")).toBe(true);
  });
});

it("locks the input terminal", () => {
  expect(hydrateBoard(emptyAst()).nodes.IN?.locked).toBe(true);
});

it("gives every fan-out and fan-in connection a distinct physical socket", async () => {
  const { findFactory } = await import("../presets/factoryCatalog");
  const { parseDslSketch } = await import("../presets/parseDslSketch");
  const { doc } = parseDslSketch(findFactory("Transparent Blend")!.script);
  const graph = hydrateBoard(doc);
  const edges = Object.values(graph.edges);
  expect(new Set(edges.map(e => e.sourcePortId)).size).toBe(edges.length);
  expect(new Set(edges.map(e => e.targetPortId)).size).toBe(edges.length);
  for (const node of Object.values(graph.nodes)) {
    for (const east of [false, true]) {
      const ps = Object.values(graph.ports).filter(
        (p) => p.nodeId === node.id && p.face === (east ? "e" : "w"),
      );
      const ys = ps.map(p => portGlobal(node, p).y - node.y);
      expect(new Set(ys).size).toBe(ys.length);
      expect(ys.every(y => y >= 26 && y < node.h - 12)).toBe(true);
    }
  }
});
