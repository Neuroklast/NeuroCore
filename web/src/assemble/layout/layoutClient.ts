import type { Edge, Node } from "@xyflow/react";
import type { ChipData } from "../flowFromAst";
import { flowToLayout } from "./fromFlow";
import { runLayout } from "./runLayout";
import type { LayoutMode, LayoutResult, LayoutView } from "./types";

let worker: Worker | null = null;
let nextId = 1;
const pending = new Map<number, { resolve: (r: LayoutResult) => void; reject: (e: Error) => void }>();

function getWorker(): Worker | null {
  if (worker) {
    return worker;
  }
  if (typeof Worker === "undefined") {
    return null;
  }
  try {
    worker = new Worker(new URL("./layout.worker.ts", import.meta.url), { type: "module" });
    worker.onmessage = (ev: MessageEvent<LayoutResult & { reqId: number; ok?: boolean; error?: string }>) => {
      const job = pending.get(ev.data.reqId);
      if (! job) {
        return;
      }
      pending.delete(ev.data.reqId);
      if (ev.data.ok === false) {
        job.reject(new Error(ev.data.error || "layout failed"));
        return;
      }
      job.resolve({ nodes: ev.data.nodes, edgePaths: ev.data.edgePaths });
    };
    return worker;
  } catch {
    return null;
  }
}

export function applyLayoutResult<
  N extends { id: string; position: { x: number; y: number }; width?: number; height?: number },
  E extends { id: string; data?: unknown },
>(
  result: LayoutResult,
  nodes: N[],
  edges: E[],
  moveNodes: boolean,
): { nodes: N[]; edges: E[] } {
  const nextNodes = moveNodes
    ? nodes.map((n) => {
      const p = result.nodes[n.id];
      return p ? { ...n, position: { x: p.x, y: p.y }, width: p.w, height: p.h } : n;
    })
    : nodes;
  const nextEdges = edges.map((e) => {
    const d = result.edgePaths[e.id];
    if (! d) {
      return e;
    }
    return { ...e, data: { ...(e.data as object), route: d } };
  });
  return { nodes: nextNodes, edges: nextEdges };
}

export async function requestLayout(
  mode: LayoutMode,
  nodes: Node<ChipData>[],
  edges: Edge[],
  view: LayoutView = { w: 960, h: 420 },
): Promise<LayoutResult> {
  const payload = flowToLayout(nodes, edges);
  const local = runLayout(mode, payload.nodes, payload.edges, view);
  const w = getWorker();
  if (w) {
    const reqId = nextId;
    nextId += 1;
    w.postMessage({ type: mode, nodes: payload.nodes, edges: payload.edges, view, reqId });
  }
  return local;
}
