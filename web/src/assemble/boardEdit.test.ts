import { describe, expect, it } from "vitest";
import { circuitHasSelection } from "../chrome/shortcuts";
import {
  bindHoverOpensChip,
  boardContextHit,
  canDeleteChip,
  chipAtWorld,
  chipExpandAction,
  circuitAllowsTextSelect,
  hitBoardEdge,
} from "./boardEdit";
import type { BoardGraph } from "./boardModel";
import { chipMenuActions } from "./chipMenu";

function graph(): BoardGraph {
  return {
    nodes: {
      IN: { id: "IN", type: "in", role: "io", x: 0, y: 32, w: 96, h: 96, args: {}, label: "IN", channel: "", locked: true },
      filter1: { id: "filter1", type: "filter", role: "chip", x: 160, y: 32, w: 256, h: 96, args: {}, label: "FILTER", channel: "", locked: false },
      OUT: { id: "OUT", type: "out", role: "io", x: 480, y: 32, w: 96, h: 96, args: {}, label: "OUT", channel: "", locked: false },
    },
    ports: {
      "IN::out::src": { id: "IN::out::src", nodeId: "IN", jackId: "out", east: true, index: 0, count: 1, kind: "audio" },
      "filter1::in::dst": { id: "filter1::in::dst", nodeId: "filter1", jackId: "in", east: false, index: 0, count: 1, kind: "audio" },
      "filter1::out::src": { id: "filter1::out::src", nodeId: "filter1", jackId: "out", east: true, index: 0, count: 1, kind: "audio" },
      "OUT::in::dst": { id: "OUT::in::dst", nodeId: "OUT", jackId: "in", east: false, index: 0, count: 1, kind: "audio" },
    },
    edges: {
      e0: {
        id: "e0",
        sourceNodeId: "IN",
        sourcePortId: "IN::out::src",
        targetNodeId: "filter1",
        targetPortId: "filter1::in::dst",
        kind: "audio",
        route: [{ x: 96, y: 80 }, { x: 160, y: 80 }],
      },
    },
  };
}

describe("board edit hits", () => {
  it("never deletes IN or OUT; DSP chips are removable", () => {
    const g = graph();
    expect(canDeleteChip(g.nodes.IN)).toBe(false);
    expect(canDeleteChip(g.nodes.OUT)).toBe(false);
    expect(canDeleteChip(g.nodes.filter1)).toBe(true);
    expect(chipMenuActions()).toEqual(["insertAfter", "inspect", "delete"]);
  });

  it("chip wins, then a cable, else the empty pane", () => {
    const g = graph();
    expect(boardContextHit("filter1", { x: 200, y: 80 }, g)).toEqual({ kind: "chip", id: "filter1" });
    expect(boardContextHit(null, { x: 128, y: 80 }, g)).toEqual({
      kind: "edge",
      id: "e0",
      sourceId: "IN",
      targetId: "filter1",
    });
    expect(boardContextHit(null, { x: 40, y: 400 }, g)).toEqual({ kind: "pane" });
    expect(hitBoardEdge({ x: 128, y: 80 }, g)?.id).toBe("e0");
    expect(hitBoardEdge({ x: 40, y: 400 }, g)).toBeNull();
  });

  it("opens bind sockets only while a knob is dragged over a DSP chip", () => {
    expect(bindHoverOpensChip(true, true, "chip")).toBe(true);
    expect(bindHoverOpensChip(true, true, "io")).toBe(false);
    expect(bindHoverOpensChip(true, false, "chip")).toBe(false);
    expect(bindHoverOpensChip(false, true, "chip")).toBe(false);
    expect(chipExpandAction("chip", "filter")).toBe("inspect");
    expect(chipExpandAction("io", "out")).toBe("inspect");
    expect(chipExpandAction("io", "in")).toBe("none");
  });

  it("hit-tests a chip AABB only — bind pads stay inside the box", () => {
    const g = graph();
    const f = g.nodes.filter1!;
    expect(chipAtWorld(Object.values(g.nodes), { x: f.x + 8, y: f.y + 8 })).toBe("filter1");
    expect(chipAtWorld(Object.values(g.nodes), { x: f.x + 8, y: f.y + f.h + 20 })).toBeNull();
  });

  it("does not let Circuit titles be selected, only fields", () => {
    expect(circuitAllowsTextSelect("pane")).toBe(false);
    expect(circuitAllowsTextSelect("chip")).toBe(false);
    expect(circuitAllowsTextSelect("field")).toBe(true);
  });

  it("treats data-selected on the headless board as a Circuit selection", () => {
    const chip = { querySelector: () => null };
    const paneOn = {
      querySelector: (sel: string) => (sel.includes("data-selected") ? chip : null),
    };
    const rootOn = {
      querySelector: (sel: string) => (sel.includes("nk-circuit") ? paneOn : null),
    };
    expect(circuitHasSelection(rootOn as unknown as ParentNode)).toBe(true);
    const paneOff = { querySelector: () => null };
    const rootOff = {
      querySelector: (sel: string) => (sel.includes("nk-circuit") ? paneOff : null),
    };
    expect(circuitHasSelection(rootOff as unknown as ParentNode)).toBe(false);
  });
});
