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

export { formatKnobDisplay, round2 } from "./noteValue";

export function knobScaleMode(enums: string[] | undefined | null): "arc" | "ticks" {
  return enums && enums.length > 0 ? "ticks" : "arc";
}

export function knobTickCount(enums: string[] | undefined | null): number {
  return enums && enums.length > 0 ? enums.length : 0;
}

/** Evenly spaced 0..1 norms for N enum detents. */
export function knobTickNorms(n: number): number[] {
  if (n <= 0) {
    return [];
  }
  if (n === 1) {
    return [0];
  }
  return Array.from({ length: n }, (_, i) => i / (n - 1));
}

export function snapEnum01(value01: number, n: number): number {
  if (n <= 1) {
    return 0;
  }
  const v = Math.max(0, Math.min(1, value01));
  const i = Math.round(v * (n - 1));
  return i / (n - 1);
}

export function enumIndexAt01(value01: number, n: number): number {
  if (n <= 1) {
    return 0;
  }
  const v = Math.max(0, Math.min(1, value01));
  return Math.max(0, Math.min(n - 1, Math.round(v * (n - 1))));
}

/** One mouse-wheel notch in 0..1. Trackpad pixels still count as one notch. */
export const WHEEL_NOTCH = 0.03;

export function wheelStep01(deltaY: number, shift = false): number {
  if (! Number.isFinite(deltaY) || deltaY === 0) {
    return 0;
  }
  const mag = WHEEL_NOTCH * (shift ? 0.25 : 1);
  return deltaY > 0 ? -mag : mag;
}

export function applyWheel01(current: number, deltaY: number, shift = false): number {
  const next = current + wheelStep01(deltaY, shift);
  if (! Number.isFinite(next)) {
    return 0;
  }
  return Math.max(0, Math.min(1, next));
}

export function enumLabelAt01(value01: number, enums: string[]): string {
  if (enums.length === 0) {
    return "";
  }
  return enums[enumIndexAt01(value01, enums.length)] ?? "";
}
