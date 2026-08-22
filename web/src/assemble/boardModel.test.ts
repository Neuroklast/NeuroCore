import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { assignRanks, hydrateBoard, portGlobal, portId } from "./boardModel";
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
  it("gives IN two east ports and OUT one west port, never the reverse", () => {
    const g = hydrateBoard(emptyAst());
    const inPorts = Object.values(g.ports).filter((p) => p.nodeId === "IN");
    expect(inPorts.every((p) => p.east)).toBe(true);
    expect(inPorts.map((p) => p.jackId).sort()).toEqual(["out", "sc"]);
    expect(Object.values(g.ports).filter((p) => p.nodeId === "IN" && ! p.east)).toHaveLength(0);
    const outPorts = Object.values(g.ports).filter((p) => p.nodeId === "OUT");
    expect(outPorts).toHaveLength(1);
    expect(outPorts[0]?.east).toBe(false);
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
});
