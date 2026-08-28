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

  it("rejects other in-flight jobs when one request times out", async () => {
    setLayoutWorkerTimeoutMs(20);
    const fake = { postMessage() {}, terminate() {} };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", ins: [], outs: [] as Array<{ id: string; y: number }>, w: 96, h: 96 }];
    const a = requestGraphLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    const b = requestGraphLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    await expect(b).rejects.toThrow(/layout worker/);
    const laid = await a;
    expect(laid.nodes).toBeTruthy();
  });
});
