import { themeOf } from "./theme";

const signal = themeOf("signal");

/** Product copy + default (signal) swatches. Live paint uses the theme engine. */
export const nk = {
  accent: signal.accent,
  accentDim: signal.accentDim,
  warn: signal.warn,
  cyan: signal.cyan,
  surface: signal.surface,
  surfaceHigh: signal.surfaceHigh,
  background: signal.background,
  ink: signal.ink,
  inkMuted: signal.inkMuted,
  inkSoft: signal.inkSoft,
  error: signal.error,
  panelBorder: signal.panelBorder,
  gridLine: `rgba(${signal.accentRgb}, 0.16)`,
  well: signal.well,
  version: "0.4.10-alpha",
  product: "NEUROKORE",
  company: "Neuroklast",
  byline: "by Neuroklast",
  osBanner: "NEUROKORE // NEUROKLAST OS",
} as const;

export function kindLabel(type: string): string {
  const t = type.toLowerCase();
  if (t.startsWith("filter")) return "FILTER";
  if (t === "eq") return "EQ";
  if (t.startsWith("custom")) return "CUSTOM";
  if (t.startsWith("stage")) return "DRIVE";
  if (t.startsWith("comp")) return "COMP";
  if (t.startsWith("ngate") || t.startsWith("noisegate") || t === "noise_gate") return "NGATE";
  if (t.startsWith("gate")) return "GATE";
  if (t.startsWith("limit")) return "LIMIT";
  if (t.startsWith("delay")) return "DELAY";
  if (t.startsWith("reverb")) return "REVERB";
  if (t.startsWith("ir")) return "CAB";
  if (t === "ott") return "OTT";
  if (t.startsWith("widen") || t === "width") return "WIDTH";
  if (t === "ms") return "MS";
  if (t === "bus") return "BUS";
  if (t === "send") return "SEND";
  if (t.startsWith("xover") || t.startsWith("crossover") || t === "msplit") return "MB SPLIT";
  if (t === "out") return "OUT";
  if (t === "in") return "IN";
  if (t === "sidechain" || t === "sc" || t === "scin") return "SC IN";
  if (t.startsWith("octav")) return "OCT";
  if (t.startsWith("pitch")) return "PITCH";
  if (t.startsWith("vocod")) return "VOC";
  if (t.startsWith("env")) return "ENV";
  if (t.startsWith("osc")) return "LFO";
  return t.toUpperCase().slice(0, 6);
}

export function mappedValue(value01: number, min: number, max: number): number {
  return min + value01 * (max - min);
}

export function formatMapped(v: number): string {
  if (! Number.isFinite(v)) {
    return "0";
  }
  if (Math.abs(v) >= 100) {
    return v.toFixed(1);
  }
  return v.toFixed(3);
}

export function formatBound(v: number): string {
  if (! Number.isFinite(v)) {
    return "0";
  }
  if (Math.abs(v - Math.round(v)) < 1e-4 && Math.abs(v) < 1e7) {
    return String(Math.round(v));
  }
  return v.toFixed(2);
}
