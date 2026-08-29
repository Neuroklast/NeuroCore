import { afterEach, beforeEach, describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { layoutBoard, rerouteBoard } from "./boardCommit";
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
      commandedLayout: null,
      layoutBusy: false,
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

  it("forced Compact applies after a chip drag and clears userMoved", async () => {
    const fake = {
      postMessage(msg: { reqId: number; type: string }) {
        queueMicrotask(() => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: {
                IN: { x: 32, y: 32, w: 96, h: 96 },
                stage1: { x: 32, y: 192, w: 160, h: 96 },
              },
              edgePaths: { e0: "M128 80L160 80" },
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    useBoardStore.getState().hydrate(chain());
    useBoardStore.getState().moveNode("stage1", 640, 32);
    expect(useBoardStore.getState().userMoved).toBe(true);
    const dragged = useBoardStore.getState().nodes.stage1!.x;
    await layoutBoard("COMPACT", { w: 400, h: 420 }, { force: true });
    expect(useBoardStore.getState().userMoved).toBe(false);
    expect(useBoardStore.getState().nodes.stage1!.x).not.toBe(dragged);
    expect(useBoardStore.getState().nodes.stage1!.y).toBeGreaterThan(useBoardStore.getState().nodes.IN!.y);
  });

  it("auto layout does not Arrange after a user Compact", async () => {
    const fake = {
      postMessage(msg: { reqId: number; type: string }) {
        queueMicrotask(() => {
          const compact = msg.type === "COMPACT";
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: {
                IN: { x: 32, y: 32, w: 96, h: 96 },
                stage1: { x: compact ? 32 : 240, y: compact ? 192 : 32, w: 160, h: 96 },
              },
              edgePaths: { e0: "M128 80L160 80" },
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    useBoardStore.getState().hydrate(chain());
    await layoutBoard("COMPACT", { w: 400, h: 420 }, { force: true });
    const compactY = useBoardStore.getState().nodes.stage1!.y;
    expect(compactY).toBeGreaterThan(useBoardStore.getState().nodes.IN!.y);
    await layoutBoard("ARRANGE", { w: 960, h: 420 });
    expect(useBoardStore.getState().nodes.stage1!.y).toBe(compactY);
    expect(useBoardStore.getState().commandedLayout).toBe("COMPACT");
  });

  it("keeps layoutBusy until Compact/Arrange finishes so Circuit can show a progress bar", async () => {
    const fake = {
      postMessage(msg: { reqId: number }) {
        queueMicrotask(() => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: {
                IN: { x: 32, y: 32, w: 96, h: 96 },
                stage1: { x: 240, y: 32, w: 160, h: 96 },
              },
              edgePaths: { e0: "M128 80L160 80" },
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    useBoardStore.getState().hydrate(chain());
    expect(useBoardStore.getState().layoutBusy).toBe(true);
    await layoutBoard("ARRANGE", { w: 960, h: 420 }, { force: true });
    expect(useBoardStore.getState().layoutBusy).toBe(false);
  });
});

describe("host echo is not a user drag", () => {
  it("keeps xy on hydrate without setting userMoved", () => {
    useBoardStore.getState().hydrate(chain());
    useBoardStore.setState({ userMoved: false });
    const x = useBoardStore.getState().nodes.stage1!.x;
    useBoardStore.getState().hydrate(chain(), false, true);
    expect(useBoardStore.getState().nodes.stage1!.x).toBe(x);
    expect(useBoardStore.getState().userMoved).toBe(false);
  });
});
