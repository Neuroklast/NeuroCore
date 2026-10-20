import { fitCamera } from "./boardCamera";
import type { BoardCamera, BoardGraph, BoardNode } from "./boardModel";

export type BoardRegion = {
  key: string;
  label: string;
  ids: string[];
  children: BoardRegion[];
};
/** A view hierarchy only. Never writes positions, ports, edges or the script. */
export function navigationTree(graph: BoardGraph): BoardRegion {
  const ordered: string[] = [],
    seen = new Set<string>();
  const outgoing = new Map<string, string[]>();
  const incoming = new Set<string>();
  for (const e of Object.values(graph.edges)) {
    const list = outgoing.get(e.sourceNodeId) ?? [];
    list.push(e.targetNodeId);
    outgoing.set(e.sourceNodeId, list);
    incoming.add(e.targetNodeId);
  }
  // Iterative traversal also covers parked chips and feedback without recursion overflow.
  const roots = Object.keys(graph.nodes).filter((id) => !incoming.has(id));
  for (const root of [...roots, ...Object.keys(graph.nodes)]) {
    const stack = [root];
    while (stack.length) {
      const id = stack.pop()!;
      if (seen.has(id) || !graph.nodes[id]) continue;
      seen.add(id);
      ordered.push(id);
      stack.push(...(outgoing.get(id) ?? []).slice().reverse());
    }
  }
  function region(ids: string[], key: string): BoardRegion {
    const buses = [
      ...new Set(ids.map((id) => graph.nodes[id]?.busName).filter(Boolean)),
    ];
    const label =
      key === "board"
        ? "Board"
        : buses.length === 1
          ? String(buses[0])
          : ids.length === 1
            ? ids[0]!
            : `${ids[0]} → ${ids[ids.length - 1]}`;
    const children: BoardRegion[] = [];
    if (ids.length > 3) {
      const span = Math.ceil(
        ids.length / Math.min(6, Math.ceil(ids.length / 3)),
      );
      for (let i = 0; i < ids.length; i += span)
        children.push(region(ids.slice(i, i + span), `${key}.${i / span}`));
    }
    return { key, label, ids, children };
  }
  return region(ordered, "board");
}
export function findRegion(
  root: BoardRegion,
  key: string,
): BoardRegion | undefined {
  if (root.key === key) return root;
  for (const child of root.children) {
    const found = findRegion(child, key);
    if (found) return found;
  }
}
export function regionForChip(root: BoardRegion, id: string): BoardRegion {
  const child = root.children.find((c) => c.ids.includes(id));
  return child ? regionForChip(child, id) : root;
}
export function scopeGraph(graph: BoardGraph, ids: string[]): BoardGraph {
  const set = new Set(ids);
  return {
    nodes: Object.fromEntries(
      ids.filter((id) => graph.nodes[id]).map((id) => [id, graph.nodes[id]!]),
    ),
    ports: Object.fromEntries(
      Object.entries(graph.ports).filter(([, p]) => set.has(p.nodeId)),
    ),
    edges: Object.fromEntries(
      Object.entries(graph.edges).filter(
        ([, e]) => set.has(e.sourceNodeId) && set.has(e.targetNodeId),
      ),
    ),
  };
}
export function boundaryLinks(graph: BoardGraph, ids: string[]) {
  const set = new Set(ids);
  return Object.values(graph.edges).filter(
    (e) => set.has(e.sourceNodeId) !== set.has(e.targetNodeId),
  );
}
export function boardGridStep(scale: number): number {
  return (
    32 *
    2 ** Math.max(0, Math.ceil(Math.log2(16 / Math.max(0.001, scale) / 32)))
  );
}
export function cableEnvelope(rms: number, peak: number) {
  const r = Number.isFinite(rms) ? Math.max(0, rms) : 0,
    p = Number.isFinite(peak) ? Math.max(0, peak) : 0;
  return {
    body: r <= 0.001 ? 0 : Math.min(0.65, Math.sqrt(r) * 0.65),
    contact: p <= 0.001 ? 0 : Math.min(1, Math.sqrt(p)),
    clip: p > 1,
  };
}
// Editor-session view metadata, bounded by patch topology; never shared host preferences.
export const boardSessions = new Map<
  string,
  {
    key: string;
    cameras: Map<string, BoardCamera>;
    names: Map<string, string>;
    selectionIds: string[];
  }
>();
export function boardSession(signature: string) {
  let session = boardSessions.get(signature);
  if (!session) {
    session = {
      key: "board",
      cameras: new Map(),
      names: new Map(),
      selectionIds: [],
    };
    boardSessions.set(signature, session);
    if (boardSessions.size > 16)
      boardSessions.delete(boardSessions.keys().next().value!);
  }
  return session;
}

/** Reserve navigation bands and clamp detail legibility around the same world centre. */
export function fitReadableRegion(
  nodes: BoardNode[],
  view: { w: number; h: number },
  uiScale = 1,
): BoardCamera {
  const area = { w: view.w, h: Math.max(80, view.h - 112) };
  const fit = fitCamera(nodes, area),
    scale = Math.max(0.55 / Math.max(0.1, uiScale), fit.scale);
  return {
    scale,
    tx: area.w / 2 - ((area.w / 2 - fit.tx) / fit.scale) * scale,
    ty: 56 + area.h / 2 - ((area.h / 2 - fit.ty) / fit.scale) * scale,
  };
}
