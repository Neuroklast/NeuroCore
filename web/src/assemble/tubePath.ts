import { BOARD_PAD } from "./grid";

export type Pt = { x: number; y: number };
export type Obstacle = { id: string; x: number; y: number; w: number; h: number };

export const TUBE_STUB = 44;
export const TUBE_RADIUS = 0;
export const TUBE_CLEAR = BOARD_PAD;
/** No corner until the current run is at least this long. */
export const TUBE_MIN_RUN = 40;
const EPS = 2.5;

function sgn(n: number): number {
  return n < 0 ? -1 : n > 0 ? 1 : 0;
}

export function isOctilinearDelta(dx: number, dy: number): boolean {
  return segmentAngle(dx, dy) !== null;
}

/**
 * Canonical segment heading folded into {0, 45, 90}.
 * Absolute 135° (NW) and 180° (west) rewrite to 45 and 0; non-octilinear → null.
 */
export function segmentAngle(dx: number, dy: number): 0 | 45 | 90 | null {
  const ax = Math.abs(dx);
  const ay = Math.abs(dy);
  if (ax < EPS && ay < EPS) {
    return 0;
  }
  if (ay < EPS) {
    return 0;
  }
  if (ax < EPS) {
    return 90;
  }
  if (Math.abs(ax - ay) < EPS) {
    return 45;
  }
  return null;
}

/** Rewrite any non-{0,45,90} segment via an octilinear elbow. */
export function enforceSegmentAngles(pts: Pt[]): Pt[] {
  if (pts.length < 2) {
    return pts;
  }
  const out: Pt[] = [];
  push(out, pts[0]);
  for (let i = 1; i < pts.length; i += 1) {
    const a = out[out.length - 1]!;
    const b = pts[i]!;
    if (segmentAngle(b.x - a.x, b.y - a.y) === null) {
      for (const m of octilinearMid(a, b)) {
        push(out, m);
      }
    }
    push(out, b);
  }
  return simplify(out);
}

function almost(a: number, b: number): boolean {
  return Math.abs(a - b) < 0.6;
}

function push(out: Pt[], p: Pt) {
  const last = out[out.length - 1];
  if (last && almost(last.x, p.x) && almost(last.y, p.y)) {
    return;
  }
  out.push({ x: p.x, y: p.y });
}

function colinear(a: Pt, b: Pt, c: Pt): boolean {
  return Math.abs((b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x)) < 1;
}

export function inflate(o: Obstacle, pad: number): Obstacle {
  return { id: o.id, x: o.x - pad, y: o.y - pad, w: o.w + pad * 2, h: o.h + pad * 2 };
}

/** Liang–Barsky: true if the open segment overlaps the rect. */
export function segmentHitsRect(a: Pt, b: Pt, r: Obstacle): boolean {
  let t0 = 0;
  let t1 = 1;
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const p = [-dx, dx, -dy, dy];
  const q = [a.x - r.x, r.x + r.w - a.x, a.y - r.y, r.y + r.h - a.y];
  for (let i = 0; i < 4; i += 1) {
    if (Math.abs(p[i]) < 1e-9) {
      if (q[i] < 0) {
        return false;
      }
    } else {
      const t = q[i] / p[i];
      if (p[i] < 0) {
        t0 = Math.max(t0, t);
      } else {
        t1 = Math.min(t1, t);
      }
    }
  }
  return t0 < t1;
}

export function midHits(pts: Pt[], rects: Obstacle[]): boolean {
  if (pts.length < 2) {
    return false;
  }
  for (let i = 1; i < pts.length; i += 1) {
    const stub = pts.length >= 4 && (i === 1 || i === pts.length - 1);
    if (stub) {
      continue;
    }
    for (const r of rects) {
      if (segmentHitsRect(pts[i - 1], pts[i], r)) {
        return true;
      }
    }
  }
  return false;
}

