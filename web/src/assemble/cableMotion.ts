import { lfoPeriodMs } from "./lfoLamp";
import { logAmp, PLASMA_PITCH, PLASMA_SPAN } from "./tubeModel";

export type CableKind = "audio" | "mod";
export type CableStyle = "wave" | "dots";
export type CableLayer = "lfo" | "wave" | "dots" | "still";

/** One LFO period = one pulse trip down the cable. */
export function lfoChaseMs(hz: number): number {
  return lfoPeriodMs(hz);
}

/** Thin cyan wire. Knob-arc glow sits on the moving dot, not the tube. */
export const LFO_WIRE = 1.35;
export const LFO_DOT = 9;

/** Geometric length of an M/L path, not the SVG string length. */
export function svgPathLength(d: string): number {
  const pts: Array<{ x: number; y: number }> = [];
  const re = /[ML]\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)/gi;
  let m: RegExpExecArray | null = re.exec(d);
  while (m) {
    pts.push({ x: Number(m[1]), y: Number(m[2]) });
    m = re.exec(d);
  }
  let n = 0;
  for (let i = 1; i < pts.length; i += 1) {
    n += Math.hypot(pts[i]!.x - pts[i - 1]!.x, pts[i]!.y - pts[i - 1]!.y);
  }
  return n;
}

/** One blob, gap ≥ path so a second pulse cannot share the wire. trip = path so one cycle = one crossing. */
export function lfoDash(pathLen: number, blob = LFO_DOT): { dash: string; cycle: number; trip: number } {
  const gap = Math.max(1, pathLen);
  return { dash: `${blob} ${gap}`, cycle: blob + gap, trip: gap };
}

/** Knob-arc cyan intensity. Depth 0 is a faint ember, 1 is full glow. */
export function lfoDotGlow(amp: number): number {
  return 0.28 + 0.72 * logAmp(amp);
}

export function resolveAmp(
  expr: string,
  knobs: Array<{ id: string; value: number }>,
): number {
  const t = expr.trim();
  const n = Number(t);
  if (Number.isFinite(n)) {
    return Math.max(0, Math.min(1, n));
  }
  if (/^[a-f]$/i.test(t)) {
    const k = knobs.find((x) => x.id === t.toLowerCase());
    if (k) {
      return Math.max(0, Math.min(1, k.value));
    }
  }
  return 1;
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
