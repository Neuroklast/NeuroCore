import { describe, expect, it } from "vitest";
import type { AstDocument, AstNode } from "../bridge/ast";
import { canDeleteChip } from "./boardEdit";
import { midiPlateLit, sidechainPlateLive } from "./boardTerminalLive";
import { applyPlaced, graphOpEndpoints, graphToLayout, hydrateBoard, portGlobal, portId } from "./boardModel";
import { parseRoutePath } from "./boardPath";
import { chipSpec } from "./chipSpec";
import { compact, reroute } from "./layout/runLayout";

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

function node(id: string, type: string, args: Record<string, string> = {}, jacks: AstNode["jacks"] = []): AstNode {
  return { id, type, busName: "main", args, trailingComment: "", jacks };
}

describe("board architecture baseline", () => {
  it("records current StepSeq and Comp plate widths from full south captions", () => {
    const seq = hydrateBoard({
      ...emptyAst(),
      nodes: [node("stepseq1", "stepseq", chipSpec("stepseq").defaultArgs)],
    });
    const comp = hydrateBoard({
      ...emptyAst(),
      nodes: [node("comp1", "comp", chipSpec("comp").defaultArgs)],
    });
    expect(seq.nodes.stepseq1!.w).toBeLessThanOrEqual(448);
    expect(comp.nodes.comp1!.w).toBeLessThanOrEqual(448);
  });

  it("keeps a connected extra param jack after compact", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [
        node("osc1", "osc", { freq: "1" }),
        node("comp1", "comp", chipSpec("comp").defaultArgs),
      ],
      edges: [
        { from: "IN", to: "comp1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "osc1", to: "comp1", kind: "mod", fromJack: "mod", toJack: "ceiling" },
      ],
    };
    const g = hydrateBoard(ast);
    const south = Object.values(g.ports).filter((p) => p.nodeId === "comp1" && p.face === "s").map((p) => p.jackId);
    expect(south).toContain("threshold");
    expect(south).toContain("ceiling");
    expect(south).not.toContain("lookahead");
    expect(g.ports[portId("comp1", "ceiling", false)]).toBeTruthy();
  });

  it("routes a south param cable to the south jack, not the west audio in", async () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [
        node("osc1", "osc", { freq: "1" }),
        node("comp1", "comp", chipSpec("comp").defaultArgs),
      ],
      edges: [
        { from: "IN", to: "comp1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "osc1", to: "comp1", kind: "mod", fromJack: "mod", toJack: "ceiling" },
      ],
    };
    const g = hydrateBoard(ast);
    const layout = graphToLayout(g);
    const ceilingId = portId("comp1", "ceiling", false);
    expect(layout.nodes.find((n) => n.id === "comp1")!.ins.map((p) => p.id)).toContain(ceilingId);
    expect(layout.edges.some((e) => e.toJack === ceilingId)).toBe(true);
    const laid = await compact(layout.nodes, layout.edges, { w: 896, h: 520 });
    const edge = layout.edges.find((e) => e.toJack === ceilingId);
    expect(edge).toBeTruthy();
    const pts = parseRoutePath(laid.edgePaths[edge!.id] ?? "");
    expect(pts.length).toBeGreaterThan(1);
    const jack = g.ports[ceilingId]!;
    const box = laid.nodes.comp1!;
    const dest = portGlobal({ ...g.nodes.comp1!, x: box.x, y: box.y, w: box.w, h: box.h }, jack);
    const end = pts[pts.length - 1]!;
    expect(end.x, "south jack, not west audio in").toBeCloseTo(dest.x, 0);
    expect(end.y, "south jack, not west audio in").toBeCloseTo(dest.y, 0);
  });
});

