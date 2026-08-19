import { Position } from "@xyflow/react";
import { BOARD_GRID, BOARD_HALF, BOARD_PAD, BOARD_RAIL, snapToCellCenter } from "./grid";
import { inflate, midHits, segmentAngle, type Obstacle } from "./tubePath";

export type Pt = { x: number; y: number };
export type { Obstacle };

/** Port leaves/enters to the first cell midline. */
export const STUB_CELLS = 1;
/** Stub to the first cell midline outside a one-cell chip pad. */
export const TUBE_STUB = BOARD_GRID + BOARD_HALF;
/** Shortest 45° that stays on midlines: one cell east and one cell south. */
export const DIAG_MIN = BOARD_GRID;
/** Center-to-center gap. Same air as chip pad, visually. */
export const TUBE_RAIL = BOARD_RAIL;

export type AudioStepOpts = {
  sourcePosition?: Position;
  targetPosition?: Position;
  centerX?: number;
  reservedXs?: number[];
  reservedPaths?: Pt[][];
  obstacles?: Obstacle[];
  sourceId?: string;
  targetId?: string;
};

export function pickCenterX(sx: number, tx: number, reservedXs: number[] = []): number {
  const mid = snapToCellCenter((sx + tx) / 2);
  const candidates = [mid];
  for (let k = 1; k <= 10; k += 1) {
    candidates.push(snapToCellCenter(sx + TUBE_STUB + (k - 1) * TUBE_RAIL));
    candidates.push(snapToCellCenter(mid + k * TUBE_RAIL));
    candidates.push(snapToCellCenter(mid - k * TUBE_RAIL));
  }
  for (const x of candidates) {
    if (reservedXs.every((r) => Math.abs(r - x) >= TUBE_RAIL)) {
      return x;
    }
  }
  return mid + reservedXs.length * TUBE_RAIL;
}

export function verticalRails(pts: Pt[]): number[] {
  const xs: number[] = [];
  for (let i = 1; i < pts.length; i += 1) {
    if (Math.abs(pts[i].x - pts[i - 1].x) < 1 && Math.abs(pts[i].y - pts[i - 1].y) > 4) {
      xs.push(pts[i].x);
    }
  }
  return xs;
}

/** Always a whole number of grid cells. Never shrink at the port. */
export function stubOffset(_sx?: number, _tx?: number): number {
  return TUBE_STUB;
}

/** Next cell midline at or east of n. */
function snapEast(n: number): number {
  const c = snapToCellCenter(n);
  return c + 1e-6 < n ? c + BOARD_GRID : c;
}

/** Next cell midline at or west of n. */
function snapWest(n: number): number {
  const c = snapToCellCenter(n);
  return c > n + 1e-6 ? c - BOARD_GRID : c;
}

function allPads(opts?: AudioStepOpts): Obstacle[] {
  return (opts?.obstacles ?? []).map((o) => inflate(o, BOARD_PAD));
}

function polylineD(pts: Pt[]): string {
  if (pts.length === 0) {
    return "";
  }
  let d = `M${pts[0].x} ${pts[0].y}`;
  for (let i = 1; i < pts.length; i += 1) {
    d += `L${pts[i].x} ${pts[i].y}`;
  }
  return d;
}

/** True if A-B is H and B-C is V, or the reverse. */
function isRightAngle(a: Pt, b: Pt, c: Pt): boolean {
  const abH = Math.abs(a.y - b.y) < 1.5 && Math.abs(a.x - b.x) > 1;
  const abV = Math.abs(a.x - b.x) < 1.5 && Math.abs(a.y - b.y) > 1;
  const bcH = Math.abs(b.y - c.y) < 1.5 && Math.abs(b.x - c.x) > 1;
  const bcV = Math.abs(b.x - c.x) < 1.5 && Math.abs(b.y - c.y) > 1;
  return (abH && bcV) || (abV && bcH);
}

/**
 * Cut every 90° H/V corner with a 45° of at least one cell (√2 × 32).
 * Long runs stay H or V. A one-cell height change becomes a single 45°.
 */
