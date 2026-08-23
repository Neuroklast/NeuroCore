import { chipSpec, resolveChipId } from "../assemble/chipSpec";
import { parseDslSketch } from "../presets/parseDslSketch";
import { isKeyword } from "./dslLanguage";

export type CompleteKind = "keyword" | "block" | "property" | "value" | "snippet";

export interface CompleteItem {
  label: string;
  insertText: string;
  detail: string;
  kind: CompleteKind;
}

const BLOCKS = [
  "param", "stage", "filter", "eq", "comp", "gate", "limit",
  "osc", "env", "delay", "reverb", "ms", "octaver", "pitch", "vocoder",
  "xover", "ott", "widen", "ir", "phaser", "flanger", "bus", "out", "split", "custom",
] as const;

const PROPS: Record<string, string[]> = {
  filter: ["type", "cutoff", "resonance", "center", "width", "channel"],
  eq: ["type", "freq", "q", "gain", "channel"],
  stage: ["channel", "y"],
  delay: ["time", "feedback", "mix", "sync", "pingpong"],
  reverb: ["size", "decay", "damp", "mix"],
  ott: ["depth", "time", "low", "mid", "high"],
  ir: ["mix", "gain"],
  out: ["main", "mid", "low", "high"],
  osc: ["shape", "freq", "sync", "depth"],
  env: ["type", "unit", "attack", "release", "hold", "min", "max", "invert"],
  phaser: ["stages", "rate", "depth", "center", "feedback", "mix"],
  flanger: ["rate", "depth", "delay", "feedback", "mix", "invert"],
  comp: ["threshold", "ratio", "attack", "release", "ceiling"],
  gate: ["threshold", "attack", "release", "ceiling"],
  pitch: ["semitones", "shift", "mix", "formant", "ceiling", "sync"],
  ms: ["mode"],
  xover: ["f1", "f2"],
  custom: ["y"],
};

const SNIPPETS: Record<string, string> = {
  filter: "filter1: type = lowpass; cutoff = 1000; resonance = 0.4",
  reverb: "reverb1: size = 0.45; decay = 0.5; damp = 0.3; mix = 0.25",
  delay: "delay1: time = 1/4; feedback = 0.35; mix = 0.3",
  stage: "stage1: y = tanh(x * a)",
  osc: "osc1: shape = sine; freq = 2",
  env: "env1: type = peak; unit = lin; attack = 0.01; release = 0.1; min = 0; max = 1",
  phaser: "phaser1: stages = 6; rate = 0.4; depth = 0.7; center = 800; feedback = 0.3; mix = 0.5",
  flanger: "flanger1: rate = 0.25; depth = 0.7; delay = 2; feedback = 0.45; mix = 0.5; invert = on",
  custom: "custom1: y = x",
};

export function wordAt(text: string, caret: number): { start: number; prefix: string } {
  const n = Math.max(0, Math.min(text.length, caret));
  let start = n;
  while (start > 0 && /[A-Za-z0-9_.]/.test(text[start - 1])) {
    start -= 1;
  }
  return { start, prefix: text.slice(start, n) };
}

export function lineHead(text: string, start: number): string {
  let lineStart = start;
  while (lineStart > 0 && text[lineStart - 1] !== "\n") {
    lineStart -= 1;
  }
  return text.slice(lineStart, start).trim().toLowerCase();
}

function lineBlockKind(head: string): string {
  const first = head.split(":")[0]?.trim() ?? "";
  const letters = first.replace(/[0-9_]+$/g, "");
  if (first.startsWith("filter")) return "filter";
  if (first.startsWith("eq")) return "eq";
  if (first.startsWith("osc")) return "osc";
  if (first.startsWith("env")) return "env";
  if (first.startsWith("comp")) return "comp";
  if (first.startsWith("stage")) return "stage";
  if (first.startsWith("delay")) return "delay";
  if (first.startsWith("reverb")) return "reverb";
  if (first.startsWith("ott")) return "ott";
  if (first.startsWith("ir")) return "ir";
  if (first === "out") return "out";
  if (first.startsWith("ms")) return "ms";
  if (first.startsWith("xover")) return "xover";
  if ((BLOCKS as readonly string[]).includes(letters)) return letters;
  return "";
}

function add(out: CompleteItem[], item: CompleteItem, prefix: string) {
  const p = prefix.toLowerCase();
  const l = item.label.toLowerCase();
  if (p && ! l.startsWith(p) && ! l.includes(p)) {
    return;
  }
  out.push(item);
}

