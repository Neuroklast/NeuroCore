import { chipSpec, resolveChipId } from "../assemble/chipSpec";
import { parseDslSketch } from "../presets/parseDslSketch";
import { isKeyword } from "./dslLanguage";

export type CompleteKind = "keyword" | "block" | "property" | "value" | "snippet";

export interface CompleteItem {
  label: string;
  insertText: string;
  detail: string;
  kind: CompleteKind;
  insertAsSnippet?: boolean;
  plainInsertText?: string;
  replaceStart?: number;
}

const BLOCKS = [
  "param", "stage", "filter", "eq", "comp", "gate", "limit",
  "osc", "env", "delay", "reverb", "ms", "octaver", "pitch", "vocoder",
  "xover", "ott", "widen", "ir", "phaser", "flanger", "bus", "out", "split", "custom",
] as const;

function properties(kind: string): string[] {
  return kind === "out" ? ["main", "low", "mid", "high", "gain"] : chipSpec(kind).paramJacks;
}

function unusedId(kind: string, script: string): string {
  if (kind === "out") return "out";
  const stem = kind === "gate" || kind === "noisegate" ? "ngate" : kind;
  const used = new Set([...script.matchAll(/^\s*([a-z_][a-z_0-9]*)\s*:/gim)].map(m => m[1].toLowerCase()));
  for (let i = 1; ; ++i) if (!used.has(`${stem}${i}`)) return `${stem}${i}`;
}

function propertyItem(kind: string, key: string): CompleteItem {
  const spec = chipSpec(kind);
  const value = spec.defaultArgs[key] ?? spec.enums[key]?.[0] ?? "0";
  return { label: key, insertText: `${key} = \${1:${value}}`, plainInsertText: `${key} = ${value}`,
    detail: "property · Tab to edit", kind: "property", insertAsSnippet: true };
}

function snippetFor(kind: string, script: string): { snippet: string; plain: string } | null {
  if (kind === "param") return { snippet: "param ${1:a} = ${2:Macro} [${3:0}, ${4:1}]", plain: "param a = Macro [0, 1]" };
  if (kind === "bus") return { snippet: "bus ${1:dirt}:", plain: "bus dirt:" };
  const spec = chipSpec(kind);
  if (spec.paramJacks.length === 0) return null;
  const id = unusedId(kind, script);
  const plainArgs = spec.paramJacks.map((key) => `${key} = ${spec.defaultArgs[key] ?? (spec.enums[key]?.[0] ?? "0")}`);
  const snippetArgs = spec.paramJacks.map((key, i) => `${key} = \${${i + 2}:${spec.defaultArgs[key] ?? (spec.enums[key]?.[0] ?? "0")}}`);
  return {
    snippet: `\${1:${id}}: ${snippetArgs.join("; ")}`,
    plain: `${id}: ${plainArgs.join("; ")}`,
  };
}

function snippetItem(kind: string, script: string): CompleteItem | null {
  const text = snippetFor(kind, script);
  return text ? {
    label: `${kind} line`, insertText: text.snippet, plainInsertText: text.plain,
    detail: "all parameters · Tab to edit", kind: "snippet", insertAsSnippet: true,
  } : null;
}

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
  if (/^(ngate|noisegate|gate)/.test(first)) return "gate";
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
  const next = `${text.slice(0, start)}${item.plainInsertText ?? item.insertText}${text.slice(caret)}`;
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
  // Completion operates on the current line, including inside an unfinished
  // document. Compilation remains the authority when the draft is submitted.
  const lineStart = text.lastIndexOf("\n", caret - 1) + 1;
  const lineEnd = text.indexOf("\n", caret);
  const line = text.slice(lineStart, lineEnd < 0 ? text.length : lineEnd);
  const local = caret - lineStart;
  return items.filter((it) => {
    const { start } = wordAt(line, local);
    try { return enumsOk(`${line.slice(0, start)}${it.plainInsertText ?? it.insertText}${line.slice(local)}`); }
    catch { return false; }
  });
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
      for (const p of properties(kind)) {
        add(items, propertyItem(kind, p), prefix);
      }
      if (items.length > 0) {
        return compileSafe(text, caret, items).slice(0, 24);
      }
    }
  }

  if (kind && ! head.includes(":")) {
    const snippet = snippetItem(kind, text);
    if (snippet) {
      const lineStart = text.lastIndexOf("\n", start - 1) + 1;
      snippet.replaceStart = lineStart + (text.slice(lineStart, start).match(/^\s*/)?.[0].length ?? 0);
      add(items, snippet, "");
    }
    return items;
  }

  for (const b of BLOCKS) {
    add(items, { label: b, insertText: b, detail: "block", kind: "block" }, prefix);
  }
  if (prefix) {
    for (const k of BLOCKS) {
      if (k.startsWith(prefix.toLowerCase()) || prefix.toLowerCase().startsWith(k)) {
        const snippet = snippetItem(k, text);
        if (snippet) add(items, snippet, prefix);
      }
    }
  }
  const broad = items.filter((it) => ! prefix || it.label.toLowerCase().includes(prefix.toLowerCase())
    || (it.kind !== "block" && isKeyword(it.label)));
  return compileSafe(text, caret, broad).slice(0, 24);
}