export function filletCorners(pts: Pt[], cut = DIAG_MIN): Pt[] {
  const src = cleanPts(pts);
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
    const d = Math.min(cut, inLen, outLen);
    if (d < DIAG_MIN - 0.5) {
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
  return cleanPts(out);
}

/** East stub, vertical rail, west stub. Corners get filleted after. */
function hvhVia(
  sx: number,
  sy: number,
  tx: number,
  ty: number,
  offset: number,
  railShift = 0,
  viaY?: number,
): { d: string; points: Pt[]; centerX: number } {
  const ax = snapEast(sx + offset + railShift);
  const bx = snapWest(tx - BOARD_PAD);
  const railY = snapToCellCenter(viaY ?? ty);
  if (Math.abs(sy - ty) < 1 && Math.abs(railY - sy) < 1 && ax < bx) {
    const points: Pt[] = [{ x: sx, y: sy }, { x: tx, y: ty }];
    return { d: polylineD(points), points, centerX: ax };
  }
  if (Math.abs(railY - ty) < 1 && Math.abs(railY - sy) >= 1 && Math.abs(railY - sy) <= DIAG_MIN + 1) {
    const s = railY >= sy ? 1 : -1;
    const points: Pt[] = [
      { x: sx, y: sy },
      { x: ax, y: sy },
      { x: ax + DIAG_MIN, y: sy + s * DIAG_MIN },
      { x: bx, y: ty },
      { x: tx, y: ty },
    ];
    return { d: polylineD(points), points, centerX: ax };
  }
  if (ax >= bx) {
    const mid = snapEast(sx + BOARD_HALF);
    if (mid < tx - BOARD_HALF) {
      const points = filletCorners([
        { x: sx, y: sy },
        { x: mid, y: sy },
        { x: mid, y: ty },
        { x: tx, y: ty },
      ]);
      return { d: polylineD(points), points, centerX: mid };
    }
    const west = snapWest(tx - BOARD_PAD - DIAG_MIN);
    let detour = railY;
    if (Math.abs(detour - ty) < 1 || Math.abs(detour - sy) < 1) {
      detour = snapToCellCenter(ty + (ty >= sy ? TUBE_RAIL : -TUBE_RAIL));
    }
    const points = filletCorners([
      { x: sx, y: sy },
      { x: ax, y: sy },
      { x: ax, y: detour },
      { x: west, y: detour },
      { x: west, y: ty },
      { x: tx, y: ty },
    ]);
    return { d: polylineD(points), points, centerX: mid };
  }
  const points = filletCorners([
    { x: sx, y: sy },
    { x: ax, y: sy },
    { x: ax, y: railY },
    { x: bx, y: railY },
    { x: bx, y: ty },
    { x: tx, y: ty },
  ]);
  return { d: polylineD(points), points, centerX: ax };
}

/** 45° run is at least one cell on each axis: length ≥ √2 × BOARD_GRID. */
export function diagLongEnough(pts: Pt[]): boolean {
  for (let i = 1; i < pts.length; i += 1) {
    const dx = pts[i].x - pts[i - 1].x;
    const dy = pts[i].y - pts[i - 1].y;
    if (segmentAngle(dx, dy) !== 45) {
      continue;
    }
    if (Math.abs(dx) < DIAG_MIN - 0.5 || Math.abs(dy) < DIAG_MIN - 0.5) {
      return false;
    }
  }
  return true;
}

function scorePath(
  pts: Pt[],
  pads: Obstacle[],
  reservedXs: number[],
  reservedPaths: Pt[][],
): number {
  const hits = pads.length > 0 && midHits(pts, pads) ? 8000 : 0;
  const plug = pts.length >= 2 && stubGoesEast(pts[0]!, pts[1]!)
    && stubGoesEast(pts[pts.length - 2]!, pts[pts.length - 1]!) ? 0 : 1600;
  const ortho = isOctilinearPoints(pts) && turnsAreOctilinear(pts) ? 0 : 4000;
  const xs = midRunVerticals(pts);
  let railHit = 0;
  for (const x of xs) {
    if (reservedXs.some((r) => Math.abs(r - x) < TUBE_RAIL)) {
      railHit += 600;
    }
  }
  for (const other of reservedPaths) {
    if (railsOverlap(pts, other)) {
      railHit += 2000;
    }
  }
  const corners = countCorners(pts) * 80;
  const stair = countCorners(pts) > 2 ? 900 : 0;
  const noDiag = hasDiagonal(pts) || Math.abs(pts[0]!.y - pts[pts.length - 1]!.y) < 1 ? 0 : 160;
  const shortDiag = diagLongEnough(pts) ? 0 : 800;
  return hits + plug + ortho + railHit + corners + stair + noDiag + shortDiag + pathLength(pts) * 0.02;
}

function cleanPts(pts: Pt[]): Pt[] {
  const out: Pt[] = [];
  for (const p of pts) {
    const last = out[out.length - 1];
    if (last && Math.abs(last.x - p.x) < 0.6 && Math.abs(last.y - p.y) < 0.6) {
      continue;
    }
    out.push(p);
  }
  let i = 1;
  while (i < out.length - 1) {
    const a = out[i - 1]!;
    const b = out[i]!;
    const c = out[i + 1]!;
    const colinear = Math.abs((b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x)) < 1;
    if (colinear) {
      out.splice(i, 1);
    } else {
      i += 1;
    }
  }
  return out;
}

export function countCorners(pts: Pt[]): number {
  return Math.max(0, cleanPts(pts).length - 2);
}

function ensureStubs(pts: Pt[], sx: number, sy: number, tx: number, ty: number): Pt[] {
  const out = cleanPts(pts);
  if (out.length < 2) {
    return out;
  }
  const a = out[1]!;
  if (! stubGoesEast(out[0]!, a) || a.x - sx < BOARD_HALF - 4) {
    const stub = { x: snapEast(sx + BOARD_HALF), y: sy };
    if (Math.abs(a.x - sx) < 1) {
      out[1] = { x: stub.x, y: a.y };
    }
    out.splice(1, 0, stub);
  }
  const n = out.length;
  const b = out[n - 2]!;
  if (! stubGoesEast(b, out[n - 1]!) || tx - b.x < BOARD_HALF - 4) {
    const stub = { x: snapWest(tx - BOARD_HALF), y: ty };
    if (Math.abs(b.x - tx) < 1) {
      out[n - 2] = { x: stub.x, y: b.y };
    }
    out.splice(out.length - 1, 0, stub);
  }
  return out;
}

export function audioStepPath(
  sx: number,
  sy: number,
  tx: number,
  ty: number,
  opts?: AudioStepOpts,
): { d: string; points: Pt[]; centerX: number } {
  const reserved = opts?.reservedXs ?? [];
  const reservedPaths = opts?.reservedPaths ?? [];
  const offset = TUBE_STUB;
  const pads = allPads(opts);
  const via0 = Math.abs(sy - ty) < 1 ? sy : ty;
  const naive = hvhVia(sx, sy, tx, ty, offset, 0, via0);

  const shifts = [0, TUBE_RAIL, TUBE_RAIL * 2, TUBE_RAIL * 3, -TUBE_RAIL];
  const ys = new Set<number>([
    via0,
    snapToCellCenter(sy),
    snapToCellCenter(ty),
    snapToCellCenter(sy + TUBE_RAIL),
    snapToCellCenter(sy - TUBE_RAIL),
    snapToCellCenter(ty + TUBE_RAIL),
    snapToCellCenter(ty - TUBE_RAIL),
  ]);
  let right = snapToCellCenter(Math.max(sx, tx) + TUBE_STUB);
  let bot = snapToCellCenter(Math.max(sy, ty));
  let top = snapToCellCenter(Math.min(sy, ty));
  for (const o of [...pads, ...(opts?.obstacles ?? [])]) {
    ys.add(snapToCellCenter(o.y - TUBE_RAIL));
    ys.add(snapToCellCenter(o.y + o.h + TUBE_RAIL));
    right = Math.max(right, snapToCellCenter(o.x + o.w + TUBE_RAIL));
    bot = Math.max(bot, snapToCellCenter(o.y + o.h + TUBE_RAIL));
    top = Math.min(top, snapToCellCenter(o.y - TUBE_RAIL));
  }
  ys.add(bot);
  ys.add(top);

  let best = naive;
  let bestScore = scorePath(naive.points, pads, reserved, reservedPaths);
  let bestClear: { d: string; points: Pt[]; centerX: number } | null =
    pads.length === 0 || ! midHits(naive.points, pads) ? naive : null;
  let bestClearScore = bestClear ? bestScore : Infinity;

  const consider = (cand: { d: string; points: Pt[]; centerX: number }) => {
    const pts = filletCorners(ensureStubs(cand.points, sx, sy, tx, ty));
    if (pads.length > 0 && midHits(pts, pads)) {
      return;
    }
    const next = { ...cand, points: pts, d: polylineD(pts) };
    const s = scorePath(pts, pads, reserved, reservedPaths);
    if (s < bestScore) {
      best = next;
      bestScore = s;
    }
    if (s < bestClearScore) {
      bestClear = next;
      bestClearScore = s;
    }
  };

  for (const viaY of ys) {
    for (const shift of shifts) {
      consider(hvhVia(sx, sy, tx, ty, offset, shift, viaY));
    }
  }
  for (const viaY of [bot, top]) {
    for (const shift of shifts) {
      consider(hvhVia(sx, sy, tx, ty, offset, shift + (right - (sx + offset)), viaY));
    }
  }
  if (pads.length > 0) {
    consider(aroundPads(sx, sy, tx, ty, offset, pads));
  }
  const picked = bestClear ?? (pads.length > 0 ? aroundPads(sx, sy, tx, ty, offset, pads) : best);
  const pts = filletCorners(ensureStubs(picked.points, sx, sy, tx, ty));
  return { ...picked, points: pts, d: polylineD(pts) };
}

/** Leave east if the stub is clear, else drop to a free midline, go around the pads. */
function aroundPads(
  sx: number,
  sy: number,
  tx: number,
  ty: number,
  offset: number,
  pads: Obstacle[],
): { d: string; points: Pt[]; centerX: number } {
  let viaY = snapToCellCenter(ty >= sy ? Math.max(sy, ty) + BOARD_GRID : Math.min(sy, ty) - BOARD_GRID);
  let east = snapEast(Math.max(sx, tx) + offset);
  let stubX = snapEast(sx + BOARD_HALF);
  for (const p of pads) {
    viaY = ty >= sy
      ? Math.max(viaY, snapToCellCenter(p.y + p.h + BOARD_PAD))
      : Math.min(viaY, snapToCellCenter(p.y - BOARD_PAD));
    east = Math.max(east, snapEast(p.x + p.w + BOARD_PAD));
    if (p.x > sx && sy > p.y && sy < p.y + p.h) {
      stubX = Math.min(stubX, snapWest(p.x - BOARD_PAD));
    }
  }
  if (stubX <= sx + 8) {
    stubX = snapEast(sx + BOARD_HALF);
  }
  const west = snapWest(tx - BOARD_PAD - DIAG_MIN);
  const points = filletCorners([
    { x: sx, y: sy },
    { x: stubX, y: sy },
    { x: stubX, y: viaY },
    { x: west, y: viaY },
    { x: west, y: ty },
    { x: tx, y: ty },
  ]);
  return { d: polylineD(points), points, centerX: stubX };
}

export function pathLength(pts: Pt[]): number {
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
  }
  return n;
}

