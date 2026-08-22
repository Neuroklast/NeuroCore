import { peakToDb } from "../bridge/telemetry";
import { CABLE_STILL_DB } from "./cableMotion";
import { isEnvNode, isLfoNode } from "./flowFromAst";

const PORT_LABEL: Record<string, string> = {
  in: "IN",
  out: "OUT",
  mid: "MID",
  side: "SIDE",
  left: "L",
  right: "R",
  l: "L",
  r: "R",
  sc: "SC",
  mod: "MOD",
  low: "LOW",
  high: "HIGH",
};

/** Hardware brackets on the contact. `[ MID ]` not `mid`. */
export function portBracket(jackId: string): string {
  const key = jackId.trim().toLowerCase();
  const label = PORT_LABEL[key] ?? (jackId.trim().toUpperCase() || "IN");
  return `[ ${label} ]`;
}

export function chipFootPeak(peak: number): string {
  const db = peakToDb(peak);
  if (! Number.isFinite(db) || db <= CABLE_STILL_DB) {
    return "—";
  }
  const n = Math.round(db);
  const sign = n > 0 ? "+" : "";
  return `${sign}${n}dB`;
}

/** One foot line from real tape values. Never a UUID. */
export function chipFootLine(id: string, peak: number): string {
  return `${id.trim()}  ${chipFootPeak(peak)}`;
}

export function overloadLabel(peak: number): string | null {
  if (! (peak >= 1)) {
    return null;
  }
  const db = peakToDb(peak);
  const sign = db >= 0 ? "+" : "";
  return `${sign}${db.toFixed(1)}dB`;
}

/** Gemini hazard: outlined Δ + bang. Not a filled header pip. */
export function clipWarnMark(): {
  viewBox: string;
  triangle: string;
  stem: string;
  fill: "none";
  dot: { cx: number; cy: number; r: number };
} {
  return {
    viewBox: "0 0 24 24",
    triangle: "12,3 22,21 2,21",
    stem: "M12 9 V15",
    fill: "none",
    dot: { cx: 12, cy: 18.2, r: 1.15 },
  };
}

export type ChipLamp = "clip" | "lfo" | "env";

/** Closed DSP plate: identity + one lamp. No tech-demo greeble. */
export function closedChipChrome(type: string, id = ""): {
  greeble: boolean;
  barcode: boolean;
  grip: boolean;
  fakeJacks: boolean;
  lamp: ChipLamp;
} {
  const env = isEnvNode({ type, id });
  const lfo = isLfoNode({ type, id });
  return {
    greeble: false,
    barcode: false,
    grip: false,
    fakeJacks: false,
    lamp: env ? "env" : lfo ? "lfo" : "clip",
  };
}

/** Overlay lists the same keys. South captions must not sit under it. */
export function overlayHidesBindRail(detail: boolean): boolean {
  return detail;
}

export function bindRailVisible(detail: boolean, keys: readonly string[]): boolean {
  return keys.length > 0 && ! overlayHidesBindRail(detail);
}
