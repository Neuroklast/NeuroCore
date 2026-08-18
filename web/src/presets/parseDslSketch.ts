import type { AstDocument, AstEdge, AstJack, AstNode, AstParam } from "../bridge/ast";
import { CHIP_GAP, CHIP_W, IO_W } from "../assemble/chipLayout";
import { visualAudioEdges, visualJacksFor } from "../assemble/visualEdges";
import type { KnobState } from "../store/hostStore";

export type SketchKnob = KnobState;

const LETTERS = ["a", "b", "c", "d", "e", "f"] as const;
const NOTE = /^(\d+)\s*\/\s*(\d+)\.?$/;

const TYPE_ALIAS: Record<string, string> = {
  hpf: "filter",
  hp: "filter",
  highpass: "filter",
  bpf: "filter",
  bp: "filter",
  bandpass: "filter",
  lpf: "filter",
  lp: "filter",
  lowpass: "filter",
  verb: "reverb",
  stereo: "widen",
  stereoize: "widen",
  stereoiser: "widen",
  midside: "ms",
  mid_side: "ms",
  splitms: "ms",
  joinms: "ms",
  splitlr: "split_lr",
  joinlr: "join_lr",
  ngate: "noisegate",
  noise_gate: "noisegate",
  noisegate: "noisegate",
  probe: "meter",
  sc: "sidechain",
  scin: "sidechain",
  limiter: "limit",
  convolve: "ir",
  custom: "custom",
};