export function firstLast(pts: Pt[]): { first: Pt; last: Pt } {
  return { first: pts[0] ?? { x: 0, y: 0 }, last: pts[pts.length - 1] ?? { x: 0, y: 0 } };
}

export function stubGoesEast(a: Pt, b: Pt): boolean {
  return Math.abs(a.y - b.y) < 1.5 && b.x - a.x > 8;
}

export function isOrthogonalPoints(pts: Pt[]): boolean {
  for (let i = 1; i < pts.length; i += 1) {
    const dx = pts[i].x - pts[i - 1].x;
    const dy = pts[i].y - pts[i - 1].y;
    if (Math.abs(dx) < 0.6 && Math.abs(dy) < 0.6) {
      continue;
    }
    const horiz = Math.abs(dy) < 1.5;
    const vert = Math.abs(dx) < 1.5;
    if (! horiz && ! vert) {
      return false;
    }
  }
  return true;
}

/** Horizontal, vertical, or 45° — every other slope is illegal. */
export function hasDiagonal(pts: Pt[]): boolean {
  for (let i = 1; i < pts.length; i += 1) {
    const dx = pts[i].x - pts[i - 1].x;
    const dy = pts[i].y - pts[i - 1].y;
    if (segmentAngle(dx, dy) === 45 && Math.hypot(dx, dy) > 8) {
      return true;
    }
  }
  return false;
}

