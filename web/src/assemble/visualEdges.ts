import type { AstEdge, AstJack, AstNode } from "../bridge/ast";

function argOf(node: Pick<AstNode, "args">, key: string): string {
  return (node.args[key] ?? "").trim().toLowerCase();
}

export function msMode(node: Pick<AstNode, "args">): string {
  return argOf(node, "mode") || argOf(node, "encode");
}

export function isJoinMode(mode: string): boolean {
  const m = mode.trim().toLowerCase();
  return m === "decode" || m === "join" || m === "lr" || m === "stereo" || m === "to_lr"
    || m === "join_lr" || m === "lr_join";
}

/** MS (mid/side) vs LR (left/right). Empty if the chip is not a split/join. */
export function splitFamily(node: Pick<AstNode, "type" | "args">): "ms" | "lr" | "" {
  const t = node.type.toLowerCase();
  if (t === "split_lr" || t === "join_lr") {
    return "lr";
  }
  if (t === "split_ms" || t === "join_ms") {
    return "ms";
  }
  if (t !== "ms") {
    return "";
  }
  const fam = argOf(node, "family") || argOf(node, "rails");
  if (fam === "lr" || fam === "leftright" || fam === "l/r") {
    return "lr";
  }
  const m = msMode(node);
  if (m === "split_lr" || m === "join_lr" || m === "lr_split" || m === "lr_join") {
    return "lr";
  }
  return "ms";
}

export function forkRails(family: "ms" | "lr"): [string, string] {
  return family === "lr" ? ["left", "right"] : ["mid", "side"];
}

/** Encode stays an alias of split. */
export function isMsEncode(node: Pick<AstNode, "type" | "args">): boolean {
  const t = node.type.toLowerCase();
  if (t === "split_ms") {
    return true;
  }
  if (t === "join_ms" || t !== "ms") {
    return false;
  }
  return splitFamily(node) === "ms" && ! isJoinMode(msMode(node));
}

export function isMsDecode(node: Pick<AstNode, "type" | "args">): boolean {
  const t = node.type.toLowerCase();
  if (t === "join_ms") {
    return true;
  }
  if (t === "split_ms") {
    return false;
  }
  return t === "ms" && splitFamily(node) === "ms" && ! isMsEncode(node);
}

export function isLrSplit(node: Pick<AstNode, "type" | "args">): boolean {
  const t = node.type.toLowerCase();
  if (t === "split_lr") {
    return true;
  }
  if (t === "join_lr") {
    return false;
  }
  return splitFamily(node) === "lr" && ! isJoinMode(msMode(node));
}

export function isLrJoin(node: Pick<AstNode, "type" | "args">): boolean {
  const t = node.type.toLowerCase();
  if (t === "join_lr") {
    return true;
  }
  if (t === "split_lr") {
    return false;
  }
  return splitFamily(node) === "lr" && isJoinMode(msMode(node));
}

export function isForkSplit(node: Pick<AstNode, "type" | "args">): boolean {
  return isMsEncode(node) || isLrSplit(node);
}

export function isForkJoin(node: Pick<AstNode, "type" | "args">): boolean {
  return isMsDecode(node) || isLrJoin(node);
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
  return t.startsWith("xover") || t.startsWith("crossover") || t === "msplit";
}

export function isSend(node: Pick<AstNode, "type">): boolean {
  return node.type.toLowerCase() === "send";
}

function remapForkSerial(edge: AstEdge, byId: Map<string, AstNode>): AstEdge {
  if (edge.kind === "mod") {
    return edge;
  }
  const src = byId.get(edge.from);
  const dst = byId.get(edge.to);
  let fromJack = edge.fromJack;
  let toJack = edge.toJack;
  if (src && isForkSplit(src) && (fromJack === "out" || ! fromJack)) {
    const family = splitFamily(src) || "ms";
    const [a, b] = forkRails(family);
    fromJack = channelRail(dst ?? { args: {} }) === b ? b : a;
    if (dst && isForkJoin(dst) && splitFamily(dst) === family && (toJack === "in" || ! toJack)) {
      toJack = fromJack;
    }
  }
  if (dst && isForkJoin(dst) && (toJack === "in" || ! toJack)) {
    const family = splitFamily(dst) || "ms";
    const [a, b] = forkRails(family);
    toJack = channelRail(src ?? { args: {} }) === b ? b : a;
  }
  if (fromJack === edge.fromJack && toJack === edge.toJack) {
    return edge;
  }
  return { ...edge, fromJack, toJack };
}

function busOf(node: Pick<AstNode, "busName">): string {
  return node.busName || "main";
}

export function isJoinSignal(node: Pick<AstNode, "type">): boolean {
  return node.type.toLowerCase() === "join";
}

function isForkKid(node: AstNode, family: "ms" | "lr"): boolean {
  if (node.type === "out" || node.type === "bus" || isJoinSignal(node)
      || isForkSplit(node) || isForkJoin(node)) {
    return false;
  }
  const t = node.type.toLowerCase();
  if (t.startsWith("osc") || t.startsWith("env")) {
    return false;
  }
  const ch = channelRail(node);
  if (family === "lr") {
    return ch === "" || ch === "left" || ch === "right";
  }
  return ch === "" || ch === "mid" || ch === "side";
}

