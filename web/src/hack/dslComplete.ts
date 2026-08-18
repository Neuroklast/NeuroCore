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
  "osc", "env", "delay", "reverb", "ms", "octaver", "vocoder",
  "xover", "ott", "widen", "ir", "bus", "out", "split", "custom",
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
  comp: ["threshold", "ratio", "attack", "release"],
  gate: ["threshold", "attack", "release"],
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

export function complete(text: string, caret: number): CompleteItem[] {
  const { start, prefix } = wordAt(text, caret);
  const head = lineHead(text, start);
  const kind = lineBlockKind(head);
  const items: CompleteItem[] = [];

  const afterType = /type\s*=\s*$/.test(head) || /type\s*=\s*$/.test(`${head}${prefix}`);
  if (head.includes("type =") || afterType) {
    const values = kind === "eq"
      ? ["peak", "notch", "lowcut", "highcut"]
      : kind === "osc"
        ? ["sine", "saw", "triangle", "square"]
        : kind === "ms"
          ? ["encode", "decode"]
          : ["lowpass", "highpass", "bandpass"];
    for (const v of values) {
      add(items, { label: v, insertText: v, detail: "type", kind: "value" }, prefix);
    }
    return items.slice(0, 24);
  }

  if (kind && (head.includes(":") || /;\s*$/.test(head))) {
    if (! /=\s*$/.test(head) || /;\s*$/.test(head) || /:\s*$/.test(head)) {
      for (const p of PROPS[kind] ?? []) {
        add(items, { label: p, insertText: `${p} = `, detail: "property", kind: "property" }, prefix);
      }
      if (items.length > 0) {
        return items.slice(0, 24);
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
    return items.slice(0, 24);
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
  return items.filter((it) => ! prefix || it.label.toLowerCase().includes(prefix.toLowerCase())
    || (it.kind !== "block" && isKeyword(it.label))).slice(0, 24);
}
