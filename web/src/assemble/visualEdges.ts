import type { AstEdge, AstJack, AstNode } from "../bridge/ast";

export function isMsEncode(node: Pick<AstNode, "type" | "args">): boolean {
  if (node.type !== "ms") {
    return false;
  }
  const m = (node.args.mode ?? "").trim().toLowerCase();
  return m !== "decode" && m !== "lr" && m !== "stereo" && m !== "to_lr";
}

export function isMsDecode(node: Pick<AstNode, "type" | "args">): boolean {
  return node.type === "ms" && ! isMsEncode(node);
}

export function channelRail(node: Pick<AstNode, "args">): string {
  const c = (node.args.channel ?? "").trim().toLowerCase();
  if (c === "mid" || c === "m") return "mid";
  if (c === "side" || c === "s") return "side";
  if (c === "left" || c === "l") return "left";
  if (c === "right" || c === "r") return "right";
  return "";
}

export function isXover(node: Pick<AstNode, "type">): boolean {
  const t = node.type.toLowerCase();
  return t.startsWith("xover") || t.startsWith("crossover");
}

function remapMsSerial(edge: AstEdge, byId: Map<string, AstNode>): AstEdge {
  if (edge.kind === "mod") {
    return edge;
  }
  const src = byId.get(edge.from);
  const dst = byId.get(edge.to);
  let fromJack = edge.fromJack;
  let toJack = edge.toJack;
  if (src && isMsEncode(src) && (fromJack === "out" || ! fromJack)) {
    fromJack = channelRail(dst ?? { args: {} }) === "side" ? "side" : "mid";
    if (dst && isMsDecode(dst) && (toJack === "in" || ! toJack)) {
      toJack = fromJack;
    }
  }
  if (dst && isMsDecode(dst) && (toJack === "in" || ! toJack)) {
    toJack = channelRail(src ?? { args: {} }) === "side" ? "side" : "mid";
  }
  if (fromJack === edge.fromJack && toJack === edge.toJack) {
    return edge;
  }
  return { ...edge, fromJack, toJack };
}

function busOf(node: Pick<AstNode, "busName">): string {
  return node.busName || "main";
}

function isMsKid(node: AstNode): boolean {
  if (node.type === "out" || node.type === "bus" || isMsEncode(node) || isMsDecode(node)) {
    return false;
  }
  const t = node.type.toLowerCase();
  if (t.startsWith("osc") || t.startsWith("env")) {
    return false;
  }
  const ch = channelRail(node);
  return ch === "" || ch === "mid" || ch === "side";
}

function jack(id: string, output: boolean, kind: string): AstJack {
  return { id, label: id, output, kind };
}

/** Jacks the circuit actually draws — encode has mid/side, not a fake out. */
export function visualJacksFor(node: AstNode, _nodes: AstNode[] = []): AstJack[] {
  if (node.type === "ms") {
    return isMsEncode(node)
      ? [jack("in", false, "audio"), jack("mid", true, "audio"), jack("side", true, "audio")]
      : [jack("mid", false, "audio"), jack("side", false, "audio"), jack("out", true, "audio")];
  }
  if (isXover(node)) {
    const outs = [jack("in", false, "audio"), jack("low", true, "mix")];
    if (node.args.f2) {
      outs.push(jack("mid", true, "mix"));
    }
    outs.push(jack("high", true, "mix"));
    return outs;
  }
  return node.jacks ?? [];
}

function keyOf(e: AstEdge): string {
  return `${e.from}>${e.to}>${e.fromJack}>${e.toJack}`;
}

/**
 * Serial out/in edges hide MS forks and xover mixes. Rewrite them to the
 * cables the board should draw. Idempotent if the edges are already visual.
 * Untagged chips between encode/decode sit on both rails (DSP: L=mid, R=side).
 */
export function visualAudioEdges(nodes: AstNode[], edges: AstEdge[]): AstEdge[] {
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const outId = nodes.find((n) => n.type === "out")?.id ?? "OUT";
  let next = edges.filter((e) => e.kind !== "mod").map((e) => ({ ...e }));
  const mods = edges.filter((e) => e.kind === "mod");

  const strip = (from: string, to: string) => {
    next = next.filter((e) => ! (e.from === from && e.to === to && e.kind === "audio"));
  };
  const push = (e: AstEdge) => {
    const k = keyOf(e);
    if (next.some((x) => keyOf(x) === k)) {
      return;
    }
    next.push(e);
  };

  const emitChain = (enc: string, srcJack: string, kids: AstNode[], dec: string, dstJack: string) => {
    if (kids.length === 0) {
      push({ from: enc, to: dec, kind: "audio", fromJack: srcJack, toJack: dstJack });
      return;
    }
    push({ from: enc, to: kids[0]!.id, kind: "audio", fromJack: srcJack, toJack: "in" });
    for (let k = 1; k < kids.length; k += 1) {
      push({ from: kids[k - 1]!.id, to: kids[k]!.id, kind: "audio", fromJack: "out", toJack: "in" });
    }
    push({ from: kids[kids.length - 1]!.id, to: dec, kind: "audio", fromJack: "out", toJack: dstJack });
  };

  for (let i = 0; i < nodes.length; i += 1) {
    const enc = nodes[i]!;
    if (! isMsEncode(enc)) {
      continue;
    }
    const bus = busOf(enc);
    const dec = nodes.slice(i + 1).find((n) => isMsDecode(n) && busOf(n) === bus);
    if (! dec) {
      continue;
    }
    const between = nodes.slice(i + 1, nodes.indexOf(dec)).filter((n) => isMsKid(n) && busOf(n) === bus);
    const mid = between.filter((n) => channelRail(n) !== "side");
    const side = between.filter((n) => channelRail(n) !== "mid");
    if (between[0]) strip(enc.id, between[0].id);
    for (let k = 1; k < between.length; k += 1) {
      strip(between[k - 1]!.id, between[k]!.id);
    }
    if (between[between.length - 1]) strip(between[between.length - 1]!.id, dec.id);
    strip(enc.id, dec.id);

    emitChain(enc.id, "mid", mid, dec.id, "mid");
    emitChain(enc.id, "side", side, dec.id, "side");
  }

  next = next.map((e) => remapMsSerial(e, byId));

  for (const n of nodes) {
    if (! isXover(n)) {
      continue;
    }
    const jacks = visualJacksFor(n, nodes).filter((j) => j.output);
    for (const j of jacks) {
      push({ from: n.id, to: outId, kind: "mix", fromJack: j.id, toJack: j.id });
    }
  }

  return [...next, ...mods];
}
