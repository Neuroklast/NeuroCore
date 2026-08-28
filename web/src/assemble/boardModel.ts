import type { AstDocument, AstEdge, AstJack, AstNode } from "../bridge/ast";
import { kindLabel } from "../theme/tokens";
import { CHIP_GAP, chipBox } from "./chipLayout";
import { BOARD_GRID, snapToGrid } from "./grid";
import { canonicalIoJacks } from "./ioPaint";
import { globalPort, sidePortLocals } from "./portLayout";
import { visualAudioEdges, visualJacksFor } from "./visualEdges";
import { inferModLinks, isLfoNode, visibleNodes } from "./flowFromAst";
import type { LayoutEdge, LayoutNode } from "./layout/types";

export type PortKind = "audio" | "mod" | "sc";

export type BoardNode = {
  id: string;
  type: string;
  role: "io" | "chip";
  x: number;
  y: number;
  w: number;
  h: number;
  args: Record<string, string>;
  label: string;
  channel: string;
  locked: boolean;
};

export type BoardPort = {
  id: string;
  nodeId: string;
  jackId: string;
  east: boolean;
  index: number;
  count: number;
  kind: PortKind;
};

export type BoardEdge = {
  id: string;
  sourceNodeId: string;
  sourcePortId: string;
  targetNodeId: string;
  targetPortId: string;
  kind: PortKind;
  route: Array<{ x: number; y: number }>;
};

export type BoardGraph = {
  nodes: Record<string, BoardNode>;
  ports: Record<string, BoardPort>;
  edges: Record<string, BoardEdge>;
};

export type BoardCamera = { tx: number; ty: number; scale: number };

export const BOARD_HIT = 24;
export const BOARD_MAGNET = 30;
export const BOARD_MIN_SCALE = 0.4;
export const BOARD_MAX_SCALE = 1;

export function portId(nodeId: string, jackId: string, east: boolean): string {
  return `${nodeId}::${jackId}::${east ? "src" : "dst"}`;
}

export function portKindOf(jack: AstJack): PortKind {
  if (jack.id === "sc" || jack.kind === "mod" && jack.id === "sc") {
    return "sc";
  }
  if (jack.kind === "mod") {
    return "mod";
  }
  return jack.id === "sc" ? "sc" : "audio";
}

export function portLocal(node: Pick<BoardNode, "w" | "h">, port: BoardPort): { x: number; y: number } {
  const locals = sidePortLocals(port.count, { w: node.w, h: node.h }, port.east);
  return locals[port.index] ?? { x: port.east ? node.w : 0, y: node.h * 0.5 };
}

export function portGlobal(node: BoardNode, port: BoardPort): { x: number; y: number } {
  return globalPort(node, portLocal(node, port));
}

function isSidechainType(type: string): boolean {
  const t = type.toLowerCase();
  return t === "sidechain" || t === "sc" || t === "scin";
}

function channelOf(node: AstNode): string {
  const bus = (node.busName || "").toLowerCase();
  if (! bus || bus === "main" || bus === "mod") {
    return "";
  }
  return bus.toUpperCase();
}

function addPorts(
  ports: Record<string, BoardPort>,
  nodeId: string,
  jacks: AstJack[],
  east: boolean,
): void {
  const side = jacks.filter((j) => j.output === east && j.kind !== "knob");
  side.forEach((j, index) => {
    const id = portId(nodeId, j.id, east);
    ports[id] = {
      id,
      nodeId,
      jackId: j.id,
      east,
      index,
      count: side.length,
      kind: j.id === "sc" ? "sc" : portKindOf(j),
    };
  });
}

function makeNode(
  id: string,
  type: string,
  role: "io" | "chip",
  x: number,
  y: number,
  jacks: AstJack[],
  args: Record<string, string>,
  label: string,
  channel: string,
  locked: boolean,
): BoardNode {
  const box = chipBox(type, jacks, false, args);
  return {
    id,
    type,
    role,
    x: snapToGrid(x),
    y: snapToGrid(y),
    w: box.w,
    h: box.h,
    args,
    label,
    channel,
    locked,
  };
}

export function previewBoardIds(ast: AstDocument | null, sidechainOn = false): string[] {
  if (! ast) {
    return [];
  }
  const vis = visibleNodes(ast, sidechainOn);
  const ids: string[] = ["IN"];
  for (const n of vis) {
    if (n.id !== "IN" && n.type !== "in") {
      ids.push(n.id);
    }
  }
  if (! vis.some((n) => n.type === "out")) {
    ids.push("OUT");
  }
  return ids;
}

