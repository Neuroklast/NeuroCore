import { formatMapped, mappedValue } from "../theme/tokens";

export type LiveKnob = { id: string; value: number; min: number; max: number };

export function knobMapped(letter: string, knobs: LiveKnob[]): number | null {
  const k = knobs.find((x) => x.id === letter.toLowerCase());
  if (! k) {
    return null;
  }
  return mappedValue(k.value, k.min, k.max);
}

/** Formula plus the live number for bound letters. */
export function liveArg(expr: string, knobs: LiveKnob[]): { formula: string; live: string } {
  const t = expr.trim();
  if (/^[a-f]$/i.test(t)) {
    const v = knobMapped(t, knobs);
    return { formula: t.toLowerCase(), live: v == null ? t : formatMapped(v) };
  }
  let live = expr;
  for (const k of knobs) {
    if (! /^[a-f]$/.test(k.id)) {
      continue;
    }
    const n = formatMapped(mappedValue(k.value, k.min, k.max));
    live = live.replace(new RegExp(`\\b${k.id}\\b`, "g"), n);
  }
  return { formula: expr, live };
}