function octilinearMid(a: Pt, b: Pt): Pt[] {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  if (Math.abs(dy) < EPS || Math.abs(dx) < EPS) {
    return [];
  }
  if (Math.abs(Math.abs(dx) - Math.abs(dy)) < EPS) {
    return [];
  }
  if (Math.abs(dx) >= Math.abs(dy)) {
    return [{ x: a.x + sgn(dx) * Math.abs(dy), y: b.y }];
  }
  return [{ x: b.x, y: a.y + sgn(dy) * Math.abs(dx) }];
}

function pathLen(pts: Pt[]): number {
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
  }
  return n;
}

export function runLength(a: Pt, b: Pt): number {
  return Math.hypot(b.x - a.x, b.y - a.y);
}

/** Audio/mod jack face is east/west. Last and first runs must stay horizontal. */
export function stubIsHorizontal(a: Pt, b: Pt): boolean {
  return Math.abs(b.y - a.y) < EPS && Math.abs(b.x - a.x) > EPS;
}

export function stubIsVertical(a: Pt, b: Pt): boolean {
  return Math.abs(b.x - a.x) < EPS && Math.abs(b.y - a.y) > EPS;
}

export function plugsHorizontal(pts: Pt[]): boolean {
  if (pts.length < 2) {
    return false;
  }
  return stubIsHorizontal(pts[0], pts[1])
    && stubIsHorizontal(pts[pts.length - 2], pts[pts.length - 1]);
}

export function ensureHorizontalPlugs(pts: Pt[]): Pt[] {
  if (pts.length < 2) {
    return pts;
  }
  const start = pts[0];
  const end = pts[pts.length - 1];
  const dir = end.x >= start.x ? 1 : -1;
  const span = Math.abs(end.x - start.x);
  const stub = Math.max(12, Math.min(TUBE_MIN_RUN, span > 1 ? span * 0.35 : TUBE_MIN_RUN));
  if (span < stub * 2 + 4) {
    const midX = (start.x + end.x) / 2;
    return simplify([start, { x: midX, y: start.y }, { x: midX, y: end.y }, end]);
  }
  const a = { x: start.x + dir * stub, y: start.y };
  const b = { x: end.x - dir * stub, y: end.y };
  return restoreOctilinear(simplify([start, a, ...pts.slice(1, -1), b, end]));
}

/** Every segment that meets a corner must be at least `min` long. */
export function minRunOk(pts: Pt[], min = TUBE_MIN_RUN): boolean {
  if (pts.length <= 2) {
    return true;
  }
  for (let i = 1; i < pts.length - 1; i += 1) {
    if (runLength(pts[i - 1], pts[i]) < min - 0.5 || runLength(pts[i], pts[i + 1]) < min - 0.5) {
      return false;
    }
  }
  return true;
}

function restoreOctilinear(pts: Pt[]): Pt[] {
  return enforceSegmentAngles(pts);
}

export function dropShortCorners(pts: Pt[], min = TUBE_MIN_RUN): Pt[] {
  let out = simplify(pts);
  let guard = 0;
  while (guard < 16 && out.length > 2 && ! minRunOk(out, min)) {
    guard += 1;
    let cut = -1;
    let shortest = Infinity;
    for (let i = 1; i < out.length - 1; i += 1) {
      const n = Math.min(runLength(out[i - 1], out[i]), runLength(out[i], out[i + 1]));
      if (n < shortest) {
        shortest = n;
        cut = i;
      }
    }
    if (cut < 0) {
      break;
    }
    out.splice(cut, 1);
    out = restoreOctilinear(out);
  }
  return out;
}

function simplify(pts: Pt[]): Pt[] {
  const out: Pt[] = [];
  for (const p of pts) {
    push(out, p);
  }
  if (out.length <= 3) {
    return out;
  }
  let i = 1;
  while (i < out.length - 1) {
    if (colinear(out[i - 1], out[i], out[i + 1])) {
      out.splice(i, 1);
    } else {
      i += 1;
    }
  }
  return out;
}

