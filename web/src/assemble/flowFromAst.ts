import { Position, type Edge, type Node } from "@xyflow/react";
import type { AstDocument, AstEdge, AstJack, AstNode } from "../bridge/ast";
import { kindLabel } from "../theme/tokens";
import { CHIP_GAP, chipBox } from "./chipLayout";
import { lettersInExpr } from "./bindLinks";
import { handleId, tokenInExpr } from "./handles";
import { canonicalIoJacks } from "./ioPaint";
import { visualAudioEdges, visualJacksFor } from "./visualEdges";

export { CHIP_GAP, CHIP_H, CHIP_W, IO_W, chipHeight, ioHeight } from "./chipLayout";

export type ChipData = {
  label: string;
  type: string;
  jacks: AstJack[];
  letters: string;
  focus?: "off" | "soft" | "sharp";
  args: Record<string, string>;
  channel: string;
  summary: string;
  nodeId: string;
};

function lettersOn(node: AstNode): string {
  const found: string[] = [];
  for (const [k, v] of Object.entries(node.args)) {
    for (const letter of lettersInExpr(v)) {
      found.push(`${letter} ${k}`);
    }
  }
  return found.join("  ");
}

export type SignalKind = "audio" | "mod";

export function isLfoNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  const t = (node.type || "").toLowerCase();
  const id = (node.id || "").toLowerCase();
  return t.startsWith("osc") || t === "lfo" || id.startsWith("osc") || id.startsWith("lfo");
}

export function isEnvNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  const t = (node.type || "").toLowerCase();
  const id = (node.id || "").toLowerCase();
  return t.startsWith("env") || id.startsWith("env");
}

export function isModulatorNode(node: Pick<AstNode, "type"> & { id?: string }): boolean {
  return isLfoNode(node) || isEnvNode(node);
}

export function inferModLinks(nodes: AstNode[]): AstEdge[] {
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const mods = nodes.filter(isModulatorNode);
  const out: AstEdge[] = [];
  const seen = new Set<string>();
  const push = (e: AstEdge) => {
    const key = `${e.from}>${e.to}>${e.fromJack}>${e.toJack}`;
    if (seen.has(key)) {
      return;
    }
    seen.add(key);
    out.push(e);
  };

  for (const dest of nodes) {
    for (const j of dest.jacks ?? []) {
      if (j.output || j.kind !== "mod") {
        continue;
      }
      if (! byId.has(j.id)) {
        continue;
      }
      push({ from: j.id, to: dest.id, kind: "mod", fromJack: "mod", toJack: j.id });
    }
    for (const src of mods) {
      if (src.id === dest.id) {
        continue;
      }
      const hit = Object.values(dest.args).some((v) => tokenInExpr(v, src.id));
      if (! hit) {
        continue;
      }
      const destJack = dest.jacks?.find((j) => ! j.output && j.kind === "mod" && j.id === src.id)?.id
        ?? src.id;
      const srcJack = src.jacks?.find((j) => j.output && j.kind === "mod")?.id ?? "mod";
      push({ from: src.id, to: dest.id, kind: "mod", fromJack: srcJack, toJack: destJack });
    }
  }
  return out;
}

export function resolveHz(expr: string, knobs: Array<{ id: string; value: number; min: number; max: number }>): number {
  const t = expr.trim();
  const n = Number(t);
  if (Number.isFinite(n) && n > 0) {
    return n;
  }
  if (/^[a-f]$/i.test(t)) {
    const k = knobs.find((x) => x.id === t.toLowerCase());
    if (k) {
      const hz = k.min + k.value * (k.max - k.min);
      return hz > 0 ? hz : 1;
    }
  }
  return 1;
}

export function compactSummary(node: AstNode): string {
  const get = (key: string) => node.args[key] ?? "";
  if (node.type === "send") return "tap";
  if (node.type === "stage") return get("y");
  if (node.type.startsWith("filter")) {
    let t = get("type");
    const low = t.toLowerCase();
    if (low.startsWith("high")) t = "HP";
    else if (low.startsWith("low")) t = "LP";
    else if (low.startsWith("band")) t = "BP";
    return `${t} ${get("cutoff")}`.trim();
  }
  if (node.type.startsWith("osc")) return `${get("shape")} ${get("freq")}`.trim();
  if (node.type.startsWith("reverb")) return `sz ${get("size")}`.trim();
  if (node.type.startsWith("comp") || node.type.startsWith("gate")) return `th ${get("threshold")}`.trim();
  if (node.type === "ms") return node.args.encode || node.args.mode || "M/S";
  return "";
}

function channelOf(node: AstNode): string {
  const bus = (node.busName || "").toLowerCase();
  if (! bus || bus === "main" || bus === "mod") {
    return "";
  }
  return bus.toUpperCase();
}

function isSidechainType(type: string): boolean {
  const t = type.toLowerCase();
  return t === "sidechain" || t === "sc" || t === "scin";
}

export function visibleNodes(ast: AstDocument, _sidechainOn = false): AstNode[] {
  return ast.nodes.filter((n) => {
    if (n.type === "bus") {
      const name = (n.args.name || n.id || "").toLowerCase();
      return name !== "__park";
    }
    if (isSidechainType(n.type)) {
      return false;
    }
    return true;
  });
}

export type FlowOpts = {
  sidechainOn?: boolean;
};

