/** Circuit bottom rail: no letter on the title. Left stack still shows A–F. */
export function knobShowsLetter(compact: boolean): boolean {
  return ! compact;
}

/** Circuit bind source is a jack on the north face. Left rail keeps the letter plug. */
export function knobBindKind(compact: boolean): "jack" | "letter" {
  return compact ? "jack" : "letter";
}

/** Circuit bottom rail stacks the bind jack above the name. Left stack parks the letter in the corner. */
export function knobPlugPlacement(compact: boolean): "stack" | "corner" {
  return compact ? "stack" : "corner";
}

/** Circuit rail: bind jack lives in document flow above the name. Never absolute-over-title. */
export function knobPlugPosition(place: "stack" | "corner"): "flow" | "absolute" {
  return place === "stack" ? "flow" : "absolute";
}

export const KNOB_CX = 40;
export const KNOB_CY = 36;
export const KNOB_ARC_R = 27;
/** 240° sweep, 4π/3 → 8π/3. */
export const KNOB_SWEEP = (Math.PI * 4) / 3;

export function knobCircumference(r = KNOB_ARC_R): number {
  return 2 * Math.PI * r;
}

export function knobArcLen(r = KNOB_ARC_R): number {
  return r * KNOB_SWEEP;
}

/** Empty at 0, full at 1. Bind to stroke-dashoffset. */
export function knobArcOffset(value01: number, r = KNOB_ARC_R): number {
  const v = Math.max(0, Math.min(1, value01));
  return (1 - v) * knobArcLen(r);
}

export const KNOB_CARD_CLIP =
  "polygon(10px 0, 100% 0, 100% calc(100% - 10px), calc(100% - 10px) 100%, 0 100%, 0 10px)";

export function knobInteractive(active: boolean): boolean {
  return active;
}

/** Left rail titles sit under the 45° cut, not on the card lip. */
export function knobTitleInset(place: "stack" | "corner"): number {
  return place === "corner" ? 10 : 2;
}
