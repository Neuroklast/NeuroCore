import { BOARD_GRID } from "../grid";
import type { Pt } from "./types";

/** One cell on each axis so a 45° stays on midlines. Length = √2 × 32. */
export const CHAMFER = BOARD_GRID;

export function isDiagonal(a: Pt, b: Pt): boolean {
  return ! almost(a.x, b.x) && ! almost(a.y, b.y);
}

/** Two 45° in a row is a lightning bolt — forbidden. */
export function hasLightning(pts: Pt[]): boolean {
  for (let i = 1; i < pts.length - 1; i += 1) {
    if (isDiagonal(pts[i - 1]!, pts[i]!) && isDiagonal(pts[i]!, pts[i + 1]!)) {
      return true;
    }
  }
  return false;
}

export function firstLastHorizontal(pts: Pt[]): boolean {
  if (pts.length < 2) {
    return false;
  }
  const a = pts[0]!;
  const b = pts[1]!;
  const c = pts[pts.length - 2]!;
  const d = pts[pts.length - 1]!;
  return almost(a.y, b.y) && b.x > a.x && almost(c.y, d.y) && d.x > c.x;
}

export function waypointToSvgPath(pts: Pt[]): string {
  if (pts.length === 0) {
    return "";
  }
  let d = `M${pts[0].x} ${pts[0].y}`;
  for (let i = 1; i < pts.length; i += 1) {
    d += `L${pts[i].x} ${pts[i].y}`;
  }
  return d;
}

function almost(a: number, b: number): boolean {
  return Math.abs(a - b) < 0.6;
}

function clean(pts: Pt[]): Pt[] {
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

function isRightAngle(a: Pt, b: Pt, c: Pt): boolean {
  const abH = almost(a.y, b.y) && ! almost(a.x, b.x);
  const abV = almost(a.x, b.x) && ! almost(a.y, b.y);
  const bcH = almost(b.y, c.y) && ! almost(b.x, c.x);
  const bcV = almost(b.x, c.x) && ! almost(b.y, c.y);
  return (abH && bcV) || (abV && bcH);
}

/** Replace every 90° H/V corner with a 45° of `cut` px (default one cell). */
export function chamferWaypoints(pts: Pt[], cut = CHAMFER): Pt[] {
  const src = clean(pts);
  if (src.length < 3) {
    return src;
  }
  const out: Pt[] = [{ x: src[0]!.x, y: src[0]!.y }];
  for (let i = 1; i < src.length - 1; i += 1) {
    const a = src[i - 1]!;
    const b = src[i]!;
    const c = src[i + 1]!;
    if (! isRightAngle(a, b, c)) {
      out.push({ x: b.x, y: b.y });
      continue;
    }
    const inLen = Math.hypot(b.x - a.x, b.y - a.y);
    const outLen = Math.hypot(c.x - b.x, c.y - b.y);
    const keep = cut;
    const d = Math.min(cut, inLen - keep, outLen - keep);
    if (d < cut - 0.5) {
      out.push({ x: b.x, y: b.y });
      continue;
    }
    out.push({
      x: b.x - ((b.x - a.x) / inLen) * d,
      y: b.y - ((b.y - a.y) / inLen) * d,
    });
    out.push({
      x: b.x + ((c.x - b.x) / outLen) * d,
      y: b.y + ((c.y - b.y) / outLen) * d,
    });
  }
  out.push({ x: src[src.length - 1]!.x, y: src[src.length - 1]!.y });
  return clean(out);
}