export function isOctilinearPoints(pts: Pt[]): boolean {
  for (let i = 1; i < pts.length; i += 1) {
    const dx = pts[i].x - pts[i - 1].x;
    const dy = pts[i].y - pts[i - 1].y;
    if (segmentAngle(dx, dy) === null) {
      return false;
    }
  }
  return true;
}

/** Interior turn is 0°, 45°, or 90°. Acute Z / U-turns are illegal. */
export function turnsAreOctilinear(pts: Pt[]): boolean {
  const clean: Pt[] = [];
  for (const p of pts) {
    const last = clean[clean.length - 1];
    if (last && Math.abs(last.x - p.x) < 0.6 && Math.abs(last.y - p.y) < 0.6) {
      continue;
    }
    clean.push(p);
  }
  for (let i = 1; i < clean.length - 1; i += 1) {
    const ax = clean[i].x - clean[i - 1].x;
    const ay = clean[i].y - clean[i - 1].y;
    const bx = clean[i + 1].x - clean[i].x;
    const by = clean[i + 1].y - clean[i].y;
    if (Math.hypot(ax, ay) < 0.6 || Math.hypot(bx, by) < 0.6) {
      continue;
    }
    const mag = Math.hypot(ax, ay) * Math.hypot(bx, by);
    const cos = (ax * bx + ay * by) / mag;
    const ok = cos > 0.85
      || Math.abs(Math.abs(cos) - 0.7071) < 0.12
      || Math.abs(cos) < 0.15;
    if (! ok) {
      return false;
    }
  }
  return true;
}

