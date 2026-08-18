import { lfoPeriodMs } from "./lfoLamp";
import { logAmp, PLASMA_PITCH, PLASMA_SPAN } from "./tubeModel";

export type CableKind = "audio" | "mod";
export type CableStyle = "wave" | "dots";
export type CableLayer = "lfo" | "wave" | "dots" | "still";

/** One LFO period = one pulse trip down the cable. */
export function lfoChaseMs(hz: number): number {
  return lfoPeriodMs(hz);
}

export function lfoDash(pathLen: number, blob = 14): { dash: string; cycle: number } {
  const gap = Math.max(48, pathLen);
  return { dash: `${blob} ${gap}`, cycle: blob + gap };
}

/** Wave packets sit on a fixed pixel pitch, not stretched to the cable. */
export const WAVE_MS_PER_PITCH = 180;

export function waveDash(pitch = PLASMA_PITCH, span = PLASMA_SPAN): string {
  const lit = Math.max(1, span);
  const gap = Math.max(1, pitch - lit);
  return `${lit} ${gap}`;
}

export function dashPatternLength(dash: string): number {
  return dash
    .trim()
    .split(/\s+/)
    .reduce((sum, part) => sum + (Number(part) || 0), 0);
}

/** Map a scope buffer onto the same pitch so the DSP wave slides, it does not stretch. */
export function waveDashFromScope(
  samples: ArrayLike<number>,
  pitch = PLASMA_PITCH,
  windows = 16,
): string {
  const n = Math.max(1, Math.min(windows, samples.length || 0));
  if (! samples.length) {
    return waveDash(pitch);
  }
  const step = Math.max(1, Math.floor(samples.length / n));
  const parts: string[] = [];
  for (let i = 0; i < n; i += 1) {
    const v = Math.abs(Number(samples[i * step] ?? 0));
    const amp = Number.isFinite(v) ? Math.min(1, v) : 0;
    const lit = Math.max(1.4, pitch * (0.16 + 0.72 * amp));
    parts.push(`${lit.toFixed(1)} ${(pitch - lit).toFixed(1)}`);
  }
  return parts.join(" ");
}

export function waveAnimMs(patternPx: number, pitch = PLASMA_PITCH): number {
  const p = pitch > 0 ? pitch : PLASMA_PITCH;
  const len = patternPx > 0 ? patternPx : p;
  return Math.max(80, Math.round((len / p) * WAVE_MS_PER_PITCH));
}

export function dotDash(pitch = 24, size = 2.4): string {
  return `${size} ${Math.max(8, pitch - size)}`;
}

/** Loud signal → shorter period → faster dots. */
export function dotPeriodMs(peak: number): number {
  const a = logAmp(peak);
  return Math.round(1420 - a * 1200);
}

export function cableLayer(
  kind: CableKind,
  style: CableStyle,
  motionOn: boolean,
): CableLayer {
  if (! motionOn) {
    return "still";
  }
  if (kind === "mod") {
    return "lfo";
  }
  return style === "dots" ? "dots" : "wave";
}
