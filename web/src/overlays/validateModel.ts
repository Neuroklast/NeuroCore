import type { AstNode, Diagnostic } from "../bridge/ast";
import { chipSpec, resolveChipId } from "../assemble/chipSpec";
import { isMsDecode, isMsEncode, isXover, visualJacksFor } from "../assemble/visualEdges";
import { isValidLink } from "../assemble/validateLink";
import { parseDslSketch } from "../presets/parseDslSketch";

export interface ValidateCheck {
  id: string;
  title: string;
  ok: boolean;
  detail: string;
}

export interface ValidateIssue {
  severity: "error" | "warn";
  message: string;
}

export interface ValidateReport {
  ok: boolean;
  checks: ValidateCheck[];
  issues: ValidateIssue[];
}

const CHECK_META: Array<{ id: string; title: string }> = [
  { id: "parse", title: "Parse" },
  { id: "braces", title: "Braces" },
  { id: "enums", title: "Enums" },
  { id: "ranges", title: "Ranges" },
  { id: "ms_lr", title: "MS/LR" },
  { id: "bus_join", title: "Bus/Join" },
  { id: "jacks", title: "Jacks" },
];

function lettersOf(id: string): string {
  return id.toLowerCase().replace(/[^a-z]/g, "");
}

function catalogId(node: AstNode): string {
  const letters = lettersOf(node.id);
  if (letters.startsWith("splitlr")) return "split_lr";
  if (letters.startsWith("joinlr")) return "join_lr";
  if (letters.startsWith("splitms")) return "split_ms";
  if (letters.startsWith("joinms")) return "join_ms";
  return resolveChipId(node.type, node.args);
}

function msLrFamily(node: AstNode): "ms" | "lr" | null {
  const id = catalogId(node);
  if (id === "split_ms" || id === "join_ms") return "ms";
  if (id === "split_lr" || id === "join_lr") return "lr";
  if (node.type === "ms" || lettersOf(node.id).startsWith("ms")) return "ms";
  return null;
}

function msLrRole(node: AstNode): "split" | "join" | null {
  const id = catalogId(node);
  if (id === "join") return null;
  if (id === "split_ms" || id === "split_lr") return "split";
  if (id === "join_ms" || id === "join_lr") return "join";
  if (isMsEncode(node)) return "split";
  if (isMsDecode(node)) return "join";
  const letters = lettersOf(node.id);
  if (letters.startsWith("split")) return "split";
  if (letters.startsWith("join") && id !== "join") return "join";
  return null;
}

function isPlainToken(value: string): boolean {
  return /^[a-z_][a-z0-9_]*$/i.test(value.trim());
}

function isNumericLiteral(value: string): boolean {
  return /^[-+]?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?$/i.test(value.trim());
}

function declaredBuses(script: string): Set<string> {
  const buses = new Set<string>(["main"]);
  for (const raw of script.split(/\r?\n/)) {
    const line = raw.trim();
    const m = line.match(/^bus\s+([a-z_][a-z0-9_]*)\s*:/i);
    if (m?.[1]) {
      buses.add(m[1].toLowerCase());
    }
  }
  return buses;
}

