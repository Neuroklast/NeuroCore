import type { BoardEdge, BoardGraph, BoardPort, PortFace, PortKind } from "./boardModel";
import { BOARD_MAGNET, portGlobal, portIsOutput } from "./boardModel";
import { isValidLink } from "./validateLink";

export type Pt = { x: number; y: number };

export function portLinkEnd(port: BoardPort): { kind: string; output: boolean; jack: string } {
  const kind = port.kind === "sc" ? "audio" : port.kind;
  return { kind, output: portIsOutput(port), jack: port.jackId };
}

export function canLink(src: BoardPort, dst: BoardPort): boolean {
  if (src.nodeId === dst.nodeId) {
    return false;
  }
  if (src.nodeId === "MIDI" || dst.nodeId === "MIDI") {
    return false;
  }
  if (! portIsOutput(src) || portIsOutput(dst)) {
    return false;
  }
  return isValidLink(portLinkEnd(src), portLinkEnd(dst));
}

export function magnetPort(
  cursor: Pt,
  from: BoardPort,
  graph: BoardGraph,
  radius = BOARD_MAGNET,
): BoardPort | null {
  let best: { port: BoardPort; d: number } | null = null;
  for (const p of Object.values(graph.ports)) {
    if (!(portIsOutput(from) ? canLink(from, p) : canLink(p, from))) {
      continue;
    }
    const n = graph.nodes[p.nodeId];
    if (! n) {
      continue;
    }
    const g = portGlobal(n, p);
    const d = Math.hypot(g.x - cursor.x, g.y - cursor.y);
    if (d <= radius && (! best || d < best.d)) {
      best = { port: p, d };
    }
  }
  return best?.port ?? null;
}

/** Cubic preview: leave along the port normal, slack grows with distance. */
export function bezierPreview(from: Pt, to: Pt, face: PortFace): { c1: Pt; c2: Pt } {
  const dist = Math.hypot(to.x - from.x, to.y - from.y);
  const stub = Math.min(140, 48 + dist * 0.22);
  const nx = face === "e" ? 1 : face === "w" ? -1 : 0;
  const ny = face === "s" ? 1 : face === "n" ? -1 : 0;
  return {
    c1: { x: from.x + nx * stub, y: from.y + ny * stub },
    c2: { x: to.x - nx * stub, y: to.y - ny * stub },
  };
}

export function edgesAfterConnect(graph: BoardGraph, src: BoardPort, dst: BoardPort): Record<string, BoardEdge> {
  const next: Record<string, BoardEdge> = {};
  for (const [id, e] of Object.entries(graph.edges)) {
    if (e.targetPortId === dst.id) {
      continue;
    }
    if (e.sourcePortId === src.id && e.targetPortId === dst.id) {
      continue;
    }
    next[id] = e;
  }
  const id = `e-${src.nodeId}-${dst.nodeId}-${src.jackId}-${dst.jackId}`;
  const kind: PortKind = src.kind === "sc" ? "sc" : src.kind;
  next[id] = {
    id,
    sourceNodeId: src.nodeId,
    sourcePortId: src.id,
    targetNodeId: dst.nodeId,
    targetPortId: dst.id,
    kind,
    route: [],
  };
  return next;
}

export function edgesAfterCutPort(graph: BoardGraph, portId: string): Record<string, BoardEdge> {
  const next: Record<string, BoardEdge> = {};
  for (const [id, e] of Object.entries(graph.edges)) {
    if (e.sourcePortId === portId || e.targetPortId === portId) {
      continue;
    }
    next[id] = e;
  }
  return next;
}

export type ConnectDrag = {
  fromPort: BoardPort;
  from: Pt;
  to: Pt;
  snapPortId: string | null;
  kind: PortKind;
};

export const connectDragRef: { current: ConnectDrag | null } = { current: null };
