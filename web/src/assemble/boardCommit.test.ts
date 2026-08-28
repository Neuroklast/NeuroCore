import { afterEach, beforeEach, describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { rerouteBoard } from "./boardCommit";
import { useBoardStore } from "./boardStore";
import { setLayoutWorkerFactory, setLayoutWorkerTimeoutMs } from "./layout/layoutClient";

function chain(id = "stage1", type = "stage"): AstDocument {
  return {
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [{
      id,
      type,
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
    edges: [{ from: "IN", to: id, kind: "audio", fromJack: "out", toJack: "in" }],
    inJacks: [{ id: "out", label: "out", output: true, kind: "audio" }],
  };
}

afterEach(() => {
  setLayoutWorkerFactory(null);
  setLayoutWorkerTimeoutMs(null);
});

describe("rerouteBoard layout generation", () => {
  beforeEach(() => {
    useBoardStore.setState({
      nodes: {},
      ports: {},
      edges: {},
      userMoved: false,
      layoutEpoch: 0,
    });
  });

  it("drops a reroute that started before a new hydrate", async () => {
    let reply: ((data: { nodes: Record<string, { x: number; y: number; w: number; h: number }>; edgePaths: Record<string, string> }) => void) | null = null;
    const fake = {
      postMessage(msg: { reqId: number }) {
        reply = (data) => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: { reqId: msg.reqId, ok: true, ...data },
          });
        };
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);

    useBoardStore.getState().hydrate(chain());
    expect(useBoardStore.getState().nodes.stage1).toBeDefined();
    rerouteBoard();

    useBoardStore.getState().hydrate({
      ...chain("filter1", "filter"),
      edges: [{ from: "IN", to: "filter1", kind: "audio", fromJack: "out", toJack: "in" }],
    });
    expect(useBoardStore.getState().nodes.filter1).toBeDefined();
    const filterX = useBoardStore.getState().nodes.filter1!.x;
    if (reply == null) {
      throw new Error("layout worker did not post");
    }
    reply({
      nodes: { stage1: { x: 640, y: 96, w: 160, h: 96 }, IN: { x: 640, y: 96, w: 96, h: 96 } },
      edgePaths: {},
    });
    await Promise.resolve();
    await Promise.resolve();

    expect(useBoardStore.getState().nodes.stage1).toBeUndefined();
    expect(useBoardStore.getState().nodes.filter1?.x).toBe(filterX);
    expect(useBoardStore.getState().nodes.IN?.x).not.toBe(640);
  });
});
