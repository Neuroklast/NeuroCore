import { describe, expect, it } from "vitest";
import type { AstDocument, AstNode } from "../bridge/ast";
import { canDeleteChip } from "./boardEdit";
import { graphOpEndpoints, graphToLayout, hydrateBoard, portGlobal, portId } from "./boardModel";
import { parseRoutePath } from "./boardPath";
import { chipSpec } from "./chipSpec";
import { compact } from "./layout/runLayout";

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
    const south = Object.values(g.ports).filter((p) => p.nodeId === "comp1" && p.south).map((p) => p.jackId);
    expect(south).toContain("threshold");
    expect(south).toContain("ceiling");
    expect(south).not.toContain("lookahead");
    expect(g.ports[portId("comp1", "ceiling", false)]).toBeTruthy();
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
    expect(Object.values(g.edges).some((e) => e.sourceNodeId === "MIDI")).toBe(false);
  });
});
