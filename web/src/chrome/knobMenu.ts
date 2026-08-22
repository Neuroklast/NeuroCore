import type { KnobState } from "../store/hostStore";
import { labelForWhole, noteRangeToTime, parseNoteToken, timeRangeToNote } from "./noteValue";

export const KNOB_MENU_FIELDS = ["name", "min", "max", "unit", "note", "learn"] as const;
export type KnobMenuField = (typeof KNOB_MENU_FIELDS)[number];

export type KnobEdit = {
  name?: string;
  min?: number;
  max?: number;
  unit?: string;
  isNote?: boolean;
};

/** Same actions in Unit and Circuit. Native menu: rename / min / max / MIDI. */
export function knobMenuFields(knob: Pick<KnobState, "enums">): KnobMenuField[] {
  if (knob.enums && knob.enums.length > 0) {
    return ["name", "learn"];
  }
  return [...KNOB_MENU_FIELDS];
}

export function applyKnobEdit(knob: KnobState, edit: KnobEdit, bpm = 120): KnobState {
  const name = edit.name != null ? edit.name.trim() : knob.name;
  let min = edit.min != null && Number.isFinite(edit.min) ? edit.min : knob.min;
  let max = edit.max != null && Number.isFinite(edit.max) ? edit.max : knob.max;
  const unit = edit.unit != null ? edit.unit.trim() : knob.unit;
  const isNote = edit.isNote != null ? edit.isNote : knob.isNote;
  const flippedNote = edit.isNote != null && edit.isNote !== Boolean(knob.isNote)
    && edit.min == null && edit.max == null;
  if (flippedNote) {
    const conv = edit.isNote
      ? timeRangeToNote(min, max, bpm, unit || knob.unit)
      : noteRangeToTime(min, max, bpm, unit || knob.unit);
    min = conv.min;
    max = conv.max;
  }
  if (! isNote && max < min) {
    const t = min;
    min = max;
    max = t;
  }
  if (! isNote && Math.abs(max - min) < 1e-9) {
    max = min + 1;
  }
  return {
    ...knob,
    name: name || knob.id.toUpperCase(),
    min,
    max,
    unit: unit || undefined,
    isNote,
  };
}

export function parseKnobBound(raw: string, fallback: number, isNote = false): number {
  if (isNote) {
    const whole = parseNoteToken(raw);
    if (whole != null) {
      return whole;
    }
  }
  const n = Number(raw.trim().replace(",", "."));
  return Number.isFinite(n) ? n : fallback;
}

function formatNumericBound(v: number): string {
  if (! Number.isFinite(v)) {
    return "0";
  }
  if (Math.abs(v - Math.round(v)) < 1e-5 && Math.abs(v) < 1e7) {
    return String(Math.round(v));
  }
  let s = v.toFixed(4);
  while (s.includes(".") && s.endsWith("0")) {
    s = s.slice(0, -1);
  }
  if (s.endsWith(".")) {
    s += "0";
  }
  return s;
}

function formatRange(min: number, max: number, isNote: boolean): string {
  if (isNote) {
    return `[${labelForWhole(min)}, ${labelForWhole(max)}]`;
  }
  return `[${formatNumericBound(min)}, ${formatNumericBound(max)}]`;
}

/** Rewrite `param a = Name [min, max]` so Circuit/Terminal see the knob menu. */
export function rewriteParamLine(script: string, letter: string, edit: KnobEdit): string {
  const id = letter.trim().toLowerCase();
  if (! /^[a-f]$/.test(id)) {
    return script;
  }
  const lines = script.split("\n");
  let found = false;
  const next = lines.map((raw) => {
    const trimmed = raw.trimStart();
    if (found || trimmed.startsWith("#") || trimmed.startsWith("//")) {
      return raw;
    }
    if (! trimmed.toLowerCase().startsWith("param")) {
      return raw;
    }
    let rest = trimmed.slice(5).trimStart();
    if (rest.charAt(0).toLowerCase() !== id) {
      return raw;
    }
    rest = rest.slice(1).trimStart();
    if (! rest.startsWith("=")) {
      return raw;
    }
    rest = rest.slice(1).trimStart();
    let nameEnd = rest.length;
    const br = rest.indexOf("[");
    const hash = rest.indexOf("#");
    const sl = rest.indexOf("//");
    if (br >= 0) nameEnd = Math.min(nameEnd, br);
    if (hash >= 0) nameEnd = Math.min(nameEnd, hash);
    if (sl >= 0) nameEnd = Math.min(nameEnd, sl);
    let name = rest.slice(0, nameEnd).trim();
    if (edit.name != null && edit.name.trim()) {
      name = edit.name.trim();
    }
    if (! name) {
      name = id;
    }
    let comment = "";
    const cut = hash >= 0 && (sl < 0 || hash < sl) ? hash : sl;
    if (cut >= 0) {
      comment = rest.slice(cut);
    }
    const indent = raw.length - trimmed.length;
    let min = 0;
    let max = 1;
    let isNote = Boolean(edit.isNote);
    if (br >= 0) {
      const close = rest.indexOf("]", br);
      const inside = close > br ? rest.slice(br + 1, close) : "";
      const parts = inside.split(",");
      const lo = parseKnobBound((parts[0] ?? "").trim(), min, true);
      const hi = parseKnobBound((parts[1] ?? "").trim(), max, true);
      min = lo;
      max = hi;
      isNote = edit.isNote != null ? edit.isNote : Boolean(parseNoteToken((parts[0] ?? "").trim()));
    }
    if (edit.min != null && Number.isFinite(edit.min)) {
      min = edit.min;
    }
    if (edit.max != null && Number.isFinite(edit.max)) {
      max = edit.max;
    }
    if (edit.isNote != null) {
      isNote = edit.isNote;
    }
    const line = `${raw.slice(0, indent)}param ${id} = ${name} ${formatRange(min, max, isNote)}`;
    found = true;
    return comment ? `${line}  ${comment}` : line;
  });
  if (! found) {
    let insertAt = 0;
    for (let i = 0; i < next.length; i += 1) {
      const t = next[i]!.trimStart();
      if (! t || t.startsWith("#") || t.startsWith("//")) {
        insertAt = i + 1;
        continue;
      }
      if (t.toLowerCase().startsWith("param")) {
        insertAt = i + 1;
      } else {
        break;
      }
    }
    const min = edit.min ?? 0;
    const max = edit.max ?? 1;
    const name = (edit.name ?? id).trim() || id;
    next.splice(insertAt, 0, `param ${id} = ${name} ${formatRange(min, max, Boolean(edit.isNote))}`);
  }
  return next.join("\n");
}