function jack(id: string, output: boolean, kind: string): AstJack {
  return { id, label: id, output, kind };
}

/** Jacks the circuit actually draws — split is 1 in / 2 out, join is 2 in / 1 out. */
export function visualJacksFor(node: AstNode, _nodes: AstNode[] = []): AstJack[] {
  if (isMsEncode(node) || node.type.toLowerCase() === "split_ms") {
    return [jack("in", false, "audio"), jack("mid", true, "audio"), jack("side", true, "audio")];
  }
  if (isMsDecode(node) || node.type.toLowerCase() === "join_ms") {
    return [jack("mid", false, "audio"), jack("side", false, "audio"), jack("out", true, "audio")];
  }
  if (isLrSplit(node)) {
    return [jack("in", false, "audio"), jack("left", true, "audio"), jack("right", true, "audio")];
  }
  if (isLrJoin(node)) {
    return [jack("left", false, "audio"), jack("right", false, "audio"), jack("out", true, "audio")];
  }
  if (isXover(node)) {
    return [
      jack("in", false, "audio"),
      jack("low", true, "mix"),
      jack("mid", true, "mix"),
      jack("high", true, "mix"),
    ];
  }
  if (isSend(node)) {
    return [
      jack("in", false, "send"),
      jack("out", true, "audio"),
      jack("ctrl", true, "ctrl"),
    ];
  }
  if (isJoinSignal(node)) {
    return [jack("inA", false, "audio"), jack("inB", false, "audio"), jack("out", true, "audio")];
  }
  return node.jacks ?? [];
}

function keyOf(e: AstEdge): string {
  return `${e.from}>${e.to}>${e.fromJack}>${e.toJack}`;
}

/**
 * Serial out/in edges hide MS forks and xover mixes. Rewrite them to the
 * cables the board should draw. Idempotent if the edges are already visual.
 * Untagged chips between a matching split/join sit on both rails
 * (MS: L=mid, R=side; L/R: left/right).
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
    if (! isForkSplit(enc)) {
      continue;
    }
    const family = splitFamily(enc);
    if (family !== "ms" && family !== "lr") {
      continue;
    }
    const [railA, railB] = forkRails(family);
    const bus = busOf(enc);
    const dec = nodes.slice(i + 1).find((n) => isForkJoin(n) && splitFamily(n) === family && busOf(n) === bus);
    if (! dec) {
      continue;
    }
    const between = nodes.slice(i + 1, nodes.indexOf(dec)).filter((n) => isForkKid(n, family) && busOf(n) === bus);
    const aKids = between.filter((n) => channelRail(n) !== railB);
    const bKids = between.filter((n) => channelRail(n) !== railA);
    if (between[0]) strip(enc.id, between[0].id);
    for (let k = 1; k < between.length; k += 1) {
      strip(between[k - 1]!.id, between[k]!.id);
    }
    if (between[between.length - 1]) strip(between[between.length - 1]!.id, dec.id);
    strip(enc.id, dec.id);

    emitChain(enc.id, railA, aKids, dec.id, railA);
    emitChain(enc.id, railB, bKids, dec.id, railB);
  }

  next = next.map((e) => remapForkSerial(e, byId));

  const isRailAudio = (n: AstNode) => {
    if (n.type === "out" || n.type === "bus" || isJoinSignal(n) || isForkSplit(n) || isForkJoin(n)) {
      return false;
    }
    const t = n.type.toLowerCase();
    return ! t.startsWith("osc") && ! t.startsWith("env");
  };

  for (const jn of nodes.filter(isJoinSignal)) {
    const idx = nodes.indexOf(jn);
    const before = nodes.slice(0, idx);
    const mainSrc = [...before].reverse().find((n) => isRailAudio(n) && busOf(n) === "main");
    const busSrc = [...before].reverse().find((n) => isRailAudio(n) && busOf(n) !== "main" && busOf(n) !== "mod");
    next = next.filter((e) => ! (e.to === jn.id && e.kind === "audio"));
    next = next.filter((e) => ! (e.from === jn.id && e.kind === "audio"));
    if (mainSrc) {
      strip(mainSrc.id, outId);
      push({ from: mainSrc.id, to: jn.id, kind: "audio", fromJack: "out", toJack: "inA" });
    } else {
      push({ from: "IN", to: jn.id, kind: "audio", fromJack: "out", toJack: "inA" });
    }
    if (busSrc) {
      strip(busSrc.id, outId);
      push({ from: busSrc.id, to: jn.id, kind: "audio", fromJack: "out", toJack: "inB" });
    }
    const after = nodes.slice(idx + 1).find((n) => isRailAudio(n) && busOf(n) === "main");
    push({ from: jn.id, to: after?.id ?? outId, kind: "audio", fromJack: "out", toJack: "in" });
  }

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
