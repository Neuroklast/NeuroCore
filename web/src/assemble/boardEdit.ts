import { BOARD_HIT, portGlobal, type BoardEdge, type BoardGraph, type BoardNode } from "./boardModel";
import { stubRoute } from "./boardPath";
import { chipMenuActions, type ChipMenuAction } from "./chipMenu";

export type BoardHit =
  | { kind: "pane" }
  | { kind: "chip"; id: string }
  | { kind: "edge"; id: string; sourceId: string; targetId: string };

export function canDeleteChip(node: Pick<BoardNode, "id" | "role"> | undefined): boolean {
  if (! node) {
    return false;
  }
  if (node.role === "io") {
    return false;
  }
  const id = node.id.toLowerCase();
  return id !== "in" && id !== "out";
}

export function chipBoardActions(): ChipMenuAction[] {
  return chipMenuActions();
}

export function chipExpandAction(role: string, type: string): "inspect" | "none" {
  if (role === "chip" || type === "out") {
    return "inspect";
  }
  return "none";
}

/** Knob drag over a DSP chip opens bind sockets. IO stays shut. */
export function bindHoverOpensChip(dragging: boolean, over: boolean, role: string): boolean {
  return dragging && over && role === "chip";
}

export function chipAtWorld(
  nodes: BoardNode[],
  world: { x: number; y: number },
  extraH: (n: BoardNode) => number = () => 0,
): string | null {
  let hit: { id: string; area: number } | null = null;
  for (const n of nodes) {
    const h = n.h + extraH(n);
    if (world.x < n.x || world.y < n.y || world.x > n.x + n.w || world.y > n.y + h) {
      continue;
    }
    const area = n.w * h;
    if (! hit || area < hit.area) {
      hit = { id: n.id, area };
    }
  }
  return hit?.id ?? null;
}

function distPointSeg(
  p: { x: number; y: number },
  a: { x: number; y: number },
  b: { x: number; y: number },
): number {
  const vx = b.x - a.x;
  const vy = b.y - a.y;
  const den = vx * vx + vy * vy;
  const t = den <= 1e-6 ? 0 : Math.max(0, Math.min(1, ((p.x - a.x) * vx + (p.y - a.y) * vy) / den));
  return Math.hypot(p.x - (a.x + t * vx), p.y - (a.y + t * vy));
}

export function distToRoute(p: { x: number; y: number }, route: Array<{ x: number; y: number }>): number {
  if (route.length < 2) {
    return Number.POSITIVE_INFINITY;
  }
  let best = Number.POSITIVE_INFINITY;
  for (let i = 1; i < route.length; i += 1) {
    best = Math.min(best, distPointSeg(p, route[i - 1]!, route[i]!));
  }
  return best;
}

export function edgeRoute(graph: BoardGraph, edge: BoardEdge): Array<{ x: number; y: number }> {
  if (edge.route.length >= 2) {
    return edge.route;
  }
  const sn = graph.nodes[edge.sourceNodeId];
  const tn = graph.nodes[edge.targetNodeId];
  const sp = graph.ports[edge.sourcePortId];
  const tp = graph.ports[edge.targetPortId];
  if (! sn || ! tn || ! sp || ! tp) {
    return [];
  }
  return stubRoute(portGlobal(sn, sp), portGlobal(tn, tp));
}

export function hitBoardEdge(
  world: { x: number; y: number },
  graph: BoardGraph,
  maxDist = BOARD_HIT * 0.5,
): BoardEdge | null {
  let best: { edge: BoardEdge; d: number } | null = null;
  for (const e of Object.values(graph.edges)) {
    const d = distToRoute(world, edgeRoute(graph, e));
    if (d <= maxDist && (! best || d < best.d)) {
      best = { edge: e, d };
    }
  }
  return best?.edge ?? null;
}

/** Circuit labels are not a text document. Search and inspect fields stay selectable. */
export function circuitAllowsTextSelect(target: "pane" | "chip" | "field"): boolean {
  return target === "field";
}

/** Chip wins over cable. Empty pane is add. */
export function boardContextHit(
  chipId: string | null,
  world: { x: number; y: number },
  graph: BoardGraph,
): BoardHit {
  if (chipId && graph.nodes[chipId]) {
    return { kind: "chip", id: chipId };
  }
  const edge = hitBoardEdge(world, graph);
  if (edge) {
    return { kind: "edge", id: edge.id, sourceId: edge.sourceNodeId, targetId: edge.targetNodeId };
  }
  return { kind: "pane" };
}
