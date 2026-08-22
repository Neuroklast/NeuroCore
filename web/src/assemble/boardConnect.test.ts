import { describe, expect, it } from "vitest";
import { hydrateBoard, portGlobal, portId } from "./boardModel";
import { bezierPreview, canLink, edgesAfterConnect, edgesAfterCutPort, magnetPort } from "./boardConnect";
import type { AstDocument } from "../bridge/ast";

function chain(): AstDocument {
  return {
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [{
      id: "stage1",
      type: "stage",
      busName: "main",
      args: { y: "x" },
      trailingComment: "",
      x: 240,
      y: 32,
      jacks: [
        { id: "in", label: "in", output: false, kind: "audio" },
        { id: "out", label: "out", output: true, kind: "audio" },
      ],
    }],
    edges: [{ from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" }],
    inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
  };
}

describe("board connect", () => {
  it("only links an east source to a west target of the same family", () => {
    const g = hydrateBoard(chain());
    const inn = g.ports[portId("IN", "out", true)]!;
    const stIn = g.ports[portId("stage1", "in", false)]!;
    const stOut = g.ports[portId("stage1", "out", true)]!;
    expect(canLink(inn, stIn)).toBe(true);
    expect(canLink(stIn, inn)).toBe(false);
    expect(canLink(inn, stOut)).toBe(false);
    expect(canLink(stOut, stOut)).toBe(false);
  });

  it("snaps to the dest centre inside the magnet radius", () => {
    const g = hydrateBoard(chain());
    const inn = g.ports[portId("IN", "out", true)]!;
    const stIn = g.ports[portId("stage1", "in", false)]!;
    const dest = portGlobal(g.nodes.stage1!, stIn);
    expect(magnetPort({ x: dest.x + 10, y: dest.y - 8 }, inn, g)).toBe(stIn);
    expect(magnetPort({ x: dest.x + 400, y: dest.y }, inn, g)).toBeNull();
  });

  it("hot-swap replaces the edge on a busy dest port", () => {
    const g = hydrateBoard(chain());
    const inn = g.ports[portId("IN", "out", true)]!;
    const stIn = g.ports[portId("stage1", "in", false)]!;
    const before = Object.keys(g.edges).length;
    const next = edgesAfterConnect(g, inn, stIn);
    expect(Object.values(next).filter((e) => e.targetPortId === stIn.id)).toHaveLength(1);
    expect(Object.keys(next).length).toBeLessThanOrEqual(before);
  });

  it("leaves the bezier along the output normal", () => {
    const b = bezierPreview({ x: 0, y: 40 }, { x: 200, y: 80 }, true);
    expect(b.c1.x).toBeGreaterThan(0);
    expect(b.c1.y).toBe(40);
    expect(b.c2.x).toBeLessThan(200);
    const cut = edgesAfterCutPort(hydrateBoard(chain()), portId("IN", "out", true));
    expect(Object.values(cut).some((e) => e.sourceNodeId === "IN")).toBe(false);
  });
});
