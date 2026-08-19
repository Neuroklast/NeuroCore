import { BOARD_GRID } from "../grid";
import { aroundFallback, astarRoute, cellsToPoints, hvhFallback, pathTurns } from "./astar";
import { chamferWaypoints, hasLightning, waypointToSvgPath } from "./chamfer";
import { packRows } from "./compactPack";
import { placeWithElk } from "./elkPlace";
import { GridMap, portInCell, portOutCell } from "./gridMap";
import { DIR_E, DIR_W, type LayoutEdge, type LayoutMode, type LayoutNode, type LayoutResult, type LayoutView, type Pt } from "./types";

function portY(nodeY: number, localY: number): number {
  return nodeY + localY;
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

/** Jacks own a 32 px east stub. Mid path stays between the stubs so dest never U-turns. */
export function pinJackStubs(from: Pt, to: Pt, pts: Pt[]): Pt[] {
  const ax = from.x + BOARD_GRID;
  const bx = to.x - BOARD_GRID;
  const lo = Math.min(ax, bx);
  const hi = Math.max(ax, bx);
  const mid: Pt[] = [];
  for (const p of pts) {
    if (almost(p.x, from.x) && almost(p.y, from.y)) {
      continue;
    }
    if (almost(p.x, to.x) && almost(p.y, to.y)) {
      continue;
    }
    const x = Math.max(lo, Math.min(hi, p.x));
    const last = mid[mid.length - 1];
    if (last && almost(last.x, x) && almost(last.y, p.y)) {
      continue;
    }
    mid.push({ x, y: p.y });
  }
  return collapseColinear([
    { x: from.x, y: from.y },
    { x: ax, y: from.y },
    ...mid,
    { x: bx, y: to.y },
    { x: to.x, y: to.y },
  ]);
}

export function pathLength(pts: Pt[]): number {
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.abs(pts[i]!.x - pts[i - 1]!.x) + Math.abs(pts[i]!.y - pts[i - 1]!.y);
  }
  return n;
}

export async function runLayout(
  mode: LayoutMode,
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: LayoutView = { w: 960, h: 420 },
): Promise<LayoutResult> {
  const placed = mode === "REROUTE"
    ? Object.fromEntries(nodes.map((n) => [n.id, {
      x: n.x ?? 0,
      y: n.y ?? 0,
      w: n.w,
      h: n.h,
    }]))
    : mode === "COMPACT"
      ? packRows(nodes, edges, view)
      : await placeWithElk(nodes, edges, mode);

  const byId = new Map(nodes.map((n) => [n.id, n]));
  const map = new GridMap();
  for (const n of nodes) {
    const p = placed[n.id];
    if (! p) {
      continue;
    }
    map.markNode(n, p.x, p.y);
  }

  const jobs = edges.map((e) => {
    const s = placed[e.source];
    const t = placed[e.target];
    const sn = byId.get(e.source);
    const tn = byId.get(e.target);
    const out = sn?.outs.find((p) => p.id === e.fromJack) ?? sn?.outs[0];
    const inn = tn?.ins.find((p) => p.id === e.toJack) ?? tn?.ins[0];
    const sy = s && out ? portY(s.y, out.y) : 0;
    const ty = t && inn ? portY(t.y, inn.y) : 0;
    const from: Pt = s ? { x: s.x + s.w, y: sy } : { x: 0, y: 0 };
    const to: Pt = t ? { x: t.x, y: ty } : { x: 0, y: 0 };
    const start = s ? portOutCell(s.x, s.w, sy) : { c: 0, r: 0 };
    const goal = t ? portInCell(t.x, ty) : { c: 0, r: 0 };
    map.markExitStub(start.c, start.r);
    map.markEntryStub(goal.c, goal.r);
    map.reserveRunway(e.id, start, 1, 3);
    map.reserveRunway(e.id, goal, -1, 3);
    return {
      e,
      from,
      to,
      start,
      goal,
      man: Math.abs(from.x - to.x) + Math.abs(from.y - to.y),
    };
  });
  map.finishHalo();
  jobs.sort((a, b) => a.man - b.man);

  const edgePaths: Record<string, string> = {};
  for (const job of jobs) {
    let cells = astarRoute(map, job.start, job.goal, DIR_E, false, job.e.id);
    if (! cells || cells.length === 0) {
      cells = astarRoute(map, job.start, job.goal, DIR_W, true, job.e.id);
    }
    let pts: Pt[];
    if (cells && cells.length > 0) {
      map.occupy(cells, pathTurns(cells));
      pts = [job.from, ...cellsToPoints(map, cells), job.to];
    } else {
      const aroundY = job.from.y < job.to.y
        ? Math.min(job.from.y, job.to.y) - BOARD_GRID * 2
        : Math.max(job.from.y, job.to.y) + BOARD_GRID * 2;
      pts = aroundFallback(job.from, job.to, aroundY);
    }
    if (pathLength(pts) < BOARD_GRID) {
      pts = hvhFallback(job.from, job.to);
    }
    pts = pinJackStubs(job.from, job.to, pts);
    const cut = chamferWaypoints(pts);
    pts = hasLightning(cut) ? pts : cut;
    pts = pinJackStubs(job.from, job.to, pts);
    edgePaths[job.e.id] = waypointToSvgPath(pts);
  }

  return { nodes: placed, edgePaths };
}

export function arrange(nodes: LayoutNode[], edges: LayoutEdge[], view?: LayoutView): Promise<LayoutResult> {
  return runLayout("ARRANGE", nodes, edges, view);
}

export function compact(nodes: LayoutNode[], edges: LayoutEdge[], view?: LayoutView): Promise<LayoutResult> {
  return runLayout("COMPACT", nodes, edges, view);
}

export function reroute(nodes: LayoutNode[], edges: LayoutEdge[]): Promise<LayoutResult> {
  return runLayout("REROUTE", nodes, edges);
}

export function bboxArea(nodes: Record<string, { x: number; y: number; w: number; h: number }>): number {
  const vals = Object.values(nodes);
  if (vals.length === 0) {
    return 0;
  }
  const x0 = Math.min(...vals.map((n) => n.x));
  const y0 = Math.min(...vals.map((n) => n.y));
  const x1 = Math.max(...vals.map((n) => n.x + n.w));
  const y1 = Math.max(...vals.map((n) => n.y + n.h));
  return Math.max(0, x1 - x0) * Math.max(0, y1 - y0);
}

export { BOARD_GRID };
