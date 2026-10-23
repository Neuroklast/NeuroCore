import { placeHostTerminals, reserveHostColumn } from "../hostTerminalGeometry";
import { isWrapJack, wrapRailRoute, wrapRailY } from "../boardPath";
import { BOARD_GRID } from "../grid";
import { globalPort, northPortLocals, southPortLocals } from "../portLayout";
import { astarRoute, cellsToPoints, hvhFallback, pathTurns } from "./astar";
import { chamferWaypoints, hasLightning, waypointToSvgPath } from "./chamfer";
import { placeSerialChain } from "./serialPlacement";
import { packRows } from "./compactPack";

import { GridMap, portFaceCell } from "./gridMap";
import {
  dirFromFace,
  faceStep,
  type LayoutEdge,
  type LayoutFace,
  type LayoutMode,
  type LayoutNode,
  type LayoutPort,
  type LayoutResult,
  type LayoutView,
  type Pt,
} from "./types";

function resolvedFace(port: LayoutPort | undefined, side: "in" | "out"): LayoutFace {
  return port?.face ?? (side === "out" ? "e" : "w");
}

function resolvedLocal(port: LayoutPort | undefined, nodeW: number, nodeH: number, face: LayoutFace): Pt {
  const y = port?.y ?? 0;
  if (face === "s" || face === "n") {
    const n = Math.max(1, port?.count ?? 1);
    const locals = (face === "s" ? southPortLocals : northPortLocals)(n, { w: nodeW, h: nodeH });
    const i = Math.min(locals.length - 1, Math.max(0, port?.index ?? 0));
    return locals[i] ?? { x: nodeW * 0.5, y: face === "s" ? nodeH : 0 };
  }
  if (port?.x != null) {
    return { x: port.x, y };
  }
  if (face === "e") {
    return { x: nodeW, y };
  }
  return { x: 0, y };
}

function jackAt(
  placed: { x: number; y: number; w: number; h: number },
  node: LayoutNode | undefined,
  jackId: string,
  side: "in" | "out",
): { pt: Pt; face: LayoutFace } {
  const list = side === "out" ? node?.outs : node?.ins;
  const port = list?.find((p) => p.id === jackId);
  if (!port) throw new Error(`Missing ${side} contact ${node?.id ?? "unknown"}:${jackId}`);
  const face = resolvedFace(port, side);
  const local = resolvedLocal(port, placed.w, placed.h, face);
  return { pt: globalPort({ x: placed.x, y: placed.y }, local), face };
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

/** A* miss: wrap rail between rows, else HVH toward dest. Never around the far hull. */
export function blockedRoute(
  from: Pt,
  to: Pt,
  srcBox?: { y: number; h: number },
  dstBox?: { y: number },
): Pt[] {
  if (srcBox && dstBox && isWrapJack(from, to)) {
    return wrapRailRoute(from, to, wrapRailY(srcBox, dstBox));
  }
  return hvhFallback(from, to);
}

function stubPt(pt: Pt, face: LayoutFace): Pt {
  const s = faceStep(face);
  return { x: pt.x + s.dc * BOARD_GRID, y: pt.y + s.dr * BOARD_GRID };
}

/** Jacks own a 32 px stub along their face. East→west mid stays between the stubs so dest never U-turns. */
export function pinJackStubs(
  from: Pt,
  to: Pt,
  pts: Pt[],
  fromFace: LayoutFace = "e",
  toFace: LayoutFace = "w",
): Pt[] {
  const a = stubPt(from, fromFace);
  const b = stubPt(to, toFace);
  const clampEw = fromFace === "e" && toFace === "w";
  const lo = Math.min(a.x, b.x);
  const hi = Math.max(a.x, b.x);
  const mid: Pt[] = [];
  for (const p of pts) {
    if (almost(p.x, from.x) && almost(p.y, from.y)) {
      continue;
    }
    if (almost(p.x, to.x) && almost(p.y, to.y)) {
      continue;
    }
    const x = clampEw ? Math.max(lo, Math.min(hi, p.x)) : p.x;
    const last = mid[mid.length - 1];
    if (last && almost(last.x, x) && almost(last.y, p.y)) {
      continue;
    }
    mid.push({ x, y: p.y });
  }
  return collapseColinear([from, a, ...mid, b, to]);
}

export function pathLength(pts: Pt[]): number {
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.abs(pts[i]!.x - pts[i - 1]!.x) + Math.abs(pts[i]!.y - pts[i - 1]!.y);
  }
  return n;
}