/** Interior turn is 0° or 90°. A 45° lightning-bolt is illegal. */
export function turnsAreSquare(pts: Pt[]): boolean {
  const clean: Pt[] = [];
  for (const p of pts) {
    const last = clean[clean.length - 1];
    if (last && Math.abs(last.x - p.x) < 0.6 && Math.abs(last.y - p.y) < 0.6) {
      continue;
    }
    clean.push(p);
  }
  for (let i = 1; i < clean.length - 1; i += 1) {
    const ax = clean[i].x - clean[i - 1].x;
    const ay = clean[i].y - clean[i - 1].y;
    const bx = clean[i + 1].x - clean[i].x;
    const by = clean[i + 1].y - clean[i].y;
    if (Math.hypot(ax, ay) < 0.6 || Math.hypot(bx, by) < 0.6) {
      continue;
    }
    const dot = ax * bx + ay * by;
    const mag = Math.hypot(ax, ay) * Math.hypot(bx, by);
    const cos = dot / mag;
    if (cos > 0.2 && cos < 0.98) {
      return false;
    }
  }
  return true;
}

function midRunVerticals(pts: Pt[]): number[] {
  if (pts.length < 2) {
    return [];
  }
  const srcX = pts[0]!.x;
  const dstX = pts[pts.length - 1]!.x;
  return verticalRails(pts).filter((x) => Math.abs(x - srcX) > TUBE_STUB - 4 && Math.abs(x - dstX) > TUBE_STUB - 4);
}

function midRunHorizontals(pts: Pt[]): number[] {
  if (pts.length < 2) {
    return [];
  }
  const srcY = pts[0]!.y;
  const dstY = pts[pts.length - 1]!.y;
  const ys: number[] = [];
  for (let i = 1; i < pts.length; i += 1) {
    if (Math.abs(pts[i].y - pts[i - 1].y) < 1 && Math.abs(pts[i].x - pts[i - 1].x) > 4) {
      const y = pts[i].y;
      if (Math.abs(y - srcY) > 4 && Math.abs(y - dstY) > 4) {
        ys.push(y);
      }
    }
  }
  return ys;
}

function xSpanOnY(pts: Pt[], y: number): [number, number] | null {
  let lo = Infinity;
  let hi = -Infinity;
  for (let i = 1; i < pts.length; i += 1) {
    if (Math.abs(pts[i].y - y) < 1 && Math.abs(pts[i - 1].y - y) < 1) {
      lo = Math.min(lo, pts[i - 1].x, pts[i].x);
      hi = Math.max(hi, pts[i - 1].x, pts[i].x);
    }
  }
  return Number.isFinite(lo) ? [lo, hi] : null;
}

export function railsOverlap(a: Pt[], b: Pt[], min = TUBE_RAIL): boolean {
  const xa = midRunVerticals(a);
  const xb = midRunVerticals(b);
  for (const u of xa) {
    for (const v of xb) {
      if (Math.abs(u - v) < min) {
        const ay0 = ySpanOnX(a, u);
        const by0 = ySpanOnX(b, v);
        if (ay0 && by0 && overlap1d(ay0[0], ay0[1], by0[0], by0[1])) {
          return true;
        }
      }
    }
  }
  const ya = midRunHorizontals(a);
  const yb = midRunHorizontals(b);
  for (const u of ya) {
    for (const v of yb) {
      if (Math.abs(u - v) < min) {
        const ax0 = xSpanOnY(a, u);
        const bx0 = xSpanOnY(b, v);
        if (ax0 && bx0 && overlap1d(ax0[0], ax0[1], bx0[0], bx0[1])) {
          return true;
        }
      }
    }
  }
  return false;
}

function ySpanOnX(pts: Pt[], x: number): [number, number] | null {
  let lo = Infinity;
  let hi = -Infinity;
  for (let i = 1; i < pts.length; i += 1) {
    if (Math.abs(pts[i].x - x) < 1 && Math.abs(pts[i - 1].x - x) < 1) {
      lo = Math.min(lo, pts[i - 1].y, pts[i].y);
      hi = Math.max(hi, pts[i - 1].y, pts[i].y);
    }
  }
  return Number.isFinite(lo) ? [lo, hi] : null;
}

function overlap1d(a0: number, a1: number, b0: number, b1: number): boolean {
  return Math.min(a1, b1) - Math.max(a0, b0) > 8;
}