function stripComment(line: string): { code: string; comment: string } {
  let cut = line.length;
  const sl = line.indexOf("//");
  const hash = line.indexOf("#");
  if (sl >= 0) cut = Math.min(cut, sl);
  if (hash >= 0) cut = Math.min(cut, hash);
  return { code: line.slice(0, cut).trim(), comment: line.slice(cut).replace(/^[#/]+\s*/, "").trim() };
}

function lettersOf(id: string): string {
  return id.toLowerCase().replace(/[^a-z]/g, "");
}

function typeOf(id: string): string {
  const letters = lettersOf(id);
  return TYPE_ALIAS[letters] ?? letters;
}

function parseNumber(token: string): number | null {
  const n = Number(token);
  return Number.isFinite(n) ? n : null;
}

function parseNote(token: string): number | null {
  const m = token.trim().toLowerCase().match(NOTE);
  if (! m) {
    return null;
  }
  const den = Number(m[2]);
  if (! den) {
    return null;
  }
  let whole = Number(m[1]) / den;
  if (token.trim().endsWith(".")) {
    whole *= 1.5;
  }
  return whole;
}

function parseParam(line: string): AstParam | null {
  const eq = line.indexOf("=");
  if (eq < 0) {
    return null;
  }
  const alias = line.slice(5, eq).trim().toLowerCase();
  if (! /^[a-h]$/.test(alias)) {
    return null;
  }
  let rest = line.slice(eq + 1).trim();
  let min = 0;
  let max = 1;
  let isNote = false;
  const open = rest.indexOf("[");
  const close = rest.lastIndexOf("]");
  let name = rest;
  if (open >= 0 && close > open) {
    name = rest.slice(0, open).trim();
    const inner = rest.slice(open + 1, close);
    const comma = inner.indexOf(",");
    if (comma > 0) {
      const left = inner.slice(0, comma).trim();
      const right = inner.slice(comma + 1).trim();
      const n0 = parseNote(left);
      const n1 = parseNote(right);
      if (n0 != null && n1 != null) {
        isNote = true;
        min = n0;
        max = n1;
      } else {
        min = parseNumber(left) ?? 0;
        max = parseNumber(right) ?? 1;
        if (max < min) {
          const t = min;
          min = max;
          max = t;
        }
      }
    }
  }
  if (! name) {
    return null;
  }
  return { alias, name, min, max, isNote, noteWholes: [], noteLabels: [] };
}

function parseArgs(rest: string): Record<string, string> {
  const args: Record<string, string> = {};
  for (const part of rest.split(";")) {
    const eq = part.indexOf("=");
    if (eq <= 0) {
      continue;
    }
    const key = part.slice(0, eq).trim().toLowerCase();
    const value = part.slice(eq + 1).trim();
    if (key && value) {
      args[key] = value;
    }
  }
  return args;
}

function isMod(type: string): boolean {
  return type.startsWith("osc") || type.startsWith("env");
}

function jack(id: string, output: boolean, kind: string): AstJack {
  return { id, label: id, output, kind };
}

function jacksFor(node: AstNode, nodes: AstNode[]): AstJack[] {
  if (node.type === "bus") {
    return [jack("in", false, "send"), jack("out", true, "audio")];
  }
  if (node.type === "join") {
    return visualJacksFor({ ...node, jacks: [] }, nodes);
  }
  if (node.type === "out") {
    const ins = Object.keys(node.args).map((k) => jack(k, false, "audio"));
    return [...ins, jack("out", true, "audio")];
  }
  if (isMod(node.type)) {
    const jacks: AstJack[] = [];
    if (node.type.startsWith("env")) {
      jacks.push(jack("sc", false, "sc"));
    }
    jacks.push(jack("mod", true, "mod"));
    return jacks;
  }
  const special = visualJacksFor({ ...node, jacks: [] }, nodes);
  if (special.length > 0 && (
    node.type === "ms"
    || node.type === "send"
    || node.type === "msplit"
    || node.type.startsWith("xover")
    || node.type.startsWith("crossover")
    || node.type.startsWith("split_")
    || node.type.startsWith("join_")
  )) {
    return special;
  }
  const jacks: AstJack[] = [jack("in", false, node.type === "send" ? "send" : "audio")];
  if (node.type === "custom" || node.id.toLowerCase().startsWith("custom")) {
    for (const key of Object.keys(node.args)) {
      const k = key.toLowerCase();
      if (k === "y" || k === "channel" || k === "type") {
        continue;
      }
      if (k.startsWith("in")) {
        jacks.push(jack(key, false, "audio"));
      }
    }
  }
  for (const other of nodes) {
    if (! isMod(other.type) || other.id === node.id) {
      continue;
    }
    if (Object.values(node.args).some((v) => new RegExp(`(^|[^a-z0-9_])${other.id}([^a-z0-9_]|$)`, "i").test(v))) {
      jacks.push(jack(other.id, false, "mod"));
    }
  }
  jacks.push(jack("out", true, "audio"));
  return jacks;
}

function commentDefaults(script: string): Map<string, number> {
  const out = new Map<string, number>();
  const re = /#\s*([a-f])\s+\S+:\s*([-+0-9.]+)\s+to\s+([-+0-9.]+),\s*default\s+([-+0-9.]+)/gi;
  let m: RegExpExecArray | null;
  while ((m = re.exec(script)) != null) {
    const v = Number(m[4]);
    if (Number.isFinite(v)) {
      out.set(m[1].toLowerCase(), v);
    }
  }
  return out;
}

function clamp01(v: number): number {
  return Math.max(0, Math.min(1, v));
}

function norm01(value: number, min: number, max: number): number {
  const denom = max - min;
  if (! Number.isFinite(denom) || Math.abs(denom) < 1e-9) {
    return 0;
  }
  return clamp01((value - min) / denom);
}

export function emptyKnobs(): SketchKnob[] {
  return LETTERS.map((id) => ({
    id,
    name: "",
    value: 0,
    active: false,
    min: 0,
    max: 1,
    isNote: false,
  }));
}

export function knobsFromSketch(script: string, extras?: Array<{
  id: string;
  name: string;
  min: number;
  max: number;
  default: number;
}>): SketchKnob[] {
  const { doc } = parseDslSketch(script);
  const defaults = commentDefaults(script);
  const extraById = new Map((extras ?? []).map((p) => [p.id, p]));
  return LETTERS.map((id) => {
    const extra = extraById.get(id);
    const param = doc.params.find((p) => p.alias === id);
    if (extra) {
      return {
        id,
        name: extra.name || param?.name || id,
        value: norm01(extra.default, extra.min, extra.max),
        active: true,
        min: extra.min,
        max: extra.max,
        isNote: param?.isNote ?? false,
      };
    }
    if (! param) {
      return { id, name: "", value: 0, active: false, min: 0, max: 1, isNote: false };
    }
    const raw = defaults.get(id);
    const mid = param.min + (param.max - param.min) * 0.5;
    const mapped = raw ?? mid;
    return {
      id,
      name: param.name,
      value: param.isNote && raw != null && raw >= 0 && raw <= 1 ? raw : norm01(mapped, param.min, param.max),
      active: true,
      min: param.min,
      max: param.max,
      isNote: param.isNote,
    };
  });
}

export function parseDslSketch(script: string): { doc: AstDocument } {
  const leadingComments: string[] = [];
  const params: AstParam[] = [];
  const nodes: AstNode[] = [];
  let currentBus = "main";
  let sawBlock = false;

  for (const raw of script.split(/\r?\n/)) {
    const trimmed = raw.trim();
    if (! trimmed) {
      continue;
    }
    if (! sawBlock && (trimmed.startsWith("#") || trimmed.startsWith("//"))) {
      leadingComments.push(trimmed.replace(/^[#/]+\s*/, ""));
      continue;
    }
    const { code, comment } = stripComment(trimmed);
    if (! code) {
      continue;
    }
    if (/^param\b/i.test(code)) {
      const p = parseParam(code);
      if (p) {
        params.push(p);
      }
      continue;
    }
    sawBlock = true;
    const colon = code.indexOf(":");
    if (colon < 0) {
      const last = nodes[nodes.length - 1];
      if (last && /^\s+\S/.test(raw) && code.includes("=")) {
        Object.assign(last.args, parseArgs(code));
      }
      continue;
    }
    const head = code.slice(0, colon).trim().toLowerCase();
    let rest = code.slice(colon + 1).trim();
    const brace = rest.indexOf("{");
    if (brace >= 0) {
      rest = rest.slice(0, brace).trim();
    }
    const tokens = head.split(/\s+/).filter(Boolean);
    if (tokens[0] === "bus" && tokens[1]) {
      currentBus = tokens[1];
      nodes.push({
        id: tokens[1],
        type: "bus",
        busName: "",
        args: { name: tokens[1] },
        trailingComment: comment,
      });
      continue;
    }
    if (tokens[0] === "send") {
      nodes.push({
        id: `send_${currentBus}`,
        type: "send",
        busName: currentBus,
        args: parseArgs(rest),
        trailingComment: comment,
      });
      continue;
    }
    if (tokens[0] === "out") {
      nodes.push({
        id: "out",
        type: "out",
        busName: "",
        args: parseArgs(rest),
        trailingComment: comment,
      });
      continue;
    }
    const id = tokens[0] ?? "";
    if (! id) {
      continue;
    }
    const type = typeOf(id);
    if (type === "split") {
      const args = parseArgs(rest);
      nodes.push({
        id,
        type: "ms",
        busName: currentBus,
        args,
        trailingComment: comment,
      });
      continue;
    }
    const args = parseArgs(rest);
    if ((type === "filter" || TYPE_ALIAS[lettersOf(id)] === "filter") && ! args.type) {
      const letters = lettersOf(id);
      if (letters.startsWith("hp")) args.type = "highpass";
      else if (letters.startsWith("bp")) args.type = "bandpass";
      else if (letters.startsWith("lp")) args.type = "lowpass";
    }
    if (type === "join" && ! args.mix) {
      args.mix = "0.5";
    }
    const onBus = type === "join" ? "main" : (isMod(type) ? "mod" : currentBus);
    if (type === "join") {
      currentBus = "main";
    }
    nodes.push({
      id,
      type,
      busName: onBus,
      args,
      trailingComment: comment,
    });
  }

  for (const n of nodes) {
    n.jacks = jacksFor(n, nodes);
  }

  const edges: AstEdge[] = [];
  const push = (from: string, to: string, kind: string, fromJack: string, toJack: string) => {
    edges.push({ from, to, kind, fromJack, toJack });
  };

  const audio = nodes.filter((n) => n.type !== "bus" && n.type !== "out" && n.type !== "join" && ! isMod(n.type));
  const out = nodes.find((n) => n.type === "out");
  const buses = [...new Set(audio.map((n) => n.busName || "main"))];
  for (const bus of buses) {
    const chain = audio.filter((n) => (n.busName || "main") === bus);
    if (chain.length === 0) {
      continue;
    }
    const first = chain[0]!;
    if (bus === "main") {
      push("IN", first.id, "audio", "out", "in");
    } else if (first.type !== "send") {
      push("IN", first.id, "audio", "out", "in");
    }
    for (let i = 0; i < chain.length - 1; i += 1) {
      push(chain[i]!.id, chain[i + 1]!.id, "audio", "out", "in");
    }
    const last = chain[chain.length - 1]!;
    if (out) {
      const destJack = Object.prototype.hasOwnProperty.call(out.args, bus) ? bus : "in";
      push(last.id, out.id, "audio", "out", destJack);
    } else if (bus === "main") {
      push(last.id, "OUT", "audio", "out", "in");
    }
  }
  const rewritten = visualAudioEdges(nodes, edges);
  edges.length = 0;
  edges.push(...rewritten);

  let modX = 16;
  const busRow = new Map<string, number>();
  const busX = new Map<string, number>();
  let busIndex = 0;
  for (const n of nodes) {
    if (isMod(n.type)) {
      n.x = modX;
      n.y = 280;
      modX += CHIP_W + CHIP_GAP;
      continue;
    }
    const bus = n.busName || "main";
    if (! busRow.has(bus)) {
      busRow.set(bus, bus === "main" ? 112 : 112 + 180 * (++busIndex));
      busX.set(bus, 16 + IO_W + CHIP_GAP);
    }
    n.x = busX.get(bus);
    n.y = busRow.get(bus);
    busX.set(bus, (n.x ?? 0) + CHIP_W + CHIP_GAP);
  }

  return {
    doc: {
      version: 1,
      leadingComments,
      params,
      nodes,
      edges,
      inJacks: [jack("out", true, "audio")],
    },
  };
}