type InkLines = Map<string, Array<{lo: number; hi: number}>>;

/** Reserve final straight or chamfered ink, not just the A* centre cells. */
function claimInk(lines: InkLines, pts: Pt[]): boolean {
  const spans = pts.slice(1).map((b, i) => {
    const a = pts[i]!;
    const vertical = almost(a.x, b.x);
    const slope = vertical ? 0 : (b.y - a.y) / (b.x - a.x);
    return {
      key: vertical ? `v:${a.x}` : `${slope}:${a.y - slope * a.x}`,
      lo: Math.min(vertical ? a.y : a.x, vertical ? b.y : b.x),
      hi: Math.max(vertical ? a.y : a.x, vertical ? b.y : b.x),
    };
  });
  if (spans.some(s => (lines.get(s.key) ?? []).some(p => Math.min(p.hi, s.hi) - Math.max(p.lo, s.lo) > 0.1))) return false;
  for (const s of spans) {
    const row = lines.get(s.key) ?? [];
    row.push(s); lines.set(s.key, row);
  }
  return true;
}

function pinHostTerminals(
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
  pinNodes: LayoutNode[],
): void {
  for (const n of pinNodes) {
    placed[n.id] = { x: n.x ?? 0, y: n.y ?? 0, w: n.w, h: n.h };
  }
  placeHostTerminals(placed);
}