function bracesOk(script: string): boolean {
  const opens = (script.match(/\{/g) ?? []).length;
  const closes = (script.match(/\}/g) ?? []).length;
  if (opens !== closes) {
    return false;
  }
  return (script.match(/\(/g) ?? []).length === (script.match(/\)/g) ?? []).length;
}

function enumsOk(script: string): boolean {
  const { doc } = parseDslSketch(script);
  for (const node of doc.nodes) {
    const letters = node.id.toLowerCase().replace(/[^a-z]/g, "");
    const id = resolveChipId(node.type, node.args);
    const catalog = letters.startsWith("splitlr")
      ? "split_lr"
      : letters.startsWith("joinlr")
        ? "join_lr"
        : letters.startsWith("splitms")
          ? "split_ms"
          : letters.startsWith("joinms")
            ? "join_ms"
            : id;
    const spec = chipSpec(catalog, node.args);
    for (const [key, allowed] of Object.entries(spec.enums)) {
      const raw = node.args[key];
      if (raw == null || ! /^[a-z_][a-z0-9_]*$/i.test(raw.trim())) {
        continue;
      }
      const token = raw.trim().toLowerCase();
      if (! allowed.map((a) => a.toLowerCase()).includes(token)) {
        return false;
      }
    }
  }
  return true;
}

/** True when inserting the item leaves a brace-balanced script with legal enums. */
export function stillParsesAfterInsert(text: string, caret: number, item: CompleteItem): boolean {
  const { start } = wordAt(text, caret);
  const next = `${text.slice(0, start)}${item.insertText}${text.slice(caret)}`;
  if (! bracesOk(next)) {
    return false;
  }
  try {
    return enumsOk(next);
  } catch {
    return false;
  }
}

function compileSafe(text: string, caret: number, items: CompleteItem[]): CompleteItem[] {
  return items.filter((it) => stillParsesAfterInsert(text, caret, it));
}

export function complete(text: string, caret: number): CompleteItem[] {
  const { start, prefix } = wordAt(text, caret);
  const head = lineHead(text, start);
  const kind = lineBlockKind(head);
  const items: CompleteItem[] = [];

  // Only while the caret is on an enum value — not after `type = lowpass; cut…`
  const enumKey = head.match(/\b([a-z]+)\s*=\s*$/i)?.[1]?.toLowerCase();
  if (enumKey && kind) {
    const values = chipSpec(kind).enums[enumKey]
      ?? (enumKey === "type" && kind === "filter"
        ? ["lowpass", "highpass", "bandpass", "allpass"]
        : undefined);
    if (values && values.length > 0) {
      for (const v of values) {
        add(items, { label: v, insertText: v, detail: enumKey, kind: "value" }, prefix);
      }
      return compileSafe(text, caret, items).slice(0, 24);
    }
  }

  if (kind && (head.includes(":") || /;\s*$/.test(head))) {
    if (! /=\s*$/.test(head) || /;\s*$/.test(head) || /:\s*$/.test(head)) {
      for (const p of PROPS[kind] ?? []) {
        add(items, { label: p, insertText: `${p} = `, detail: "property", kind: "property" }, prefix);
      }
      if (items.length > 0) {
        return compileSafe(text, caret, items).slice(0, 24);
      }
    }
  }

  if (kind && ! head.includes(":")) {
    const snippet = SNIPPETS[kind];
    if (snippet) {
      add(items, { label: `${kind} line`, insertText: snippet, detail: "snippet", kind: "snippet" }, "");
    }
    add(items, {
      label: `${kind}1:`,
      insertText: `${kind}1: `,
      detail: "block",
      kind: "block",
    }, "");
    for (const p of PROPS[kind] ?? []) {
      add(items, { label: p, insertText: `${kind}1: ${p} = `, detail: "property", kind: "property" }, "");
    }
    return compileSafe(text, caret, items).slice(0, 24);
  }

  for (const b of BLOCKS) {
    add(items, { label: b, insertText: b, detail: "block", kind: "block" }, prefix);
  }
  if (prefix && SNIPPETS.filter) {
    for (const [k, snip] of Object.entries(SNIPPETS)) {
      if (k.startsWith(prefix.toLowerCase()) || prefix.toLowerCase().startsWith(k)) {
        add(items, { label: `${k} line`, insertText: snip, detail: "snippet", kind: "snippet" }, prefix);
      }
    }
  }
  const broad = items.filter((it) => ! prefix || it.label.toLowerCase().includes(prefix.toLowerCase())
    || (it.kind !== "block" && isKeyword(it.label)));
  return compileSafe(text, caret, broad).slice(0, 24);
}
