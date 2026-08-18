export { PLASMA_DRIVER } from "../theme/fx";

export const TUBE = {
  audioOuter: 20,
  audioGlass: 16,
  audioBore: 12,
  modOuter: 12,
  modGlass: 9,
  jack: 16,
} as const;

/** Glass shell never glows. */
export function shellGlow(): number {
  return 0;
}

/** 0..1, logarithmic in amplitude so quiet signals still read. */
export function logAmp(peak: number): number {
  const p = Math.max(0, Number.isFinite(peak) ? peak : 0);
  return Math.min(1, Math.log10(1 + p * 9));
}

/** Only the plasma wave may glow. Intensity follows log amplitude. */
export function plasmaGlow(peak: number, motion: string, reduced: boolean): number {
  if (reduced || motion === "off") {
    return 0;
  }
  return 0.04 + logAmp(peak) * 0.28;
}

export function tubeGlow(peak: number, motion: string, reduced: boolean): number {
  return plasmaGlow(peak, motion, reduced);
}

export function plasmaAmp(peak: number, bore: number): number {
  return bore * 0.22 * (0.35 + 0.65 * logAmp(peak));
}

/** Fixed bead pitch along the glass. One long snake makes the tube wobble. */
export const PLASMA_PITCH = 22;
export const PLASMA_SPAN = 8;

export function plasmaWindows(
  length: number,
  pitch = PLASMA_PITCH,
  span = PLASMA_SPAN,
): Array<{ s0: number; s1: number }> {
  if (! (length > 0) || ! (span > 0) || ! (pitch > 0)) {
    return [];
  }
  if (length <= span) {
    return [{ s0: 0, s1: length }];
  }
  const out: Array<{ s0: number; s1: number }> = [];
  const half = span * 0.5;
  for (let c = half; c <= length - half + 0.01; c += pitch) {
    out.push({ s0: c - half, s1: c + half });
  }
  return out;
}

export function jackDiameter(): number {
  return TUBE.jack;
}
