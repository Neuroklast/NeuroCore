

import { waypointToSvgPath } from "./layout/chamfer";
import { routeBoardTrace } from "./layout/routeTrace";

export type BindLink = {
  letter: string;
  node: string;
  key: string;
};

export type BindTarget = {
  letter: string;
  node: string;
};

export function lettersInExpr(expr: string): string[] {
  const found: string[] = [];
  const seen = new Set<string>();
  for (const m of expr.match(/\b[a-f]\b/gi) ?? []) {
    const letter = m.toLowerCase();
    if (seen.has(letter)) {
      continue;
    }
    seen.add(letter);
    found.push(letter);
  }
  return found;
}

export function bindLinks(nodes: Array<{ id: string; args: Record<string, unknown> }>): BindLink[] {
  const out: BindLink[] = [];
  for (const n of nodes) {
    for (const [key, val] of Object.entries(n.args)) {
      if (typeof val !== "string") {
        continue;
      }
      for (const letter of lettersInExpr(val)) {
        out.push({ letter, node: n.id, key });
      }
    }
  }
  return out;
}

export function bindTargets(nodes: Array<{ id: string; args: Record<string, unknown> }>): BindTarget[] {
  const out: BindTarget[] = [];
  const seen = new Set<string>();
  for (const l of bindLinks(nodes)) {
    const id = `${l.letter}:${l.node}`;
    if (seen.has(id)) {
      continue;
    }
    seen.add(id);
    out.push({ letter: l.letter, node: l.node });
  }
  return out;
}

export const BIND_RAIL0 = 156;
export const BIND_RAIL_PITCH = 10;

export type BindRect = { x: number; y: number; w: number; h: number };

/** True if a horizontal run sits inside the knob-card band. */
export function bindHitsKnobs(d: string, knobs: BindRect[]): boolean {
  if (knobs.length === 0) {
    return false;
  }
  const top = Math.min(...knobs.map((k) => k.y));
  const bot = Math.max(...knobs.map((k) => k.y + k.h));
  const pts = svgPathPoints(d);
  for (let i = 1; i < pts.length; i += 1) {
    const a = pts[i - 1]!;
    const b = pts[i]!;
    const horiz = Math.abs(a.y - b.y) <= 1.5 && Math.abs(a.x - b.x) > 2;
    if (horiz && a.y >= top && a.y <= bot) {
      return true;
    }
  }
  return false;
}

/** Knob → dest. Same A* + 32 px stubs + 45° chamfer as the audio tubes. */
export function bindSmoothPath(
  from: { x: number; y: number },
  to: { x: number; y: number },
  letterIndex: number,
  boxes: Array<{ x: number; y: number; w: number; h: number }> = [],
  _face: "top" | "bottom" = "bottom",
  knobs: BindRect[] = [],
): string {
  const knobTop = knobs.length > 0 ? Math.min(...knobs.map((k) => k.y)) : from.y;
  const src = { x: from.x, y: Math.min(from.y, knobTop) };
  const dest = { x: to.x, y: to.y };
  const solids = knobs.length > 0
    ? [...boxes, ...knobs.map((k) => ({ ...k, y: k.y + 1 }))]
    : boxes;
  const pts = routeBoardTrace(src, dest, solids, `bind${letterIndex}`);
  return waypointToSvgPath(pts);
}

export function svgPathPoints(d: string): Array<{ x: number; y: number }> {
  const nums = [...d.matchAll(/-?\d+(?:\.\d+)?/g)].map((m) => Number(m[0]));
  const pts: Array<{ x: number; y: number }> = [];
  for (let i = 0; i + 1 < nums.length; i += 2) {
    pts.push({ x: nums[i], y: nums[i + 1] });
  }
  return pts;
}

export function lastRunVertical(d: string): boolean {
  const pts = svgPathPoints(d);
  if (pts.length < 2) {
    return false;
  }
  const a = pts[pts.length - 2];
  const b = pts[pts.length - 1];
  return Math.abs(a.x - b.x) <= 1.5 && Math.abs(a.y - b.y) > 2;
}

export function firstRunVertical(d: string): boolean {
  const pts = svgPathPoints(d);
  if (pts.length < 2) {
    return false;
  }
  return Math.abs(pts[0].x - pts[1].x) <= 1.5 && Math.abs(pts[0].y - pts[1].y) > 2;
}

export function bindEndId(letter: string, node: string): string {
  return `${letter}:${node}`;
}

/** Cyan param tubes stay off until that knob is hovered or dragged. */
export function bindCableVisible(letter: string, hover: string | null, drag: string | null): boolean {
  return letter === hover || letter === drag;
}
