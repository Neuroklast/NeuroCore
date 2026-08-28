import { bindJackXs } from "./chipLayout";
import { BOARD_GRID } from "./grid";
import { waypointToSvgPath } from "./layout/chamfer";
import type { Pt } from "./layout/types";

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

export function bindJackWorld(
  node: { x: number; y: number; w: number; h: number },
  key: string,
  keys: string[],
): Pt {
  const i = Math.max(0, keys.indexOf(key));
  const xs = bindJackXs(Math.max(keys.length, 1), node.w);
  return {
    x: node.x + (xs[i] ?? node.w * 0.5),
    y: node.y + node.h,
  };
}

export function hostPointFromWorld(
  world: Pt,
  cam: { tx: number; ty: number; scale: number },
  paneInHost: { x: number; y: number },
): Pt {
  return {
    x: paneInHost.x + world.x * cam.scale + cam.tx,
    y: paneInHost.y + world.y * cam.scale + cam.ty,
  };
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

function almost(a: number, b: number): boolean {
  return Math.abs(a - b) < 0.6;
}

function collapseColinear(pts: Pt[]): Pt[] {
  const out: Pt[] = [];
  for (const p of pts) {
    const last = out[out.length - 1];
    if (last && almost(last.x, p.x) && almost(last.y, p.y)) {
      continue;
    }
    out.push({ x: p.x, y: p.y });
  }
  let i = 1;
  while (i < out.length - 1) {
    const a = out[i - 1]!;
    const b = out[i]!;
    const c = out[i + 1]!;
    if (Math.abs((b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x)) < 1) {
      out.splice(i, 1);
    } else {
      i += 1;
    }
  }
  return out;
}

export function bindPathDirs(pts: Pt[]): Array<"N" | "E" | "W" | "S" | "?"> {
  const dirs: Array<"N" | "E" | "W" | "S" | "?"> = [];
  for (let i = 1; i < pts.length; i += 1) {
    const a = pts[i - 1]!;
    const b = pts[i]!;
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    if (Math.abs(dx) <= 1.5 && dy < -1) {
      dirs.push("N");
    } else if (Math.abs(dx) <= 1.5 && dy > 1) {
      dirs.push("S");
    } else if (Math.abs(dy) <= 1.5 && dx > 1) {
      dirs.push("E");
    } else if (Math.abs(dy) <= 1.5 && dx < -1) {
      dirs.push("W");
    } else {
      dirs.push("?");
    }
  }
  return dirs;
}

export function bindTurnCount(pts: Pt[]): number {
  return Math.max(0, collapseColinear(pts).length - 2);
}

function horizHits(
  x0: number,
  x1: number,
  y: number,
  boxes: BindRect[],
  dest: Pt,
): boolean {
  const lo = Math.min(x0, x1);
  const hi = Math.max(x0, x1);
  for (const b of boxes) {
    if (y <= b.y + 1 || y >= b.y + b.h - 1) {
      continue;
    }
    if (hi <= b.x + 1 || lo >= b.x + b.w - 1) {
      continue;
    }
    const destOn = dest.x >= b.x - 1 && dest.x <= b.x + b.w + 1
      && dest.y >= b.y - 1 && dest.y <= b.y + b.h + 1;
    if (destOn && y >= dest.y - 1) {
      continue;
    }
    return true;
  }
  return false;
}

/** Knob → dest. North off the knob, one E/W rail, North into the jack. No South, no A*. */
export function bindWnePoints(
  from: { x: number; y: number },
  to: { x: number; y: number },
  boxes: BindRect[] = [],
  knobs: BindRect[] = [],
): Pt[] {
  const knobTop = knobs.length > 0 ? Math.min(...knobs.map((k) => k.y)) : from.y;
  const src = { x: from.x, y: Math.min(from.y, knobTop) };
  const dest = { x: to.x, y: to.y };
  const solids = knobs.length > 0
    ? [...boxes, ...knobs.map((k) => ({ ...k, y: k.y + 1 }))]
    : boxes;

  if (dest.y >= src.y - 1.5) {
    const rail = src.y - BOARD_GRID;
    if (Math.abs(src.x - dest.x) <= 1.5) {
      return collapseColinear([src, { x: src.x, y: rail }]);
    }
    return collapseColinear([src, { x: src.x, y: rail }, { x: dest.x, y: rail }]);
  }

  if (Math.abs(src.x - dest.x) <= 1.5) {
    return collapseColinear([src, dest]);
  }

  const ay = src.y - BOARD_GRID;
  const rails: number[] = [dest.y + BOARD_GRID, ay];
  for (const b of solids) {
    rails.push(b.y + b.h + BOARD_GRID);
  }
  const unique = [...new Set(rails.map((y) => Math.round(y)))].filter((y) => (
    y < src.y - 1 && y > dest.y + 1
  )).sort((a, b) => a - b);
  const railY = unique.find((y) => ! horizHits(src.x, dest.x, y, solids, dest))
    ?? Math.min(ay, Math.max(dest.y + BOARD_GRID, (src.y + dest.y) * 0.5));

  return collapseColinear([
    src,
    { x: src.x, y: railY },
    { x: dest.x, y: railY },
    dest,
  ]);
}

/** Knob → dest. W/N/E only, at most two corners on a free rail. */
export function bindSmoothPath(
  from: { x: number; y: number },
  to: { x: number; y: number },
  _letterIndex: number,
  boxes: Array<{ x: number; y: number; w: number; h: number }> = [],
  _face: "top" | "bottom" = "bottom",
  knobs: BindRect[] = [],
): string {
  return waypointToSvgPath(bindWnePoints(from, to, boxes, knobs));
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
