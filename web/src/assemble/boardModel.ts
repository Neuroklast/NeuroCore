import { placeHostTerminals } from "./hostTerminalGeometry";
import type { AstDocument, AstEdge, AstJack, AstNode } from "../bridge/ast";
import { kindLabel } from "../theme/tokens";
import { CHIP_GAP, chipBox } from "./chipLayout";
import { BOARD_GRID, snapToGrid, snapSize } from "./grid";
import { canonicalIoJacks } from "./ioPaint";
import { globalPort, northPortLocals, sidePortLocals, sidePortMinHeight, southPortLocals } from "./portLayout";
import { cableFace } from "./validateLink";
import { visualAudioEdges, visualJacksFor } from "./visualEdges";
import { inferModLinks, isLfoNode, visibleNodes } from "./flowFromAst";
import type { LayoutEdge, LayoutNode } from "./layout/types";

export type PortKind = "audio" | "mod" | "sc";
export type PortFace = "n" | "e" | "s" | "w";

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
  busName?: string;
};

export type BoardPort = {
  id: string;
  nodeId: string;
  jackId: string;
  face: PortFace;
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
  /** Display-only dependency on a host MIDI variable, not an editable event cable. */
  dependency?: "midi";
};

export type BoardConnectionIssue = {
  code: "missing-port";
  edge: AstEdge;
  missingPortIds: string[];
};

