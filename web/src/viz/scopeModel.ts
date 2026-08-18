export type ScopeSource = "in" | "out" | "both";
export type ScopeXScale = "samples" | "time" | "freq";
export type ScopeYScale = "linear" | "db";

export const SCOPE_COLOR = {
  in: "#00f0ff",
  out: "#ff003c",
  delta: "#f0c040",
} as const;

export interface ScopeTrace {
  id: "in" | "out" | "delta";
  color: string;
  samples: Float32Array;
}

export function deltaSamples(out: ArrayLike<number>, inn: ArrayLike<number>, n: number): Float32Array {
  const dest = new Float32Array(n);
  for (let i = 0; i < n; i += 1) {
    dest[i] = (out[i] ?? 0) - (inn[i] ?? 0);
  }
  return dest;
}

/** Pixel 0 is sample 0, last pixel is the last shown sample. */
export function sampleAtPx(samples: ArrayLike<number>, n: number, px: number, width: number): number {
  const count = Math.max(2, n);
  const span = Math.max(1, width - 1);
  const t = (Math.max(0, Math.min(span, px)) / span) * (count - 1);
  const i0 = Math.floor(t);
  const i1 = Math.min(count - 1, i0 + 1);
  const f = t - i0;
  return ((samples[i0] ?? 0) * (1 - f)) + ((samples[i1] ?? 0) * f);
}

export function tracesFor(
  source: ScopeSource,
  delta: boolean,
  scopeIn: ArrayLike<number>,
  scopeOut: ArrayLike<number>,
  n: number,
): ScopeTrace[] {
  const traces: ScopeTrace[] = [];
  if (source === "in" || source === "both") {
    traces.push({ id: "in", color: SCOPE_COLOR.in, samples: Float32Array.from(scopeIn).subarray(0, n) });
  }
  if (source === "out" || source === "both") {
    traces.push({ id: "out", color: SCOPE_COLOR.out, samples: Float32Array.from(scopeOut).subarray(0, n) });
  }
  if (delta) {
    traces.push({ id: "delta", color: SCOPE_COLOR.delta, samples: deltaSamples(scopeOut, scopeIn, n) });
  }
  return traces;
}

export function scopeTitle(source: ScopeSource, delta: boolean): string {
  const head = source === "in" ? "IN // PRE" : source === "out" ? "OUT // POST" : "IN+OUT";
  return delta ? `${head} + Δ` : head;
}

export function fieldTitle(source: ScopeSource): string {
  if (source === "in") {
    return "IN FIELD";
  }
  if (source === "out") {
    return "OUT FIELD";
  }
  return "FIELD";
}

export function loudTitle(source: ScopeSource): string {
  if (source === "in") {
    return "IN LU";
  }
  if (source === "out") {
    return "OUT LU";
  }
  return "LU";
}

/** Goniometer: x = side, y = −mid (up is in-phase). Same as native ScopeAnalytics. */
export function gonioPoint(l: number, r: number): { x: number; y: number } {
  const left = Number.isFinite(l) ? l : 0;
  const right = Number.isFinite(r) ? r : 0;
  return { x: 0.5 * (left - right), y: -0.5 * (left + right) };
}

export function nextProcessMode(mode: string): "STUDIO" | "LIVE" {
  return mode === "LIVE" ? "STUDIO" : "LIVE";
}

export function processModeIndex(mode: string): number {
  return mode === "LIVE" ? 1 : 0;
}

export const SCOPE_MENU = {
  x: [
    { id: "samples", label: "Scale Samples" },
    { id: "time", label: "Scale Time" },
    { id: "freq", label: "Scale Frequency" },
  ],
  y: [
    { id: "linear", label: "Scale Linear" },
    { id: "db", label: "Scale Decibel" },
  ],
  flags: [
    { id: "grid", label: "Toggle Grid" },
    { id: "invertY", label: "Toggle Invert Y" },
    { id: "delta", label: "Toggle Delta" },
  ],
  source: [
    { id: "in", label: "Input" },
    { id: "out", label: "Output" },
    { id: "both", label: "Both" },
  ],
} as const;
