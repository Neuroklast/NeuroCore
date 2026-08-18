import { getSmoothStepPath, Position } from "@xyflow/react";

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

function joinStep(a: string, b: string): string {
  return `${a} ${b.replace(/^M[-.\d\s]+/, "")}`;
}

function bypassY(
  from: { x: number; y: number },
  to: { x: number; y: number },
  letterIndex: number,
  boxes: Array<{ x: number; y: number; w: number; h: number }>,
): number | null {
  const left = Math.min(from.x, to.x) + 8;
  const right = Math.max(from.x, to.x) - 8;
  const hits = boxes.filter((b) => b.x < right && b.x + b.w > left);
  if (hits.length === 0) {
    return null;
  }
  const top = Math.min(...hits.map((b) => b.y));
  const bot = Math.max(...hits.map((b) => b.y + b.h));
  const below = bot + 18 + letterIndex * BIND_RAIL_PITCH;
  void top;
  return below;
}

/** Down from the knob, horizontal rail, vertical into the south (or north) jack. */
export function bindSmoothPath(
  from: { x: number; y: number },
  to: { x: number; y: number },
  letterIndex: number,
  boxes: Array<{ x: number; y: number; w: number; h: number }> = [],
  face: "top" | "bottom" = "bottom",
): string {
  const around = bypassY(from, to, letterIndex, boxes);
  const fromBelow = from.y > to.y + 12;
  const leave = fromBelow ? Position.Top : Position.Bottom;
  const viaY = face === "top"
    ? (around != null ? Math.min(around, to.y) : to.y - 28) - letterIndex * BIND_RAIL_PITCH
    : fromBelow
      ? Math.min(from.y - 16, to.y + 22) + letterIndex * BIND_RAIL_PITCH
      : (around != null ? Math.max(around, to.y) : to.y + 28) + letterIndex * BIND_RAIL_PITCH;
  const [legA] = getSmoothStepPath({
    sourceX: from.x,
    sourceY: from.y,
    sourcePosition: leave,
    targetX: to.x,
    targetY: viaY,
    targetPosition: face === "top" ? Position.Top : Position.Bottom,
    borderRadius: 0,
  });
  const [legB] = getSmoothStepPath({
    sourceX: to.x,
    sourceY: viaY,
    sourcePosition: face === "top" ? Position.Bottom : Position.Top,
    targetX: to.x,
    targetY: to.y,
    targetPosition: face === "top" ? Position.Top : Position.Bottom,
    borderRadius: 0,
  });
  return joinStep(legA, legB);
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