export type BoardGraph = {
  connectionIssues?: BoardConnectionIssue[];
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

/** Native graphOp node/jack from the semantic port id, not the visual owner. */
export function graphOpEndpoints(port: Pick<BoardPort, "id" | "jackId">): { nodeId: string; jackId: string } {
  const parts = port.id.split("::");
  return { nodeId: parts[0] ?? "", jackId: parts[1] ?? port.jackId };
}

export function portIsOutput(port: Pick<BoardPort, "id">): boolean {
  return port.id.split("::")[2] === "src";
}

export function portKindOf(jack: AstJack): PortKind {
  if (jack.id === "sc" || jack.kind === "mod" && jack.id === "sc") {
    return "sc";
  }
  if (jack.kind === "mod" || jack.kind === "param" || jack.kind === "ctrl") {
    return "mod";
  }
  return jack.id === "sc" ? "sc" : "audio";
}

export function portLocal(node: Pick<BoardNode, "w" | "h"> & Partial<Pick<BoardNode, "role">>, port: BoardPort): { x: number; y: number } {
  if (port.face === "s") {
    const locals = southPortLocals(port.count, { w: node.w, h: node.h });
    return locals[port.index] ?? { x: node.w * 0.5, y: node.h };
  }
  if (port.face === "n") {
    const locals = northPortLocals(port.count, { w: node.w, h: node.h });
    return locals[port.index] ?? { x: node.w * 0.5, y: 0 };
  }
  const locals = sidePortLocals(port.count, { w: node.w, h: node.h }, port.face === "e", node.role);
  return locals[port.index] ?? { x: port.face === "e" ? node.w : 0, y: node.h * 0.5 };
}

export function portGlobal(node: BoardNode, port: BoardPort): { x: number; y: number } {
  return globalPort(node, portLocal(node, port));
}

function channelOf(node: AstNode): string {
  const bus = (node.busName || "").toLowerCase();
  if (! bus || bus === "main" || bus === "mod") {
    return "";
  }
  return bus.toUpperCase();
}

function addSidePorts(
  ports: Record<string, BoardPort>,
  nodeId: string,
  jacks: AstJack[],
  east: boolean,
): void {
  const side = jacks.filter((j) => j.output === east && j.kind !== "knob" && cableFace(j.kind) === "side");
  side.forEach((j, index) => {
    const id = portId(nodeId, j.id, east);
    ports[id] = {
      id,
      nodeId,
      jackId: j.id,
      face: east ? "e" : "w",
      index,
      count: side.length,
      kind: j.id === "sc" ? "sc" : portKindOf(j),
    };
  });
}

function addSouthPorts(
  ports: Record<string, BoardPort>,
  nodeId: string,
  jacks: AstJack[],
): void {
  const south = jacks.filter((j) => j.kind !== "knob" && cableFace(j.kind) === "bottom");
  south.forEach((j, index) => {
    const id = portId(nodeId, j.id, false);
    if (ports[id] && ports[id]!.face !== "s") {
      return;
    }
    ports[id] = {
      id,
      nodeId,
      jackId: j.id,
      face: "s",
      index,
      count: south.length,
      kind: j.id === "sc" ? "sc" : portKindOf(j),
    };
  });
}

const MIDI_VARS = ["midi_note", "midi_freq", "midi_vel", "midi_gate", "midi_bend", "midi_mod"] as const;

function midiJack(name: string): string {
  return name.replace(/^midi_/, "");
}

function midiRefs(args: Record<string, string>): string[] {
  const blob = Object.values(args).join(" ");
  return MIDI_VARS.filter((name) => new RegExp(`\\b${name}\\b`).test(blob));
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
  const box = chipBox(role === "io" && type === "sidechain" ? "in" : type, jacks, false, args);
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
  const ids: string[] = ["IN", "MIDI", "SC"];
  for (const n of vis) {
    if (n.id !== "IN" && n.type !== "in" && n.id !== "MIDI" && n.id !== "SC") {
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

  const inJacks = canonicalIoJacks("in").filter((j) => j.id !== "sc");
  nodes.IN = makeNode("IN", "in", "io", x, rowY, inJacks, {}, "AUDIO IN", "", true);
  addSidePorts(ports, "IN", inJacks, true);

  const usedMidi = [...new Set(visible.flatMap((n) => midiRefs(n.args)))];
  const midiNames = usedMidi.length > 0 ? usedMidi : ["midi_gate"];
  const midiJacks: AstJack[] = midiNames.map((name) => ({
    id: midiJack(name),
    label: midiJack(name),
    output: true,
    kind: "mod",
  }));
  nodes.MIDI = makeNode(
    "MIDI",
    "midi",
    "io",
    x,
    rowY + nodes.IN.h + CHIP_GAP,
    midiJacks,
    {},
    "MIDI IN",
    "",
    true,
  );
  midiNames.forEach((name, index) => {
    const id = portId("MIDI", midiJack(name), true);
    ports[id] = {
      id,
      nodeId: "MIDI",
      jackId: midiJack(name),
      face: "e",
      index,
      count: midiNames.length,
      kind: "mod",
    };
  });

  const scJacks = canonicalIoJacks("sidechain");
  nodes.SC = makeNode(
    "SC",
    "sidechain",
    "io",
    x,
    nodes.MIDI.y + nodes.MIDI.h + CHIP_GAP,
    scJacks,
    {},
    "SIDECHAIN IN",
    "",
    true,
  );
  ports[portId("IN", "sc", true)] = {
    id: portId("IN", "sc", true),
    nodeId: "SC",
    jackId: "sc",
    face: "e",
    index: 0,
    count: 1,
    kind: "sc",
  };
  x += nodes.IN.w + CHIP_GAP;

  const midiDependencies: AstEdge[] = visible.flatMap((node) =>
    Object.entries(node.args).flatMap(([key, expression]) =>
      midiRefs({ [key]: expression }).map((name) => ({
        from: "MIDI", to: node.id, kind: "mod", fromJack: midiJack(name), toJack: key,
      }))),
  );
  const sidechainDependencies: AstEdge[] = visible
    .filter((node) => node.type === "sidechain" || node.type === "sc" || node.type === "scin"
      || ["comp", "noisegate", "gate", "env"].includes(node.type)
        && ["sidechain", "sc", "ext"].includes((node.args.source || node.args.input || node.args.sidechain || "").toLowerCase()))
    .map((node) => ({ from: "IN", to: node.id, kind: "audio", fromJack: "sc", toJack: "sc" }));
  const inferred = [...inferModLinks(visible), ...midiDependencies, ...sidechainDependencies];
  const jackEdges = [...(ast.edges ?? []), ...inferred];

  for (const n of visible) {
    if (n.type === "in" || n.id === "IN" || n.id === "MIDI" || n.id === "SC") {
      continue;
    }
    const visual = visualJacksFor(n, visible, jackEdges);
    const jacks = visual.length ? visual : (n.jacks ?? []);
    const custom = n.id.toLowerCase().startsWith("custom") || n.type.toLowerCase() === "custom";
    const role: "io" | "chip" = n.type === "out" ? "io" : "chip";
    const px = x;
    const py = rowY;
    nodes[n.id] = makeNode(
      n.id,
      n.type,
      role,
      px,
      py,
      jacks,
      n.args,
      custom ? n.id : (n.type === "out" ? "AUDIO OUT" : kindLabel(n.type)),
      channelOf(n),
      n.type === "out",
    );
    nodes[n.id]!.busName = n.busName;
    addSidePorts(ports, n.id, jacks, false);
    addSidePorts(ports, n.id, jacks, true);
    addSouthPorts(ports, n.id, jacks);
    x = px + nodes[n.id]!.w + CHIP_GAP;
  }

  if (! ast.nodes.some((n) => n.type === "out")) {
    const outJacks = canonicalIoJacks("out");
    nodes.OUT = makeNode("OUT", "out", "io", x, rowY, outJacks, {}, "AUDIO OUT", "MAIN", true);
    addSidePorts(ports, "OUT", outJacks, false);
    addSouthPorts(ports, "OUT", outJacks);
  }

  const explicit = visualAudioEdges(visible, ast.edges ?? []);
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
  const connectionIssues: BoardConnectionIssue[] = [];
  merged.forEach((e, i) => {
    const fromJack = e.fromJack || (e.kind === "mod" ? "mod" : "out");
    const toJack = e.toJack || (e.kind === "mod" ? e.from : "in");
    const sourcePortId = portId(e.from, fromJack, true);
    const targetPortId = portId(e.to, toJack, false);
    const missingPortIds = [sourcePortId, targetPortId].filter((id) => !ports[id]);
    if (missingPortIds.length) {
      connectionIssues.push({ code: "missing-port", edge: { ...e }, missingPortIds });
      return;
    }
    const kind: PortKind = e.kind === "mod" ? "mod" : (fromJack === "sc" ? "sc" : "audio");
    const id = `e-${e.from}-${e.to}-${fromJack}-${toJack}-${i}`;
    edges[id] = {
      id,
      sourceNodeId: ports[sourcePortId]!.nodeId,
      sourcePortId,
      targetNodeId: ports[targetPortId]!.nodeId,
      targetPortId,
      kind,
      ...(e.from === "MIDI" ? { dependency: "midi" as const } : {}),
      route: [],
    };
  });

  // Semantic jack IDs stay intact for DSP operations. Every visible connection
  // owns a physical socket, so fan-out/fan-in never starts on stacked rails.
  const useCount = new Map<string, number>();
  for (const edge of Object.values(edges)) {
    for (const field of ["sourcePortId", "targetPortId"] as const) {
      const base = edge[field];
      const count = useCount.get(base) ?? 0;
      useCount.set(base, count + 1);
      if (count > 0) {
        const id = `${base}::branch${count}`;
        ports[id] = { ...ports[base]!, id };
        edge[field] = id;
      }
    }
  }
  recountRails(nodes, ports);
  return { nodes, ports, edges, connectionIssues };
}

/** Occupancy is counted per face after every port exists. */
function recountRails(
  nodes: Record<string, BoardNode>,
  ports: Record<string, BoardPort>,
): void {
  const faces: PortFace[] = ["n", "e", "s", "w"];
  for (const node of Object.values(nodes)) {
    for (const face of faces) {
      const side = Object.values(ports).filter((p) => p.nodeId === node.id && p.face === face);
      side.sort((a, b) => a.index - b.index || a.id.localeCompare(b.id));
      side.forEach((p, index) => {
        p.index = index;
        p.count = side.length;
      });
      if (face === "e" || face === "w") {
        node.h = Math.max(node.h, snapSize(sidePortMinHeight(side.length) + BOARD_GRID));
      }
    }
  }
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
    const owned = Object.values(graph.ports).filter((p) => p.nodeId === n.id);
    const ins = owned.filter((p) => ! portIsOutput(p));
    const outs = owned.filter((p) => portIsOutput(p));
    const asLayout = (p: BoardPort) => {
      const loc = portLocal(n, p);
      return { id: p.id, x: loc.x, y: loc.y, face: p.face, index: p.index, count: p.count };
    };
    return {
      id: n.id,
      x: n.x,
      y: n.y,
      w: n.w,
      h: n.h,
      pin: n.id === "MIDI" || n.id === "SC",
      rail: isLfoNode(n) || n.type === "env" ? "mod" : n.busName || "main",
      ins: ins.map(asLayout),
      outs: outs.map(asLayout),
    };
  });
  const edges: LayoutEdge[] = Object.values(graph.edges).map((e) => ({
    id: e.id,
    source: e.sourceNodeId,
    target: e.targetNodeId,
    fromJack: e.sourcePortId,
    toJack: e.targetPortId,
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
  placeHostTerminals(nodes);
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