export async function runLayout(
  mode: LayoutMode,
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: LayoutView = { w: 960, h: 420 },
): Promise<LayoutResult> {
  const pinNodes = nodes.filter((n) => n.pin);
  const freeNodes = nodes.filter((n) => ! n.pin);
  const pinIds = new Set(pinNodes.map((n) => n.id));
  const freeEdges = edges.filter((e) => ! pinIds.has(e.source) && ! pinIds.has(e.target));
  const extraEdges = edges.filter((e) => pinIds.has(e.source) || pinIds.has(e.target));
  const model = new Map(nodes.map((node) => [node.id, node]));
  for (const edge of edges) {
    if (!model.has(edge.source) || !model.has(edge.target)) {
      throw new Error(`Missing routing node for connection ${edge.id}`);
    }
    jackAt({x:0,y:0,w:model.get(edge.source)!.w,h:model.get(edge.source)!.h},model.get(edge.source),edge.fromJack,"out");
    jackAt({x:0,y:0,w:model.get(edge.target)!.w,h:model.get(edge.target)!.h},model.get(edge.target),edge.toJack,"in");
  }

  const serial = mode === "REROUTE" ? null : placeSerialChain(freeNodes, freeEdges, view);
  const placed = serial ?? (mode === "REROUTE"
    ? Object.fromEntries(nodes.map((n) => [n.id, {
      x: n.x ?? 0,
      y: n.y ?? 0,
      w: n.w,
      h: n.h,
    }]))
    : packRows(freeNodes, freeEdges, view));
  pinHostTerminals(placed, pinNodes);
  if(mode!=="REROUTE")reserveHostColumn(placed);

  const byId = new Map(nodes.map((n) => [n.id, n]));
  const edgePaths: Record<string, string> = {};
  if (serial && extraEdges.length === 0) {
    for (const edge of freeEdges) {
      const s = placed[edge.source]!;
      const t = placed[edge.target]!;
      const a = jackAt(s, byId.get(edge.source), edge.fromJack, "out").pt;
      const b = jackAt(t, byId.get(edge.target), edge.toJack, "in").pt;
      const sameRow = Math.abs(a.y - b.y) < 1;
      const channel = Math.max(...Object.values(placed).filter((p) => p.y <= a.y && p.y + p.h >= a.y).map((p) => p.y + p.h)) + 64;
      const pts = sameRow ? [a, b] : [a, { x: a.x + 32, y: a.y }, { x: a.x + 32, y: channel }, { x: b.x - 32, y: channel }, { x: b.x - 32, y: b.y }, b];
      edgePaths[edge.id] = waypointToSvgPath(chamferWaypoints(pts));
    }
    if (extraEdges.length === 0) {
      return { nodes: placed, edgePaths };
    }
  }

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
    const fromJack = s ? jackAt(s, byId.get(e.source), e.fromJack, "out") : { pt: { x: 0, y: 0 }, face: "e" as const };
    const toJack = t ? jackAt(t, byId.get(e.target), e.toJack, "in") : { pt: { x: 0, y: 0 }, face: "w" as const };
    const from = fromJack.pt;
    const to = toJack.pt;
    const start = portFaceCell(from.x, from.y, fromJack.face);
    const goal = portFaceCell(to.x, to.y, toJack.face);
    const leave = faceStep(fromJack.face);
    const enter = faceStep(toJack.face);
    map.markExitStub(start.c, start.r);
    map.markEntryStub(goal.c, goal.r);
    map.reserveRunway(e.id, start, leave.dc, leave.dr, 1);
    map.reserveRunway(e.id, goal, enter.dc, enter.dr, 1);
    return {
      e,
      from,
      to,
      start,
      goal,
      startDir: dirFromFace(fromJack.face),
      man: Math.abs(from.x - to.x) + Math.abs(from.y - to.y),
    };
  });
  map.finishHalo();
  jobs.sort((a, b) => a.man - b.man);

  const tried = new Set<string>();
  let routed = { ...edgePaths };
  for (let attempt = 0; attempt <= jobs.length * 2; ++attempt) {
    if (jobs.length === 0) {
      break;
    }
    const order = jobs.map(j => j.e.id).join("\0");
    if (tried.has(order)) throw new Error("Cable routing needs more space between blocks.");
    tried.add(order);
    map.cable.clear();
    map.turns.clear();
    const nextPaths = { ...edgePaths };
    const ink: InkLines = new Map();
    let failed = -1;
    for (let index = 0; index < jobs.length; ++index) {
      const job = jobs[index]!;
      const cells = astarRoute(map, job.start, job.goal, job.startDir, false, job.e.id);
      if (!cells?.length) { failed = index; break; }
      map.occupy(cells, pathTurns(cells));
      const pts = collapseColinear([job.from, ...cellsToPoints(map, cells), job.to]);
      const cut = chamferWaypoints(pts);
      const withEnds = (src: Pt[]) => {
        if (src.length === 0) {
          return [job.from, job.to];
        }
        const out = src.map((p) => ({ x: p.x, y: p.y }));
        out[0] = job.from;
        out[out.length - 1] = job.to;
        return out;
      };
      let painted = withEnds(hasLightning(cut) ? pts : cut);
      if (!claimInk(ink, painted)) {
        painted = withEnds(pts);
        if (!claimInk(ink, painted)) { failed = index; break; }
      }
      nextPaths[job.e.id] = waypointToSvgPath(painted);
    }
    if (failed < 0) {
      routed = nextPaths;
      break;
    }
    if (attempt === jobs.length * 2) throw new Error("Cable routing needs more space between blocks.");
    // Reserve the constrained connection first, then reroute displaced nets.
    jobs.unshift(...jobs.splice(failed, 1));
  }

  return { nodes: placed, edgePaths: routed };
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
