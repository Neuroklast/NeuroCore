import type { KnobState } from "../store/hostStore";

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

export function applyKnobEdit(knob: KnobState, edit: KnobEdit): KnobState {
  const name = edit.name != null ? edit.name.trim() : knob.name;
  let min = edit.min != null && Number.isFinite(edit.min) ? edit.min : knob.min;
  let max = edit.max != null && Number.isFinite(edit.max) ? edit.max : knob.max;
  if (max < min) {
    const t = min;
    min = max;
    max = t;
  }
  if (Math.abs(max - min) < 1e-9) {
    max = min + 1;
  }
  const unit = edit.unit != null ? edit.unit.trim() : knob.unit;
  return {
    ...knob,
    name: name || knob.id.toUpperCase(),
    min,
    max,
    unit: unit || undefined,
    isNote: edit.isNote != null ? edit.isNote : knob.isNote,
  };
}

export function parseKnobBound(raw: string, fallback: number): number {
  const n = Number(raw.trim());
  return Number.isFinite(n) ? n : fallback;
}
