export type LfoShape = "sine" | "saw" | "square" | "triangle" | "softsquare";

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