describe("host terminals", () => {
  it("locks audio IN and OUT so they cannot move or delete", () => {
    const g = hydrateBoard(emptyAst());
    expect(g.nodes.IN?.locked).toBe(true);
    expect(g.nodes.OUT?.locked).toBe(true);
    expect(g.nodes.IN?.role).toBe("io");
    expect(g.nodes.OUT?.role).toBe("io");
    expect(canDeleteChip(g.nodes.IN)).toBe(false);
    expect(canDeleteChip(g.nodes.OUT)).toBe(false);
  });

  it("keeps IN::sc::src on a dedicated SC terminal without renaming the jack", () => {
    const g = hydrateBoard(emptyAst());
    const sc = g.ports[portId("IN", "sc", true)];
    expect(sc).toBeTruthy();
    expect(sc?.nodeId).toBe("SC");
    expect(sc?.jackId).toBe("sc");
    expect(sc?.kind).toBe("sc");
    expect(g.nodes.SC?.role).toBe("io");
    expect(g.nodes.SC?.type).toBe("sidechain");
    expect(g.nodes.SC?.locked).toBe(true);
    expect(canDeleteChip(g.nodes.SC)).toBe(false);
    expect(Object.values(g.ports).filter((p) => p.nodeId === "IN").map((p) => p.jackId)).toEqual(["out"]);
  });

  it("gives every port an explicit face, not east/south flags", () => {
    const g = hydrateBoard(emptyAst());
    const faces = new Set(Object.values(g.ports).map((p) => p.face));
    expect([...faces].every((f) => f === "n" || f === "e" || f === "s" || f === "w")).toBe(true);
    expect(g.ports[portId("IN", "out", true)]?.face).toBe("e");
    expect(g.ports[portId("OUT", "in", false)]?.face).toBe("w");
    expect(g.ports[portId("IN", "sc", true)]?.face).toBe("e");
    expect(Object.values(g.ports).some((p) => "east" in p || "south" in p)).toBe(false);
  });

  it("publishes SC cables as IN/sc, never as node SC", () => {
    expect(graphOpEndpoints({ id: "IN::sc::src", jackId: "sc" })).toEqual({ nodeId: "IN", jackId: "sc" });
    expect(graphOpEndpoints({ id: "IN::out::src", jackId: "out" })).toEqual({ nodeId: "IN", jackId: "out" });
    expect(graphOpEndpoints({ id: "OUT::in::dst", jackId: "in" })).toEqual({ nodeId: "OUT", jackId: "in" });
  });

  it("draws sidechain cables from the SC plate, not AUDIO IN", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [node("comp1", "comp", chipSpec("comp").defaultArgs, [
        { id: "in", label: "in", output: false, kind: "audio" },
        { id: "out", label: "out", output: true, kind: "audio" },
      ])],
      edges: [{ from: "IN", to: "comp1", kind: "audio", fromJack: "sc", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const sc = g.ports[portId("IN", "sc", true)]!;
    const edge = Object.values(g.edges).find((e) => e.sourcePortId === sc.id);
    expect(edge?.sourceNodeId).toBe("SC");
    expect(portGlobal(g.nodes.SC!, sc).x).toBe(g.nodes.SC!.x + g.nodes.SC!.w);
    expect(graphOpEndpoints(sc)).toEqual({ nodeId: "IN", jackId: "sc" });
  });

  it("routes a sidechain cable from the SC plate after compact", async () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [node("comp1", "comp", chipSpec("comp").defaultArgs, [
        { id: "in", label: "in", output: false, kind: "audio" },
        { id: "out", label: "out", output: true, kind: "audio" },
      ])],
      edges: [{ from: "IN", to: "comp1", kind: "audio", fromJack: "sc", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    const edge = Object.values(g.edges).find((e) => e.sourceNodeId === "SC");
    expect(edge).toBeTruthy();
    const laid = await compact(graphToLayout(g).nodes, graphToLayout(g).edges, { w: 896, h: 520 });
    expect(laid.nodes.SC, "SC stays a pinned layout node").toBeTruthy();
    const pts = parseRoutePath(laid.edgePaths[edge!.id] ?? "");
    expect(pts.length).toBeGreaterThan(1);
    expect(pts[0]!.x).toBe(laid.nodes.SC!.x + laid.nodes.SC!.w);
  });

  it("places SC below IN on the left, OUT to the right", () => {
    const g = hydrateBoard(emptyAst());
    expect(g.nodes.SC!.x).toBe(g.nodes.IN!.x);
    expect(g.nodes.SC!.y).toBeGreaterThan(g.nodes.IN!.y);
    expect(g.nodes.OUT!.x).toBeGreaterThan(g.nodes.IN!.x);
  });
});

describe("midi terminal contract", () => {
  it("adds a locked MIDI terminal with no DSP node id in connect payloads", () => {
    const ast: AstDocument = {
      ...emptyAst(),
      nodes: [node("stage1", "stage", { y: "midi_gate" }, [
        { id: "in", label: "in", output: false, kind: "audio" },
        { id: "out", label: "out", output: true, kind: "audio" },
      ])],
      edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
    };
    const g = hydrateBoard(ast);
    expect(g.nodes.MIDI?.role).toBe("io");
    expect(g.nodes.MIDI?.locked).toBe(true);
    expect(canDeleteChip(g.nodes.MIDI)).toBe(false);
    expect(graphOpEndpoints({ id: "MIDI::gate::src", jackId: "gate" }).nodeId).toBe("MIDI");
  });

  it("does not invent a MIDI graphOp source for unused patches", () => {
    const g = hydrateBoard(emptyAst());
    expect(g.ports[portId("MIDI", "gate", true)]?.nodeId).toBe("MIDI");
    expect(Object.values(g.edges).some((e) => e.sourceNodeId === "MIDI")).toBe(false);
  });

  it("lights MIDI from host midiGate/vel/mod, not from a graph edge", () => {
    expect(midiPlateLit(0, 0, 0)).toBe(false);
    expect(midiPlateLit(1, 0, 0)).toBe(true);
    expect(midiPlateLit(0, 0.8, 0)).toBe(true);
    expect(midiPlateLit(0, 0, 0.4)).toBe(true);
  });
});

describe("sidechain terminal live", () => {
  it("dims SC when the host pin is off", () => {
    expect(sidechainPlateLive(false)).toBe(false);
    expect(sidechainPlateLive(true)).toBe(true);
  });
});


it("repeated reroute keeps terminal placement and route endpoints stable", async () => {
 let graph = hydrateBoard({...emptyAst(), nodes:[node("comp1", "comp", chipSpec("comp").defaultArgs)], edges:[
  {from:"IN",to:"comp1",kind:"audio",fromJack:"out",toJack:"in"},
  {from:"IN",to:"comp1",kind:"audio",fromJack:"sc",toJack:"in"}
 ]});
 const firstLayout = graphToLayout(graph);
 const first = await reroute(firstLayout.nodes, firstLayout.edges);
 graph = applyPlaced(graph,first.nodes,{});
 const initial = {MIDI:graph.nodes.MIDI!.y, SC:graph.nodes.SC!.y};
 for (let i=0; i<3; ++i) {
  const layout = graphToLayout(graph);
  const result = await reroute(layout.nodes, layout.edges);
  expect(result.nodes.MIDI!.y).toBe(initial.MIDI);
  expect(result.nodes.SC!.y).toBe(initial.SC);
  const routes = Object.fromEntries(Object.entries(result.edgePaths).map(([id,path])=>[id,parseRoutePath(path)]));
  graph = applyPlaced(graph,result.nodes,routes);
  for (const edge of Object.values(graph.edges)) {
   const source = portGlobal(graph.nodes[edge.sourceNodeId]!,graph.ports[edge.sourcePortId]!);
   expect(edge.route[0]).toEqual(source);
  }
 }
});