export function routePoints(
  sx: number,
  sy: number,
  tx: number,
  ty: number,
  opts?: { obstacles?: Obstacle[]; sourceId?: string; targetId?: string; reserved?: Pt[][] },
): Pt[] {
  const start = { x: sx, y: sy };
  const end = { x: tx, y: ty };
  if (Math.abs(tx - sx) < EPS) {
    const out = sx + Math.max(TUBE_MIN_RUN, TUBE_STUB);
    return simplify([start, { x: out, y: sy }, { x: out, y: ty }, end]);
  }
  const dir = tx >= sx ? 1 : -1;
  const src = opts?.obstacles?.find((o) => o.id === opts.sourceId);
  const tgt = opts?.obstacles?.find((o) => o.id === opts.targetId);

  const span = Math.abs(tx - sx);
  if (span < TUBE_MIN_RUN * 2 && Math.abs(ty - sy) < EPS) {
    return [start, end];
  }
  if (span < TUBE_MIN_RUN * 2) {
    const mid = (sx + tx) / 2;
    return ensureHorizontalPlugs(dropShortCorners(simplify([start, { x: mid, y: sy }, { x: mid, y: ty }, end])));
  }
  const stub = Math.max(TUBE_MIN_RUN, Math.min(TUBE_STUB, span * 0.22));
  let ax = sx + dir * stub;
  if (src) {
    const edge = dir > 0 ? src.x + src.w : src.x;
    ax = dir > 0 ? Math.max(ax, Math.min(edge + 8, (sx + tx) / 2)) : Math.min(ax, Math.max(edge - 8, (sx + tx) / 2));
  }
  let bx = tx - dir * stub;
  if (tgt) {
    const edge = dir > 0 ? tgt.x : tgt.x + tgt.w;
    bx = dir > 0 ? Math.min(bx, Math.max(edge - 8, (sx + tx) / 2)) : Math.max(bx, Math.min(edge + 8, (sx + tx) / 2));
  }
  if ((bx - ax) * dir < TUBE_MIN_RUN) {
    const mid = (sx + tx) / 2;
    return ensureHorizontalPlugs(dropShortCorners(simplify([start, { x: mid, y: sy }, { x: mid, y: ty }, end])));
  }

  const a = { x: ax, y: sy };
  const b = { x: bx, y: ty };
  const assemble = (mid: Pt[]) => simplify([start, a, ...mid, b, end]);

  const left = Math.min(sx, tx);
  const right = Math.max(sx, tx);
  const bodies = (opts?.obstacles ?? [])
    .filter((o) => o.id !== opts?.sourceId && o.id !== opts?.targetId)
    .map((o) => inflate(o, TUBE_CLEAR));
  const corridor = bodies.filter((o) => o.x < right && o.x + o.w > left);

  const reserved = opts?.reserved ?? [];
  const score = (pts: Pt[]) => {
    const hits = corridor.length > 0 && midHits(pts, corridor) ? 5000 : 0;
    const plug = plugsHorizontal(pts) ? 0 : 2000;
    const corners = Math.max(0, pts.length - 2);
    return hits + plug + corners * 200 + countCrossings(pts, reserved) * 800 + pathLen(pts) * 0.01;
  };

  const naive = assemble(octilinearMid(a, b));
  if (! midHits(naive, corridor) && countCrossings(naive, reserved) === 0) {
    return naive;
  }

  const rails: number[] = [sy, ty];
  for (const r of corridor) {
    rails.push(r.y - 2, r.y + r.h + 2);
  }
  rails.sort((x, y) => x - y);

  let best = naive;
  let bestScore = score(naive);
  const seen = new Set<number>();
  for (const y of rails) {
    const key = Math.round(y);
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    const cand = assemble([{ x: a.x, y }, { x: b.x, y }]);
    const s = score(cand);
    if (s < bestScore) {
      best = cand;
      bestScore = s;
    }
  }
  return best;
}

