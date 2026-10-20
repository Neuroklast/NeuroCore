import type { BoardGraph } from "./boardModel";
export function boardBounds(
  nodes: Iterable<{ x: number; y: number; w: number; h: number }>,
  points: Iterable<{ x: number; y: number }> = [],
) {
  let x = Infinity,
    y = Infinity,
    right = -Infinity,
    bottom = -Infinity;
  for (const n of nodes) {
    x = Math.min(x, n.x);
    y = Math.min(y, n.y);
    right = Math.max(right, n.x + n.w);
    bottom = Math.max(bottom, n.y + n.h);
  }
  for (const p of points) {
    x = Math.min(x, p.x);
    y = Math.min(y, p.y);
    right = Math.max(right, p.x);
    bottom = Math.max(bottom, p.y);
  }
  return Number.isFinite(x)
    ? { x, y, right, bottom }
    : { x: 0, y: 0, right: 0, bottom: 0 };
}
export function boardRoutePoints(graph: Pick<BoardGraph, "edges">) {
  return Object.values(graph.edges).flatMap((e) => e.route ?? []);
}
