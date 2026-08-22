import { describe, expect, it, afterEach } from "vitest";
import { requestLayout, setLayoutWorkerFactory, setLayoutWorkerTimeoutMs } from "./layoutClient";
import type { Node, Edge } from "@xyflow/react";
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

  it("falls back to local layout if the worker never answers", async () => {
    setLayoutWorkerTimeoutMs(20);
    const fake = { postMessage() {}, terminate() {} };
    setLayoutWorkerFactory(() => fake as unknown as Worker);
    const nodes = [{ id: "IN", position: { x: 0, y: 0 }, data: { type: "in", jacks: [], args: {} } }] as unknown as Node<ChipData>[];
    const laid = await requestLayout("REROUTE", nodes, [], { w: 400, h: 200 });
    expect(laid.nodes).toBeTruthy();
  });
});
