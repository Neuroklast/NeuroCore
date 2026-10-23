import { describe, expect, it } from "vitest";
import type { AstDocument, AstNode } from "../bridge/ast";
import { canDeleteChip } from "./boardEdit";
import { graphOpEndpoints, hydrateBoard, portId } from "./boardModel";
import { chipSpec } from "./chipSpec";

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
    expect(seq.nodes.stepseq1!.w).toBeGreaterThanOrEqual(1000);
    expect(comp.nodes.comp1!.w).toBeGreaterThanOrEqual(700);
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