function checkParse(
  script: string,
  diagnostics: Diagnostic[],
  nodeCount: number,
): ValidateCheck {
  const text = script.trim();
  if (! text) {
    return { id: "parse", title: "Parse", ok: false, detail: "Empty script." };
  }
  if (diagnostics.length > 0) {
    const first = diagnostics[0]!;
    return {
      id: "parse",
      title: "Parse",
      ok: false,
      detail: `line ${first.line}: ${first.message}`,
    };
  }
  const hasBlock = script.split(/\r?\n/).some((line) => {
    const code = line.replace(/[#/].*$/, "").trim();
    return code.includes(":");
  });
  if (! hasBlock && nodeCount === 0) {
    return { id: "parse", title: "Parse", ok: false, detail: "No block lines with ':'." };
  }
  return { id: "parse", title: "Parse", ok: true, detail: "Script parsed." };
}

function checkBraces(script: string): ValidateCheck {
  const text = script.trim();
  const opens = (text.match(/\{/g) ?? []).length;
  const closes = (text.match(/\}/g) ?? []).length;
  if (opens !== closes) {
    return { id: "braces", title: "Braces", ok: false, detail: "Unbalanced { }." };
  }
  const parens = (text.match(/\(/g) ?? []).length - (text.match(/\)/g) ?? []).length;
  if (parens !== 0) {
    return { id: "braces", title: "Braces", ok: false, detail: "Unbalanced ( )." };
  }
  return { id: "braces", title: "Braces", ok: true, detail: "Braces and parentheses balanced." };
}

function checkEnums(nodes: AstNode[]): ValidateCheck {
  const bad: string[] = [];
  for (const node of nodes) {
    const spec = chipSpec(catalogId(node), node.args);
    for (const [key, allowed] of Object.entries(spec.enums)) {
      const raw = node.args[key];
      if (raw == null || ! isPlainToken(raw)) {
        continue;
      }
      const token = raw.trim().toLowerCase();
      if (! allowed.map((a) => a.toLowerCase()).includes(token)) {
        bad.push(`${node.id}.${key}=${raw}`);
      }
    }
  }
  if (bad.length > 0) {
    return { id: "enums", title: "Enums", ok: false, detail: `Illegal enum: ${bad.join(", ")}` };
  }
  return { id: "enums", title: "Enums", ok: true, detail: "Enum args match catalog." };
}

function checkRanges(nodes: AstNode[]): ValidateCheck {
  const bad: string[] = [];
  for (const node of nodes) {
    const spec = chipSpec(catalogId(node), node.args);
    for (const [key, range] of Object.entries(spec.ranges)) {
      const raw = node.args[key];
      if (raw == null || ! isNumericLiteral(raw)) {
        continue;
      }
      const n = Number(raw);
      if (! Number.isFinite(n) || n < range.min || n > range.max) {
        bad.push(`${node.id}.${key}=${raw} (expected ${range.min}…${range.max})`);
      }
    }
  }
  if (bad.length > 0) {
    return { id: "ranges", title: "Ranges", ok: false, detail: `Out of range: ${bad.join(", ")}` };
  }
  return { id: "ranges", title: "Ranges", ok: true, detail: "Numeric args within catalog ranges." };
}

function checkMsLr(nodes: AstNode[]): ValidateCheck {
  const tagged = nodes
    .map((n) => ({ n, family: msLrFamily(n), role: msLrRole(n) }))
    .filter((t) => t.family && t.role) as Array<{
    n: AstNode;
    family: "ms" | "lr";
    role: "split" | "join";
  }>;
  const has = (family: "ms" | "lr", role: "split" | "join") =>
    tagged.some((t) => t.family === family && t.role === role);
  if (has("ms", "split") && has("lr", "join")) {
    return {
      id: "ms_lr",
      title: "MS/LR",
      ok: false,
      detail: "Split Mid/Side must not pair with Join Left/Right.",
    };
  }
  if (has("lr", "split") && has("ms", "join")) {
    return {
      id: "ms_lr",
      title: "MS/LR",
      ok: false,
      detail: "Split Left/Right must not pair with Join Mid/Side.",
    };
  }
  return { id: "ms_lr", title: "MS/LR", ok: true, detail: "MS/LR families are consistent." };
}

function checkBusJoin(script: string, nodes: AstNode[]): ValidateCheck {
  const buses = declaredBuses(script);
  const named = [...buses].filter((b) => b !== "main");
  if (named.length === 0) {
    return { id: "bus_join", title: "Bus/Join", ok: true, detail: "No named buses." };
  }
  const mixer = nodes.find((n) => n.type === "join");
  if (mixer) {
    return { id: "bus_join", title: "Bus/Join", ok: true, detail: "Named buses reach join." };
  }
  const out = nodes.find((n) => n.type === "out");
  if (! out) {
    return {
      id: "bus_join",
      title: "Bus/Join",
      ok: false,
      detail: `Named bus(es) ${named.join(", ")} need an out mix or join.`,
    };
  }
  const missing = named.filter((b) => ! Object.prototype.hasOwnProperty.call(out.args, b));
  if (missing.length > 0) {
    return {
      id: "bus_join",
      title: "Bus/Join",
      ok: false,
      detail: `Out does not mix bus: ${missing.join(", ")}`,
    };
  }
  return { id: "bus_join", title: "Bus/Join", ok: true, detail: "Named buses reach out." };
}

const XOVER_BANDS = new Set(["low", "mid", "high"]);

function jackKinds(
  node: AstNode | undefined,
  jackId: string,
  xoverBands: boolean,
): { kind: string; output: boolean } | null {
  if (! node) {
    if (jackId === "out") return { kind: "audio", output: true };
    if (jackId === "in") return { kind: "audio", output: false };
    if (xoverBands && XOVER_BANDS.has(jackId)) return { kind: "mix", output: false };
    return { kind: "audio", output: jackId === "out" };
  }
  const jacks = visualJacksFor(node, []).length > 0 && (node.type === "ms" || node.type.startsWith("xover") || isXover(node))
    ? visualJacksFor(node, [])
    : (node.jacks ?? []);
  const hit = jacks.find((j) => j.id === jackId);
  if (hit) {
    return { kind: hit.kind, output: hit.output };
  }
  const spec = chipSpec(catalogId(node), node.args);
  if (spec.audioOuts.includes(jackId)) return { kind: "audio", output: true };
  if (spec.audioIns.includes(jackId)) return { kind: "audio", output: false };
  if (spec.modIns.includes(jackId)) return { kind: "mod", output: false };
  if (node.type === "out" && (jackId === "in" || (xoverBands && XOVER_BANDS.has(jackId)))) {
    return { kind: "mix", output: false };
  }
  return null;
}

function checkJacks(script: string, nodes: AstNode[], edges: { from: string; to: string; fromJack: string; toJack: string; kind: string }[]): ValidateCheck {
  const buses = declaredBuses(script);
  const xoverBands = nodes.some((n) => isXover(n));
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const out = nodes.find((n) => n.type === "out");
  if (out) {
    for (const key of Object.keys(out.args)) {
      const k = key.toLowerCase();
      if (k === "in" || buses.has(k) || (xoverBands && XOVER_BANDS.has(k))) {
        continue;
      }
      return {
        id: "jacks",
        title: "Jacks",
        ok: false,
        detail: `Out mix jack '${key}' is not a declared bus.`,
      };
    }
  }
  for (const e of edges) {
    const src = byId.get(e.from);
    const dst = byId.get(e.to);
    const from = jackKinds(src, e.fromJack || "out", xoverBands);
    const to = jackKinds(dst, e.toJack || "in", xoverBands);
    if (! from || ! to) {
      return {
        id: "jacks",
        title: "Jacks",
        ok: false,
        detail: `Unknown jack on ${e.from}:${e.fromJack} → ${e.to}:${e.toJack}`,
      };
    }
    if (! isValidLink(from, to)) {
      return {
        id: "jacks",
        title: "Jacks",
        ok: false,
        detail: `Illegal link ${e.from}:${e.fromJack} → ${e.to}:${e.toJack}`,
      };
    }
  }
  return { id: "jacks", title: "Jacks", ok: true, detail: "Jack wiring looks consistent." };
}

export function validateScript(script: string, diagnostics: Diagnostic[]): ValidateReport {
  const { doc } = parseDslSketch(script);
  const nodes = doc.nodes;
  const edges = doc.edges ?? [];

  const checks: ValidateCheck[] = [
    checkParse(script, diagnostics, nodes.length),
    checkBraces(script),
    checkEnums(nodes),
    checkRanges(nodes),
    checkMsLr(nodes),
    checkBusJoin(script, nodes),
    checkJacks(script, nodes, edges),
  ];

  // Keep stable order / titles even if a helper drifts.
  for (let i = 0; i < CHECK_META.length; i += 1) {
    const meta = CHECK_META[i]!;
    const cur = checks[i];
    if (! cur || cur.id !== meta.id) {
      checks[i] = { id: meta.id, title: meta.title, ok: false, detail: "Missing check." };
    } else {
      checks[i] = { ...cur, title: meta.title };
    }
  }

  const issues: ValidateIssue[] = [];
  for (const c of checks) {
    if (! c.ok) {
      issues.push({ severity: "error", message: `${c.title}: ${c.detail}` });
    }
  }
  if (/\bNaN\b|\bInf\b|1\s*\/\s*0/i.test(script)) {
    issues.push({ severity: "warn", message: "Possible NaN / Inf in the script." });
  }

  return { ok: checks.every((c) => c.ok), checks, issues };
}

/** Save/Apply hook — same report as the Validate overlay. */
export function validateOnSave(script: string, diagnostics: Diagnostic[]): ValidateReport {
  return validateScript(script, diagnostics);
}
