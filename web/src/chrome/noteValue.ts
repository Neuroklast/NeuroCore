/** Display text only — never round audio-thread DSP / SignalChain samples. */
export function round2(n: number): string {
  if (! Number.isFinite(n)) {
    return "0.00";
  }
  return (Math.round(n * 100) / 100).toFixed(2);
}

/** Mapped value → readout. Unit `%` means 0..1 → percent. */
export function formatKnobDisplay(mapped: number, unit?: string): string {
  if (unit === "%") {
    return `${round2(mapped * 100)}%`;
  }
  const body = round2(mapped);
  return unit ? `${body} ${unit}` : body;
}

/** Whole-note fractions. `1/4` = 0.25 of a bar. Matches `dsl::NoteValues`. */
export const NOTE_GRID: Array<{ whole: number; label: string }> = [
  { whole: 1, label: "1/1" },
  { whole: 0.75, label: "1/2." },
  { whole: 0.5, label: "1/2" },
  { whole: 0.375, label: "1/4." },
  { whole: 1 / 3, label: "1/3" },
  { whole: 0.25, label: "1/4" },
  { whole: 0.1875, label: "1/8." },
  { whole: 1 / 6, label: "1/6" },
  { whole: 0.125, label: "1/8" },
  { whole: 0.09375, label: "1/16." },
  { whole: 1 / 12, label: "1/12" },
  { whole: 0.0625, label: "1/16" },
];

const EPS = 1e-3;

export function parseNoteToken(raw: string): number | null {
  let s = raw.trim().toLowerCase();
  if (! s) {
    return null;
  }
  let dotted = false;
  let triplet = false;
  if (s.endsWith(".") || s.endsWith("d")) {
    dotted = true;
    s = s.slice(0, -1).trim();
  } else if (s.endsWith("t")) {
    triplet = true;
    s = s.slice(0, -1).trim();
  }
  let whole = 0;
  if (s === "bar" || s === "1bar") {
    whole = 1;
  } else {
    const slash = s.indexOf("/");
    if (slash <= 0) {
      return null;
    }
    const num = Number(s.slice(0, slash).trim());
    const den = Number(s.slice(slash + 1).trim());
    if (! den || ! Number.isFinite(num) || ! Number.isFinite(den)) {
      return null;
    }
    whole = num / den;
  }
  if (dotted) {
    whole *= 1.5;
  }
  if (triplet) {
    whole *= 2 / 3;
  }
  return whole > 0 && Number.isFinite(whole) ? whole : null;
}

export function labelForWhole(whole: number): string {
  const hit = NOTE_GRID.find((g) => Math.abs(g.whole - whole) < EPS);
  return hit?.label ?? `1/${Math.max(1, Math.round(1 / whole))}`;
}

export function wholeToMs(whole: number, bpm: number): number {
  const b = bpm > 1 ? bpm : 120;
  return (60000 / b) * whole * 4;
}

export function wholeToHz(whole: number, bpm: number): number {
  const b = bpm > 1 ? bpm : 120;
  const beats = whole * 4;
  return beats > 0 ? (b / 60) / beats : 0;
}

/** Note display is opt-in via `isNote`. Numeric ranges stay numeric. */
export function noteUnitForRange(_min: number, _max: number): "ms" | "hz" | null {
  return null;
}

export function typedToMapped(
  text: string,
  min: number,
  max: number,
  _bpm: number,
  isNote: boolean,
): number | null {
  const whole = parseNoteToken(text);
  if (whole == null) {
    const n = Number(text.trim().replace(",", "."));
    return Number.isFinite(n) ? Math.min(Math.max(n, Math.min(min, max)), Math.max(min, max)) : null;
  }
  if (isNote) {
    return wholeToNoteNorm(whole, min, max);
  }
  return null;
}

export function mappedToNorm(mapped: number, min: number, max: number): number {
  if (Math.abs(max - min) < 1e-12) {
    return 0;
  }
  return Math.max(0, Math.min(1, (mapped - min) / (max - min)));
}

export function wholeToNoteNorm(whole: number, min: number, max: number): number {
  const steps = noteSteps(min, max);
  if (steps.length <= 1) {
    return 0;
  }
  let best = 0;
  let err = Number.POSITIVE_INFINITY;
  steps.forEach((s, i) => {
    const e = Math.abs(s.whole - whole);
    if (e < err) {
      err = e;
      best = i;
    }
  });
  return best / (steps.length - 1);
}

export function noteSteps(min: number, max: number): Array<{ whole: number; label: string }> {
  const lo = Math.min(min, max);
  const hi = Math.max(min, max);
  const steps = NOTE_GRID.filter((g) => g.whole + EPS >= lo && g.whole - EPS <= hi);
  return min <= max ? steps.slice().reverse() : steps;
}

export function noteLabelForMapped(
  mapped: number,
  min: number,
  max: number,
  _bpm: number,
  isNote: boolean,
  norm01?: number,
): string | null {
  if (isNote) {
    const steps = noteSteps(min, max);
    if (steps.length === 0) {
      return labelForWhole(mapped);
    }
    const n = Math.max(0, Math.min(1, norm01 ?? mappedToNorm(mapped, min, max)));
    const i = Math.max(0, Math.min(steps.length - 1, Math.round(n * (steps.length - 1))));
    return steps[i]!.label;
  }
  return null;
}

export function formatNoteBound(min: number, max: number, end: "min" | "max", isNote: boolean): string | null {
  if (! isNote) {
    return null;
  }
  return labelForWhole(end === "min" ? min : max);
}
