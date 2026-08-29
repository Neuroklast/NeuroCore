import type { Edge, Node } from "../flowTypes";
import type { ChipData } from "../flowFromAst";
import { flowToLayout } from "./fromFlow";
import { runLayout } from "./runLayout";
import type { LayoutEdge, LayoutMode, LayoutNode, LayoutResult, LayoutView } from "./types";
import LayoutWorker from "./layout.worker.ts?worker";

type PendingJob = {
  resolve: (r: LayoutResult) => void;
  reject: (e: Error) => void;
  timer: ReturnType<typeof setTimeout>;
  mode: LayoutMode;
  nodes: LayoutNode[];
  edges: LayoutEdge[];
  view: LayoutView;
};

let worker: Worker | null = null;
let nextId = 1;
const pending = new Map<number, PendingJob>();
let workerFactory: (() => Worker | null) | null = null;
let workerTimeoutMs = 2500;

function fallbackJob(job: PendingJob): void {
  clearTimeout(job.timer);
  void runLayout(job.mode, job.nodes, job.edges, job.view).then(job.resolve, job.reject);
}

function clearPending(fallbackLeft: boolean): void {
  const leftover = [...pending.values()];
  pending.clear();
  for (const job of leftover) {
    if (fallbackLeft) {
      fallbackJob(job);
    } else {
      clearTimeout(job.timer);
    }
  }
}

/** Tests inject a fake Worker so ELK never runs on the caller. Pass null to restore. */
export function setLayoutWorkerFactory(factory: (() => Worker | null) | null): void {
  clearPending(false);
  workerFactory = factory;
  worker = null;
}

export function setLayoutWorkerTimeoutMs(ms: number | null): void {
  workerTimeoutMs = ms == null ? 2500 : ms;
}

function failWorker(): void {
  try {
    worker?.terminate();
  } catch {
    /* already gone */
  }
  worker = null;
  clearPending(true);
}

function bindWorker(w: Worker): Worker {
  w.onmessage = (ev: MessageEvent<LayoutResult & { reqId: number; ok?: boolean; error?: string }>) => {
    const job = pending.get(ev.data.reqId);
    if (! job) {
      return;
    }
    pending.delete(ev.data.reqId);
    clearTimeout(job.timer);
    if (ev.data.ok === false) {
      job.reject(new Error(ev.data.error || "layout failed"));
      return;
    }
    job.resolve({ nodes: ev.data.nodes, edgePaths: ev.data.edgePaths });
  };
  w.onerror = () => failWorker();
  w.onmessageerror = () => failWorker();
  return w;
}

function getWorker(): Worker | null {
  if (worker) {
    return worker;
  }
  if (workerFactory) {
    const w = workerFactory();
    if (! w) {
      return null;
    }
    worker = bindWorker(w);
    return worker;
  }
  if (typeof Worker === "undefined") {
    return null;
  }
  try {
    worker = bindWorker(new LayoutWorker());
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

function postGraphLayout(
  mode: LayoutMode,
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: LayoutView,
): Promise<LayoutResult> {
  const w = getWorker();
  if (w) {
    const reqId = nextId;
    nextId += 1;
    return new Promise<LayoutResult>((resolve, reject) => {
      const job: PendingJob = {
        resolve,
        reject,
        timer: setTimeout(() => {
          pending.delete(reqId);
          failWorker();
          fallbackJob(job);
        }, workerTimeoutMs),
        mode,
        nodes,
        edges,
        view,
      };
      pending.set(reqId, job);
      try {
        w.postMessage({ type: mode, nodes, edges, view, reqId });
      } catch {
        pending.delete(reqId);
        failWorker();
        fallbackJob(job);
      }
    });
  }
  return runLayout(mode, nodes, edges, view);
}

export async function requestLayout(
  mode: LayoutMode,
  nodes: Node<ChipData>[],
  edges: Edge[],
  view: LayoutView = { w: 960, h: 420 },
): Promise<LayoutResult> {
  const payload = flowToLayout(nodes, edges);
  return postGraphLayout(mode, payload.nodes, payload.edges, view);
}

export async function requestGraphLayout(
  mode: LayoutMode,
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: LayoutView = { w: 960, h: 420 },
): Promise<LayoutResult> {
  return postGraphLayout(mode, nodes, edges, view);
}
