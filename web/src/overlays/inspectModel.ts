import type { AstDocument, AstNode, AstParam } from "../bridge/ast";
import { chipSpec } from "../assemble/chipSpec";

export interface InspectRow {
  group: "meta" | "arg" | "jack" | "param" | "var";
  key: string;
  value: string;
}

export type InspectArgKind = "enum" | "number" | "text";

export interface InspectArgField {
  key: string;
  value: string;
  kind: InspectArgKind;
  options: string[];
  unit: string;
}

export interface BoundKnobRow {
  letter: string;
  name: string;
  unit: string;
  /** Display with two decimals and unit, e.g. `Cutoff [200.00 … 2500.00] Hz`. */
  value: string;
}

const PURE_NUMBER = /^-?\d+(\.\d+)?([eE][+-]?\d+)?$/;

function lettersInArgs(args: Record<string, string>): Set<string> {
  const used = new Set<string>();
  for (const v of Object.values(args)) {
    const exact = v.trim().toLowerCase();
    if (/^[a-f]$/.test(exact)) {
      used.add(exact);
    }
    for (const letter of v.match(/\b[a-f]\b/gi) ?? []) {
      used.add(letter.toLowerCase());
    }
  }
  return used;
}

/** Unit from the first ranged arg on this node that references the letter. */
function unitForLetter(node: AstNode, letter: string): string {
  const spec = chipSpec(node.type, node.args);
  const needle = letter.toLowerCase();
  for (const [key, raw] of Object.entries(node.args)) {
    if (! /\b[a-f]\b/i.test(raw)) {
      continue;
    }
    if (! raw.toLowerCase().includes(needle)) {
      continue;
    }
    const unit = spec.ranges[key]?.unit;
    if (unit) {
      return unit;
    }
  }
  return "";
}

function formatBound2(v: number): string {
  return v.toFixed(2);
}

export function inspectEnumOptions(type: string, key: string): string[] {
  return [...(chipSpec(type).enums[key] ?? [])];
}

/** Pure numbers clamp to chipSpec.ranges; expressions and letters pass through. */
export function clampInspectArg(type: string, key: string, raw: string): string {
  const range = chipSpec(type).ranges[key];
  if (! range) {
    return raw;
  }
  const trimmed = raw.trim();
  if (! PURE_NUMBER.test(trimmed)) {
    return raw;
  }
  const n = Number(trimmed);
  if (! Number.isFinite(n)) {
    return String(range.min);
  }
  const clamped = Math.min(range.max, Math.max(range.min, n));
  if (Number.isInteger(clamped)) {
    return String(clamped);
  }
  return String(clamped);
}

export function inspectBlurb(type: string): string {
  return chipSpec(type).blurb;
}

export function inspectArgFields(node: AstNode): InspectArgField[] {
  const spec = chipSpec(node.type, node.args);
  return Object.entries(node.args).map(([key, value]) => {
    const options = inspectEnumOptions(node.type, key);
    if (options.length > 0) {
      return { key, value, kind: "enum", options, unit: "" };
    }
    const range = spec.ranges[key];
    if (range) {
      return { key, value, kind: "number", options: [], unit: range.unit ?? "" };
    }
    return { key, value, kind: "text", options: [], unit: "" };
  });
}

export function boundKnobRows(node: AstNode | undefined, doc: AstDocument | null): BoundKnobRow[] {
  if (! node || ! doc) {
    return [];
  }
  const used = lettersInArgs(node.args);
  const params: AstParam[] = doc.params ?? [];
  const rows: BoundKnobRow[] = [];
  for (const p of params) {
    const letter = p.alias.toLowerCase();
    if (! used.has(letter)) {
      continue;
    }
    const unit = unitForLetter(node, letter);
    const value = `${p.name} [${formatBound2(p.min)} … ${formatBound2(p.max)}]${unit ? ` ${unit}` : ""}`;
    rows.push({ letter, name: p.name, unit, value });
  }
  return rows;
}

export function inspectRows(node: AstNode | undefined, doc: AstDocument | null): InspectRow[] {
  if (! node) {
    return [];
  }
  const rows: InspectRow[] = [
    { group: "meta", key: "id", value: node.id },
    { group: "meta", key: "type", value: node.type },
    { group: "meta", key: "bus", value: node.busName || "main" },
  ];
  if (node.trailingComment) {
    rows.push({ group: "meta", key: "comment", value: node.trailingComment });
  }
  for (const [k, v] of Object.entries(node.args)) {
    rows.push({ group: "arg", key: k, value: v });
  }
  for (const j of node.jacks ?? []) {
    rows.push({
      group: "jack",
      key: j.id,
      value: `${j.kind} ${j.output ? "out" : "in"}`,
    });
  }
  for (const knob of boundKnobRows(node, doc)) {
    rows.push({
      group: "param",
      key: knob.letter,
      value: knob.value,
    });
  }
  for (const other of doc?.nodes ?? []) {
    if (other.id === node.id) {
      continue;
    }
    const hit = Object.values(node.args).some((v) => {
      const s = v.toLowerCase();
      const t = other.id.toLowerCase();
      return s.includes(t);
    });
    if (hit) {
      rows.push({ group: "var", key: other.id, value: other.type });
    }
  }
  return rows;
}
