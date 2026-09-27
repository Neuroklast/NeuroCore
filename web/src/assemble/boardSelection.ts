import type { BoardNode } from "./boardModel";

export type SelectionRect = { x0: number; y0: number; x1: number; y1: number };

export function nodesInSelection(nodes: Iterable<BoardNode>, r: SelectionRect): string[] {
  const left = Math.min(r.x0, r.x1);
  const right = Math.max(r.x0, r.x1);
  const top = Math.min(r.y0, r.y1);
  const bottom = Math.max(r.y0, r.y1);
  return [...nodes].filter((n) => n.x < right && n.x + n.w > left && n.y < bottom && n.y + n.h > top).map((n) => n.id);
}

export function routeMidpoint(route: Array<{ x: number; y: number }>): { x: number; y: number } | null {
  if (route.length < 2) return null;
  const segments = route.slice(1).map((p, i) => Math.hypot(p.x - route[i]!.x, p.y - route[i]!.y));
  let remaining = segments.reduce((sum, n) => sum + n, 0) * 0.5;
  for (let i = 0; i < segments.length; i += 1) {
    if (remaining <= segments[i]! || i === segments.length - 1) {
      const a = route[i]!;
      const b = route[i + 1]!;
      const t = segments[i] ? remaining / segments[i]! : 0;
      return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t };
    }
    remaining -= segments[i]!;
  }
  return null;
}
