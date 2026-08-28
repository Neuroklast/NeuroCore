import { beforeEach, describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { useBoardStore } from "./boardStore";

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

describe("moveNode updates cables", () => {
  beforeEach(() => {
    useBoardStore.getState().hydrate(chain());
    const edges = { ...useBoardStore.getState().edges };
    for (const [id, e] of Object.entries(edges)) {
      edges[id] = { ...e, route: [{ x: 0, y: 0 }, { x: 80, y: 0 }] };
    }
    useBoardStore.getState().setEdges(edges);
  });

  it("clears incident routes so the canvas follows the chip, not the old PCB", () => {
    const before = Object.values(useBoardStore.getState().edges);
    expect(before.some((e) => e.route.length >= 2)).toBe(true);
    useBoardStore.getState().moveNode("stage1", 320, 160);
    const st = useBoardStore.getState().nodes.stage1!;
    expect(st.x).toBe(320);
    expect(st.y).toBe(160);
    const after = Object.values(useBoardStore.getState().edges).filter(
      (e) => e.sourceNodeId === "stage1" || e.targetNodeId === "stage1",
    );
    expect(after.length).toBeGreaterThan(0);
    expect(after.every((e) => e.route.length === 0)).toBe(true);
  });
});

describe("preset load layout generation", () => {
  beforeEach(() => {
    useBoardStore.setState({
      nodes: {},
      ports: {},
      edges: {},
      userMoved: false,
      layoutEpoch: 0,
    });
  });

  it("drops a stale layout from the previous graph so cables do not grab empty space", () => {
    useBoardStore.getState().hydrate(chain());
    const epochA = useBoardStore.getState().layoutEpoch;
    expect(epochA).toBeGreaterThan(0);
    useBoardStore.getState().applyLayout(
      { stage1: { x: 640, y: 96, w: 160, h: 96 } },
      {},
      epochA,
    );
    expect(useBoardStore.getState().nodes.stage1?.x).toBe(640);

    useBoardStore.getState().hydrate({
      ...chain(),
      nodes: [{
        ...chain().nodes[0]!,
        id: "filter1",
        type: "filter",
      }],
      edges: [{ from: "IN", to: "filter1", kind: "audio", fromJack: "out", toJack: "in" }],
    });
    const epochB = useBoardStore.getState().layoutEpoch;
    expect(epochB).toBeGreaterThan(epochA);
    expect(useBoardStore.getState().nodes.stage1).toBeUndefined();

    useBoardStore.getState().applyLayout(
      { stage1: { x: 640, y: 96, w: 160, h: 96 }, IN: { x: 8, y: 8, w: 96, h: 96 } },
      { "e-IN-stage1-out-in-0": [{ x: 0, y: 0 }, { x: 80, y: 0 }] },
      epochA,
    );
    expect(useBoardStore.getState().nodes.filter1).toBeDefined();
    expect(useBoardStore.getState().nodes.IN?.x).not.toBe(8);
    const routes = Object.values(useBoardStore.getState().edges).map((e) => e.route);
    expect(routes.every((r) => r.length === 0)).toBe(true);
  });
});
