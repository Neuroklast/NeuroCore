import { noteSteps, parseNoteToken, wholeToHz } from "../chrome/noteValue";

export type LfoShape = "sine" | "saw" | "square" | "triangle" | "softsquare";

export type LfoKnob = { id: string; value: number; min: number; max: number; isNote?: boolean };

function isSyncOff(raw: string): boolean {
  const t = raw.trim().toLowerCase();
  return ! t || t === "off" || t === "0" || t === "false" || t === "none";
}

function knobToHz(k: LfoKnob, bpm: number): number {
  if (k.isNote) {
    const steps = noteSteps(k.min, k.max);
    if (steps.length === 0) {
      return 1;
    }
    const i = Math.max(0, Math.min(steps.length - 1, Math.round(k.value * (steps.length - 1))));
    return wholeToHz(steps[i]!.whole, bpm);
  }
  const hz = k.min + k.value * (k.max - k.min);
  return hz > 0 ? hz : 1;
}

function exprToHz(expr: string, knobs: LfoKnob[], bpm: number): number | null {
  const t = expr.trim();
  if (isSyncOff(t)) {
    return null;
  }
  const whole = parseNoteToken(t);
  if (whole != null) {
    return wholeToHz(whole, bpm);
  }
  if (/^[a-f]$/i.test(t)) {
    const k = knobs.find((x) => x.id === t.toLowerCase());
    return k ? knobToHz(k, bpm) : null;
  }
  const n = Number(t);
  return Number.isFinite(n) && n > 0 ? n : null;
}

/** Sync note at host BPM wins; otherwise free-run freq. Lamp and cable share this. */
export function resolveLfoHz(
  args: { freq?: string; sync?: string },
  knobs: LfoKnob[],
  bpm: number,
): number {
  const synced = exprToHz(args.sync ?? "", knobs, bpm);
  if (synced != null) {
    return synced;
  }
  return exprToHz(args.freq ?? "1", knobs, bpm) ?? 1;
}

export function parseLfoShape(raw: string): LfoShape {
  const s = raw.trim().toLowerCase();
  if (s.includes("soft")) {
    return "softsquare";
  }
  if (s.includes("square") || s.includes("pulse")) {
    return "square";
  }
  if (s.includes("saw")) {
    return "saw";
  }
  if (s.includes("tri")) {
    return "triangle";
  }
  return "sine";
}

export function lfoPeriodMs(hz: number): number {
  const f = Number.isFinite(hz) && hz > 0.05 ? hz : 1;
  return Math.max(50, Math.min(8000, Math.round(1000 / f)));
}

/** One cycle, 0 = dark, 1 = full lamp. */
export function lfoWave(phase01: number, shape: LfoShape): number {
  const p = ((phase01 % 1) + 1) % 1;
  if (shape === "square") {
    return p < 0.5 ? 1 : 0;
  }
  if (shape === "saw") {
    return p;
  }
  if (shape === "triangle") {
    return p < 0.5 ? p * 2 : 2 - p * 2;
  }
  if (shape === "softsquare") {
    return 0.5 + 0.5 * Math.tanh((p < 0.5 ? 1 : -1) * 2.4);
  }
  return 0.5 + 0.5 * Math.sin(p * Math.PI * 2);
}
