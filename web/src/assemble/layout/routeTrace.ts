import { BOARD_GRID } from "../grid";
import { aroundFallback, astarRoute, cellsToPoints, hvhFallback } from "./astar";
import { chamferWaypoints, hasLightning } from "./chamfer";
import { GridMap } from "./gridMap";
import { DIR_E, DIR_N, DIR_W, type LayoutNode, type Pt } from "./types";

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

/** First 32 px north off the knob, last 32 px north into the south jack. */
export function pinBindStubs(from: Pt, to: Pt, pts: Pt[]): Pt[] {
  const ay = from.y - BOARD_GRID;
  const by = to.y + BOARD_GRID;
  if (from.y - to.y <= BOARD_GRID + 0.5 && Math.abs(from.x - to.x) <= 0.6) {
    return collapseColinear([from, to]);
  }
  if (ay <= by + 0.5) {
    const midY = Math.min(from.y, Math.max(to.y, (from.y + to.y) * 0.5));
    return collapseColinear([from, { x: from.x, y: midY }, { x: to.x, y: midY }, to]);
  }
  const lo = by;
  const hi = ay;
  const mid: Pt[] = [];
  for (const p of pts) {
    if (almost(p.x, from.x) && almost(p.y, from.y)) {
      continue;
    }
    if (almost(p.x, to.x) && almost(p.y, to.y)) {
      continue;
    }
    const y = Math.max(lo, Math.min(hi, p.y));
    const last = mid[mid.length - 1];
    if (last && almost(last.x, p.x) && almost(last.y, y)) {
      continue;
    }
    mid.push({ x: p.x, y });
  }
  return collapseColinear([
    { x: from.x, y: from.y },
    { x: from.x, y: ay },
    ...mid,
    { x: to.x, y: by },
    { x: to.x, y: to.y },
  ]);
}

export function routeBoardTrace(
  from: Pt,
  to: Pt,
  boxes: Array<{ x: number; y: number; w: number; h: number }>,
  edgeId = "bind",
): Pt[] {
  const map = new GridMap();
  boxes.forEach((b, i) => {
    const n: LayoutNode = { id: `box${i}`, w: b.w, h: b.h, ins: [], outs: [] };
    map.markNode(n, b.x, b.y);
  });
  const start = {
    c: Math.floor(from.x / BOARD_GRID),
    r: Math.floor(from.y / BOARD_GRID) - 1,
  };
  const goal = {
    c: Math.floor(to.x / BOARD_GRID),
    r: Math.ceil(to.y / BOARD_GRID),
  };
  map.markExitStub(start.c, start.r);
  map.markEntryStub(goal.c, goal.r);
  map.reserveRunway(edgeId, start, 1, 2);
  map.reserveRunway(edgeId, goal, -1, 2);
  map.expandTo(start.c, start.r);
  map.expandTo(goal.c, goal.r);
  map.finishHalo();
  let cells = astarRoute(map, start, goal, DIR_N, false, edgeId);
  if (! cells || cells.length === 0) {
    cells = astarRoute(map, start, goal, DIR_E, false, edgeId);
  }
  if (! cells || cells.length === 0) {
    cells = astarRoute(map, start, goal, DIR_W, true, edgeId);
  }
  let pts: Pt[];
  if (cells && cells.length > 0) {
    pts = [from, ...cellsToPoints(map, cells), to];
  } else {
    const aroundY = from.y > to.y
      ? to.y + BOARD_GRID
      : from.y - BOARD_GRID;
    pts = aroundFallback(from, to, aroundY);
    if (Math.abs(from.x - to.x) + Math.abs(from.y - to.y) < BOARD_GRID) {
      pts = hvhFallback(from, to);
    }
  }
  pts = pinBindStubs(from, to, pts);
  const cut = chamferWaypoints(pts);
  pts = hasLightning(cut) ? pts : cut;
  return pinBindStubs(from, to, pts);
}