export function hydrateBoard(ast: AstDocument, sidechainOn = false): BoardGraph {
  const visible = visibleNodes(ast, sidechainOn);
  const nodes: Record<string, BoardNode> = {};
  const ports: Record<string, BoardPort> = {};
  let x = BOARD_GRID;
  const rowY = BOARD_GRID * 4;

  const inJacks = canonicalIoJacks("in");
  nodes.IN = makeNode("IN", "in", "io", x, rowY, inJacks, {}, "IN", "", true);
  addPorts(ports, "IN", inJacks, true);
  x += nodes.IN.w + CHIP_GAP;

  for (const n of visible) {
    const jacks = visualJacksFor(n, visible).length ? visualJacksFor(n, visible) : (n.jacks ?? []);
    const custom = n.id.toLowerCase().startsWith("custom") || n.type.toLowerCase() === "custom";
    const role: "io" | "chip" = n.type === "out" || isSidechainType(n.type) ? "io" : "chip";
    const px = x;
    const py = rowY;
    nodes[n.id] = makeNode(
      n.id,
      isSidechainType(n.type) ? "sidechain" : n.type,
      role,
      px,
      py,
      jacks,
      n.args,
      custom ? n.id : (isSidechainType(n.type) ? "Sidechain" : kindLabel(n.type)),
      channelOf(n),
      isSidechainType(n.type),
    );
    addPorts(ports, n.id, jacks, false);
    addPorts(ports, n.id, jacks, true);
    x = px + nodes[n.id]!.w + CHIP_GAP;
  }

  if (! ast.nodes.some((n) => n.type === "out")) {
    const outJacks = canonicalIoJacks("out");
    nodes.OUT = makeNode("OUT", "out", "io", x, rowY, outJacks, {}, "OUT", "MAIN", false);
    addPorts(ports, "OUT", outJacks, false);
  }

  const explicit = visualAudioEdges(visible, ast.edges ?? []);
  const inferred = inferModLinks(visible);
  const merged: AstEdge[] = [];
  const seen = new Set<string>();
  for (const e of [...explicit, ...inferred]) {
    const key = `${e.from}>${e.to}>${e.fromJack}>${e.toJack}`;
    if (seen.has(key)) {
      continue;
    }
    seen.add(key);
    merged.push(e);
  }
  const edges: Record<string, BoardEdge> = {};
  merged.forEach((e, i) => {
    const fromJack = e.fromJack || (e.kind === "mod" ? "mod" : "out");
    const toJack = e.toJack || (e.kind === "mod" ? e.from : "in");
    const sourcePortId = portId(e.from, fromJack, true);
    const targetPortId = portId(e.to, toJack, false);
    if (! ports[sourcePortId] || ! ports[targetPortId]) {
      return;
    }
    const kind: PortKind = e.kind === "mod" ? "mod" : (fromJack === "sc" ? "sc" : "audio");
    const id = `e-${e.from}-${e.to}-${fromJack}-${toJack}-${i}`;
    edges[id] = {
      id,
      sourceNodeId: e.from,
      sourcePortId,
      targetNodeId: e.to,
      targetPortId,
      kind,
      route: [],
    };
  });

  return { nodes, ports, edges };
}

export function assignRanks(graph: BoardGraph): Record<string, number> {
  const rank: Record<string, number> = {};
  const outgoing = new Map<string, string[]>();
  for (const e of Object.values(graph.edges)) {
    const list = outgoing.get(e.sourceNodeId) ?? [];
    list.push(e.targetNodeId);
    outgoing.set(e.sourceNodeId, list);
  }
  const walk = (id: string, r: number) => {
    rank[id] = Math.max(rank[id] ?? 0, r);
    for (const t of outgoing.get(id) ?? []) {
      if ((rank[t] ?? -1) < r + 1) {
        walk(t, r + 1);
      }
    }
  };
  if (graph.nodes.IN) {
    walk("IN", 0);
  }
  for (const id of Object.keys(graph.nodes)) {
    if (rank[id] == null) {
      walk(id, 0);
    }
  }
  return rank;
}

export function graphToLayout(graph: BoardGraph): { nodes: LayoutNode[]; edges: LayoutEdge[] } {
  const nodes: LayoutNode[] = Object.values(graph.nodes).map((n) => {
    const ins = Object.values(graph.ports).filter((p) => p.nodeId === n.id && ! p.east);
    const outs = Object.values(graph.ports).filter((p) => p.nodeId === n.id && p.east);
    return {
      id: n.id,
      x: n.x,
      y: n.y,
      w: n.w,
      h: n.h,
      ins: ins.map((p) => ({ id: p.jackId, y: portLocal(n, p).y })),
      outs: outs.map((p) => ({ id: p.jackId, y: portLocal(n, p).y })),
    };
  });
  const edges: LayoutEdge[] = Object.values(graph.edges).map((e) => ({
    id: e.id,
    source: e.sourceNodeId,
    target: e.targetNodeId,
    fromJack: graph.ports[e.sourcePortId]?.jackId ?? "out",
    toJack: graph.ports[e.targetPortId]?.jackId ?? "in",
  }));
  return { nodes, edges };
}

export function applyPlaced(
  graph: BoardGraph,
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
  routes: Record<string, Array<{ x: number; y: number }>>,
): BoardGraph {
  const nodes = { ...graph.nodes };
  for (const [id, p] of Object.entries(placed)) {
    const n = nodes[id];
    if (! n) {
      continue;
    }
    nodes[id] = { ...n, x: snapToGrid(p.x), y: snapToGrid(p.y), w: p.w, h: p.h };
  }
  const edges = { ...graph.edges };
  for (const [id, route] of Object.entries(routes)) {
    const e = edges[id];
    if (e) {
      edges[id] = { ...e, route };
    }
  }
  return { ...graph, nodes, edges };
}

export function isLfoBoardNode(n: BoardNode): boolean {
  return isLfoNode({ type: n.type, id: n.id });
}
