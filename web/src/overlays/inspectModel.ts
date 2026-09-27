import type { AstDocument, AstNode, AstParam } from "../bridge/ast";
import { chipSpec } from "../assemble/chipSpec";
import {
  labelForWhole,
  NOTE_GRID,
  parseNoteToken,
  wholeToHz,
  wholeToMs,
} from "../chrome/noteValue";

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
  min: number;
  max: number;
  timing: boolean;
}

/** Inspect note picker: 8 bars down to a sixteenth, plus dotted/triplet in that span. */
export const INSPECT_NOTE_MIN = 0.0625;
export const INSPECT_NOTE_MAX = 8;

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

export function inspectTimingUnit(unit: string): boolean {
  const u = unit.toLowerCase();
  return u === "s" || u === "ms" || u === "hz";
}

/** NOTE is duration or LFO rate, not a filter cutoff in Hz. */
export function inspectTimingField(key: string, unit: string): boolean {
  const u = unit.toLowerCase();
  if (u === "s" || u === "ms") {
    return true;
  }
  if (u === "hz") {
    const k = key.toLowerCase();
    return k === "freq" || k === "rate";
  }
  return false;
}

function noteMapped(whole: number, unit: string, bpm: number): number {
  const u = unit.toLowerCase();
  if (u === "hz") {
    return wholeToHz(whole, bpm);
  }
  const ms = wholeToMs(whole, bpm);
  return u === "s" ? ms / 1000 : ms;
}

function inRange(n: number, min: number, max: number): boolean {
  const lo = Math.min(min, max);
  const hi = Math.max(min, max);
  return n + 1e-9 >= lo && n - 1e-9 <= hi;
}

export function inspectNoteChoices(unit: string, min: number, max: number, bpm: number): string[] {
  if (! inspectTimingUnit(unit)) {
    return [];
  }
  return NOTE_GRID
    .filter((g) => g.whole + 1e-9 >= INSPECT_NOTE_MIN && g.whole - 1e-9 <= INSPECT_NOTE_MAX)
    .filter((g) => inRange(noteMapped(g.whole, unit, bpm), min, max))
    .map((g) => g.label);
}

export function inspectValueFromNote(label: string, unit: string, bpm: number): string | null {
  const whole = parseNoteToken(label);
  if (whole == null || ! inspectTimingUnit(unit)) {
    return null;
  }
  const n = noteMapped(whole, unit, bpm);
  if (! Number.isFinite(n)) {
    return null;
  }
  if (Number.isInteger(n)) {
    return String(n);
  }
  const digits = unit.toLowerCase() === "s" ? 5 : 3;
  return String(Number(n.toFixed(digits)));
}

export function inspectSnapNote(raw: string, unit: string, min: number, max: number, bpm: number): string | null {
  const choices = inspectNoteChoices(unit, min, max, bpm);
  if (choices.length === 0) {
    return null;
  }
  const n = Number(raw);
  if (! Number.isFinite(n)) {
    return choices[0] ?? null;
  }
  let best = choices[0]!;
  let err = Number.POSITIVE_INFINITY;
  for (const label of choices) {
    const mapped = Number(inspectValueFromNote(label, unit, bpm));
    if (! Number.isFinite(mapped)) {
      continue;
    }
    const e = Math.abs(mapped - n);
    if (e < err) {
      err = e;
      best = label;
    }
  }
  return best;
}

export function inspectArgFields(node: AstNode): InspectArgField[] {
  const spec = chipSpec(node.type, node.args);
  return Object.entries(node.args).map(([key, value]) => {
    const options = inspectEnumOptions(node.type, key);
    if (options.length > 0) {
      return { key, value, kind: "enum", options, unit: "", min: 0, max: 0, timing: false };
    }
    const range = spec.ranges[key];
    if (range) {
      const unit = range.unit ?? "";
      return {
        key,
        value,
        kind: "number",
        options: [],
        unit,
        min: range.min,
        max: range.max,
        timing: inspectTimingField(key, unit),
      };
    }
    return { key, value, kind: "text", options: [], unit: "", min: 0, max: 0, timing: false };
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
    const lo = p.isNote ? labelForWhole(p.min) : formatBound2(p.min);
    const hi = p.isNote ? labelForWhole(p.max) : formatBound2(p.max);
    const value = `${p.name} [${lo} … ${hi}]${unit && ! p.isNote ? ` ${unit}` : ""}`;
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