export function flowFromAst(ast: AstDocument, opts: FlowOpts = {}): { nodes: Node<ChipData>[]; edges: Edge[] } {
  const sidechainOn = Boolean(opts.sidechainOn);
  const nodes: Node<ChipData>[] = [];
  let x = 16;
  const y = 16;
  const visible = visibleNodes(ast, sidechainOn);

  const inJacks = canonicalIoJacks("in");
  const inBox = chipBox("in", inJacks, false, {});
  nodes.push({
    id: "IN",
    type: "io",
    position: { x: 16, y: 112 },
    data: { label: "IN", type: "in", jacks: inJacks, letters: "", args: {}, channel: "", summary: "", nodeId: "IN" },
    sourcePosition: Position.Right,
    draggable: false,
    style: { width: inBox.w, height: inBox.h },
    width: inBox.w,
    height: inBox.h,
  });
  x += inBox.w + CHIP_GAP;

  for (const n of visible) {
    const px = n.x != null && Number.isFinite(n.x) ? n.x : x;
    const py = n.y != null && Number.isFinite(n.y) ? n.y : y;
    const jacks = visualJacksFor(n, visible).length
      ? visualJacksFor(n, visible)
      : (n.jacks ?? []);
    const box = chipBox(n.type === "out" ? "out" : n.type, jacks, false, n.args);
    const custom = n.id.toLowerCase().startsWith("custom") || n.type.toLowerCase() === "custom";
    nodes.push({
      id: n.id,
      type: n.type === "out" || isSidechainType(n.type) ? "io" : "chip",
      position: { x: px, y: py },
      data: {
        label: custom ? n.id : (isSidechainType(n.type) ? "Sidechain" : kindLabel(n.type)),
        type: isSidechainType(n.type) ? "sidechain" : n.type,
        jacks,
        letters: lettersOn(n),
        args: n.args,
        channel: channelOf(n),
        summary: compactSummary(n),
        nodeId: n.id,
      },
      style: { width: box.w, height: box.h },
      width: box.w,
      height: box.h,
      sourcePosition: Position.Right,
      targetPosition: Position.Left,
      draggable: isSidechainType(n.type) ? false : undefined,
    });
    x = px + box.w + CHIP_GAP;
  }

  if (! ast.nodes.some((n) => n.type === "out")) {
    const outJacks = canonicalIoJacks("out");
    const outBox = chipBox("out", outJacks, false, {});
    nodes.push({
      id: "OUT",
      type: "io",
      position: { x, y: 112 },
      data: {
        label: "OUT",
        type: "out",
        jacks: outJacks,
        letters: "",
        args: {},
        channel: "MAIN",
        summary: "",
        nodeId: "OUT",
      },
      sourcePosition: Position.Right,
      targetPosition: Position.Left,
      style: { width: outBox.w, height: outBox.h },
      width: outBox.w,
      height: outBox.h,
    });
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
  for (const n of visible) {
    const src = (n.args.source || n.args.sidechain || "").toLowerCase();
    const vocoder = n.type.toLowerCase().startsWith("vocod");
    if (src === "sidechain" || src === "sc" || vocoder) {
      const key = `IN>${n.id}>sc>in`;
      if (! seen.has(key)) {
        seen.add(key);
        merged.push({ from: "IN", to: n.id, kind: "audio", fromJack: "sc", toJack: "in" });
      }
    }
  }

  const byId = new Map(visible.map((n) => [n.id, n]));
  const jacksOf = (id: string, output: boolean): AstJack[] => {
    if (id === "IN") {
      return canonicalIoJacks("in").filter((j) => j.output === output);
    }
    if (id === "OUT" || id === "out") {
      const outNode = byId.get("out") ?? byId.get("OUT") ?? {
        id: "OUT",
        type: "out",
        busName: "",
        args: {},
        trailingComment: "",
      };
      return visualJacksFor(outNode, visible).filter((j) => j.output === output && j.kind !== "knob");
    }
    const n = byId.get(id);
    if (! n) {
      return [];
    }
    const drawn = visualJacksFor(n, visible);
    return drawn.filter((j) => j.output === output && j.kind !== "knob");
  };
  const edges: Edge[] = merged.filter((e) => {
    const fromJack = e.fromJack || (e.kind === "mod" ? "mod" : "out");
    const toJack = e.toJack || (e.kind === "mod" ? e.from : "in");
    return jacksOf(e.from, true).some((j) => j.id === fromJack)
      && jacksOf(e.to, false).some((j) => j.id === toJack);
  }).map((e, i) => {
    const kind = e.kind === "mod" ? "mod" : "audio";
    const src = byId.get(e.from);
    const freqExpr = src && isLfoNode(src) ? (src.args.freq ?? "1") : "";
    const syncExpr = src && isLfoNode(src) ? (src.args.sync ?? "off") : "off";
    return {
      id: `e-${e.from}-${e.to}-${e.fromJack}-${e.toJack}-${i}`,
      source: e.from,
      target: e.to,
      sourceHandle: handleId(e.fromJack || (kind === "mod" ? "mod" : "out"), true),
      targetHandle: handleId(e.toJack || (kind === "mod" ? e.from : "in"), false),
      type: "signal",
      animated: false,
      style: {
        stroke: kind === "mod" ? "var(--nk-cyan)" : "var(--nk-accent)",
        strokeWidth: kind === "mod" ? 1 : 1.15,
      },
      data: {
        kind,
        freqExpr,
        syncExpr,
        depthExpr: src && isLfoNode(src) ? (src.args.depth ?? "1") : "1",
        sourceType: src?.type ?? "",
        sourceId: e.from,
      },
    };
  });

  return { nodes, edges };
}
