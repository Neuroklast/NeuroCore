import { BOARD_GRID } from "../grid";
import { H, V, type GridMap } from "./gridMap";
import {
  CROSS_COST,
  DIR_E,
  DIR_W,
  HALO_COST,
  INF,
  STEP_COST,
  TURN_COST,
  type Cell,
  type Dir,
  type Pt,
} from "./types";

const DC = [1, 0, -1, 0];
const DR = [0, -1, 0, 1];

type State = { c: number; r: number; dir: Dir; g: number; f: number; prev: number };

function heapPush(h: State[], s: State): void {
  h.push(s);
  let i = h.length - 1;
  while (i > 0) {
    const p = (i - 1) >> 1;
    if (h[p]!.f <= h[i]!.f) {
      break;
    }
    const t = h[p]!;
    h[p] = h[i]!;
    h[i] = t;
    i = p;
  }
}

function heapPop(h: State[]): State | undefined {
  if (h.length === 0) {
    return undefined;
  }
  const top = h[0]!;
  const last = h.pop()!;
  if (h.length === 0) {
    return top;
  }
  h[0] = last;
  let i = 0;
  for (;;) {
    const l = i * 2 + 1;
    const r = l + 1;
    let s = i;
    if (l < h.length && h[l]!.f < h[s]!.f) {
      s = l;
    }
    if (r < h.length && h[r]!.f < h[s]!.f) {
      s = r;
    }
    if (s === i) {
      break;
    }
    const t = h[s]!;
    h[s] = h[i]!;
    h[i] = t;
    i = s;
  }
  return top;
}

function manhattan(a: Cell, b: Cell): number {
  return (Math.abs(a.c - b.c) + Math.abs(a.r - b.r)) * STEP_COST;
}

function stepBit(dir: Dir): number {
  return dir === DIR_E || dir === DIR_W ? H : V;
}

export function astarRoute(
  map: GridMap,
  start: Cell,
  goal: Cell,
  startDir: Dir = DIR_E,
  ignoreOverlap = false,
  edgeId = "",
): Cell[] | null {
  map.expandTo(start.c, start.r);
  map.expandTo(goal.c, goal.r);
  const heap: State[] = [];
  const seen = new Map<string, number>();
  const startState: State = {
    c: start.c,
    r: start.r,
    dir: startDir,
    g: 0,
    f: manhattan(start, goal),
    prev: -1,
  };
  heapPush(heap, startState);
  const all: State[] = [];
  while (heap.length > 0) {
    const cur = heapPop(heap)!;
    const sk = `${cur.c},${cur.r},${cur.dir}`;
    const prevG = seen.get(sk);
    if (prevG !== undefined && prevG <= cur.g) {
      continue;
    }
    seen.set(sk, cur.g);
    const idx = all.length;
    all.push(cur);
    if (cur.c === goal.c && cur.r === goal.r) {
      const cells: Cell[] = [];
      let s: State | undefined = cur;
      while (s) {
        cells.push({ c: s.c, r: s.r });
        s = s.prev >= 0 ? all[s.prev] : undefined;
      }
      cells.reverse();
      return cells;
    }
    for (let nd = 0 as Dir; nd < 4; nd = (nd + 1) as Dir) {
      if (cur.c === start.c && cur.r === start.r) {
        const eastC = start.c + DC[startDir]!;
        const eastR = start.r + DR[startDir]!;
        const eastOpen = ! map.solid.has(map.key(eastC, eastR));
        if (eastOpen && nd !== startDir) {
          continue;
        }
        if (! eastOpen && nd === DIR_W) {
          continue;
        }
      }
      const nc = cur.c + DC[nd]!;
      const nr = cur.r + DR[nd]!;
      if (! map.inBounds(nc, nr) && Math.abs(nc - goal.c) + Math.abs(nr - goal.r) > 2) {
        map.expandTo(nc, nr, 2);
      }
      const kk = map.key(nc, nr);
      if (map.solid.has(kk)) {
        continue;
      }
      if (edgeId && map.foreignLane(nc, nr, edgeId)) {
        continue;
      }
      const occ = map.cable.get(kk) ?? 0;
      const bit = stepBit(nd);
      const myPort = (nc === goal.c && nr === goal.r) || (nc === start.c && nr === start.r);
      if (! ignoreOverlap && occ !== 0 && ! myPort) {
        if ((occ & bit) !== 0) {
          continue;
        }
      }
      if (! ignoreOverlap && map.turns.has(kk) && nd !== cur.dir) {
        continue;
      }
      let add = STEP_COST;
      if (nd !== cur.dir) {
        add += TURN_COST;
      }
      if (map.halo.has(kk)) {
        add += HALO_COST;
      }
      if (! myPort && occ !== 0 && (occ & bit) === 0) {
        add += CROSS_COST;
      }
      if (add >= INF) {
        continue;
      }
      const next: State = {
        c: nc,
        r: nr,
        dir: nd,
        g: cur.g + add,
        f: cur.g + add + manhattan({ c: nc, r: nr }, goal),
        prev: idx,
      };
      heapPush(heap, next);
    }
    if (all.length > 80_000) {
      break;
    }
  }
  return null;
}

export function cellsToPoints(map: GridMap, cells: Cell[]): Pt[] {
  return cells.map((cell) => map.cellCenter(cell.c, cell.r));
}

export function hvhFallback(from: Pt, to: Pt): Pt[] {
  const ax = from.x + BOARD_GRID;
  const bx = to.x - BOARD_GRID;
  if (Math.abs(from.y - to.y) < 1 && ax <= bx) {
    return [from, to];
  }
  return [from, { x: ax, y: from.y }, { x: ax, y: to.y }, { x: bx, y: to.y }, to];
}

/** Go around a cluster instead of T-joining a sibling rail. */
export function aroundFallback(from: Pt, to: Pt, aroundY: number): Pt[] {
  const ax = from.x + BOARD_GRID;
  const bx = to.x - BOARD_GRID;
  return [
    from,
    { x: ax, y: from.y },
    { x: ax, y: aroundY },
    { x: bx, y: aroundY },
    { x: bx, y: to.y },
    to,
  ];
}

export function pathTurns(cells: Cell[]): Cell[] {
  const turns: Cell[] = [];
  for (let i = 1; i < cells.length - 1; i += 1) {
    const a = cells[i - 1]!;
    const b = cells[i]!;
    const c = cells[i + 1]!;
    const d0 = { c: b.c - a.c, r: b.r - a.r };
    const d1 = { c: c.c - b.c, r: c.r - b.r };
    if (d0.c !== d1.c || d0.r !== d1.r) {
      turns.push(b);
    }
  }
  return turns;
}