function n2(v: number): string {
  return (Math.round(v * 100) / 100).toFixed(2);
}

export function filletPath(pts: Pt[], radius = TUBE_RADIUS): { d: string; corners: number } {
  if (pts.length < 2) {
    return { d: "", corners: 0 };
  }
  if (pts.length === 2) {
    return { d: `M ${n2(pts[0].x)} ${n2(pts[0].y)} L ${n2(pts[1].x)} ${n2(pts[1].y)}`, corners: 0 };
  }
  let d = `M ${n2(pts[0].x)} ${n2(pts[0].y)}`;
  let corners = 0;
  for (let i = 1; i < pts.length - 1; i += 1) {
    const prev = pts[i - 1];
    const mid = pts[i];
    const next = pts[i + 1];
    const inLen = Math.hypot(mid.x - prev.x, mid.y - prev.y);
    const outLen = Math.hypot(next.x - mid.x, next.y - mid.y);
    const r = Math.min(radius, inLen * 0.5, outLen * 0.5);
    if (r < 1.5) {
      d += ` L ${n2(mid.x)} ${n2(mid.y)}`;
      corners += 1;
      continue;
    }
    const p1x = mid.x + ((prev.x - mid.x) / inLen) * r;
    const p1y = mid.y + ((prev.y - mid.y) / inLen) * r;
    const p2x = mid.x + ((next.x - mid.x) / outLen) * r;
    const p2y = mid.y + ((next.y - mid.y) / outLen) * r;
    d += ` L ${n2(p1x)} ${n2(p1y)} Q ${n2(mid.x)} ${n2(mid.y)} ${n2(p2x)} ${n2(p2y)}`;
    corners += 1;
  }
  const last = pts[pts.length - 1];
  d += ` L ${n2(last.x)} ${n2(last.y)}`;
  return { d, corners };
}

export function orient(a: Pt, b: Pt, c: Pt): number {
  const v = (c.y - a.y) * (b.x - a.x) - (c.x - a.x) * (b.y - a.y);
  if (Math.abs(v) < 1e-6) {
    return 0;
  }
  return v > 0 ? 1 : -1;
}

export function segmentsCross(a: Pt, b: Pt, c: Pt, d: Pt): boolean {
  if ((almost(a.x, c.x) && almost(a.y, c.y)) || (almost(a.x, d.x) && almost(a.y, d.y))
    || (almost(b.x, c.x) && almost(b.y, c.y)) || (almost(b.x, d.x) && almost(b.y, d.y))) {
    return false;
  }
  const o1 = orient(a, b, c);
  const o2 = orient(a, b, d);
  const o3 = orient(c, d, a);
  const o4 = orient(c, d, b);
  return o1 !== o2 && o3 !== o4;
}

export function polylinesCross(a: Pt[], b: Pt[]): boolean {
  for (let i = 1; i < a.length; i += 1) {
    for (let j = 1; j < b.length; j += 1) {
      if (segmentsCross(a[i - 1], a[i], b[j - 1], b[j])) {
        return true;
      }
    }
  }
  return false;
}

export function countCrossings(pts: Pt[], reserved: Pt[][]): number {
  return reserved.reduce((n, other) => n + (polylinesCross(pts, other) ? 1 : 0), 0);
}

export function tubePath(
  sx: number,
  sy: number,
  tx: number,
  ty: number,
  opts?: { obstacles?: Obstacle[]; sourceId?: string; targetId?: string; reserved?: Pt[][] },
): { d: string; corners: number; points: Pt[] } {
  const points = enforceSegmentAngles(routePoints(sx, sy, tx, ty, opts));
  return { ...filletPath(points), points };
}

export function countCurves(d: string): number {
  return (d.match(/[QC] /g) ?? []).length;
}
