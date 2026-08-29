import { describe, expect, it, afterEach } from "vitest";
import { requestGraphLayout, requestLayout, setLayoutWorkerFactory, setLayoutWorkerTimeoutMs } from "./layoutClient";
import type { Node, Edge } from "../flowTypes";
import type { ChipData } from "../flowFromAst";

afterEach(() => {
  setLayoutWorkerFactory(null);
  setLayoutWorkerTimeoutMs(null);
});

describe("requestLayout stays off the UI thread when a worker exists", () => {
  it("resolves the worker payload and never runs local ELK first", async () => {
    let posted = 0;
    const fake = {
      postMessage(msg: { reqId: number }) {
        posted += 1;
        queueMicrotask(() => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: { IN: { x: 32, y: 64, w: 96, h: 96 } },
              edgePaths: {},
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", position: { x: 0, y: 0 }, data: { type: "in", jacks: [], args: {} } }] as unknown as Node<ChipData>[];
    const edges: Edge[] = [];
    const laid = await requestLayout("ARRANGE", nodes, edges, { w: 400, h: 200 });
    expect(posted).toBe(1);
    expect(laid.nodes.IN).toEqual({ x: 32, y: 64, w: 96, h: 96 });
  });

  it("posts a headless board graph without converting through React Flow nodes", async () => {
    let posted: unknown = null;
    const fake = {
      postMessage(msg: { reqId: number; type: string; nodes: Array<{ id: string }> }) {
        posted = msg;
        queueMicrotask(() => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: { stage1: { x: 128, y: 64, w: 160, h: 96 } },
              edgePaths: { e0: "M0 0L32 0" },
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const laid = await requestGraphLayout(
      "REROUTE",
      [{ id: "stage1", w: 160, h: 96, x: 96, y: 64, ins: [], outs: [] }],
      [{ id: "e0", source: "IN", target: "stage1", fromJack: "out", toJack: "in" }],
      { w: 400, h: 200 },
    );
    expect((posted as { type: string }).type).toBe("REROUTE");
    expect((posted as { nodes: Array<{ id: string }> }).nodes[0]?.id).toBe("stage1");
    expect(laid.edgePaths.e0).toBe("M0 0L32 0");
  });

  it("falls back to local layout if the worker never answers", async () => {
    setLayoutWorkerTimeoutMs(20);
    const fake = { postMessage() {}, terminate() {} };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", position: { x: 0, y: 0 }, data: { type: "in", jacks: [], args: {} } }] as unknown as Node<ChipData>[];
    const laid = await requestLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    expect(laid.nodes).toBeTruthy();
  });

  it("falls back to runLayout when the worker errors instead of rejecting", async () => {
    const fake: { postMessage: () => void; terminate: () => void; onerror?: (ev: Event) => void } = {
      postMessage() {},
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    queueMicrotask(() => {
      fake.onerror?.(new Event("error"));
    });
    const laid = await requestGraphLayout("ARRANGE", [
      { id: "IN", x: 0, y: 0, w: 96, h: 96, ins: [], outs: [{ id: "out", y: 48 }] },
      { id: "OUT", x: 400, y: 0, w: 96, h: 96, ins: [{ id: "in", y: 48 }], outs: [] },
    ], [{ id: "e0", source: "IN", target: "OUT", fromJack: "out", toJack: "in" }], { w: 960, h: 420 });
    expect(laid.nodes.IN).toBeDefined();
    expect(laid.nodes.OUT).toBeDefined();
  });

  it("falls back in-flight jobs when one request times out instead of rejecting them", async () => {
    setLayoutWorkerTimeoutMs(20);
    const fake = { postMessage() {}, terminate() {} };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", ins: [], outs: [] as Array<{ id: string; y: number }>, w: 96, h: 96 }];
    const a = requestGraphLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    const b = requestGraphLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    const laidB = await b;
    const laidA = await a;
    expect(laidA.nodes).toBeTruthy();
    expect(laidB.nodes).toBeTruthy();
  });

  it("posts again after a timeout instead of leaving the worker dead", async () => {
    setLayoutWorkerTimeoutMs(20);
    let posted = 0;
    const fake = {
      postMessage(msg: { reqId: number }) {
        posted += 1;
        if (posted === 1) {
          return;
        }
        queueMicrotask(() => {
          (fake as { onmessage?: (ev: { data: unknown }) => void }).onmessage?.({
            data: {
              reqId: msg.reqId,
              ok: true,
              nodes: { IN: { x: 8, y: 16, w: 96, h: 96 } },
              edgePaths: {},
            },
          });
        });
      },
      terminate() {},
    };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", position: { x: 0, y: 0 }, data: { type: "in", jacks: [], args: {} } }] as unknown as Node<ChipData>[];
    await requestLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    const laid = await requestLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    expect(posted).toBe(2);
    expect(laid.nodes.IN).toEqual({ x: 8, y: 16, w: 96, h: 96 });
  });
});
