import { chipSpec } from "./chipSpec";

/** Fallback when an extra arg is not on the ChipSpec. */
const DEFAULTS: Record<string, string> = {
  y: "x",
  channel: "both",
  type: "lowpass",
  cutoff: "1000",
  resonance: "0.4",
  freq: "1000",
  q: "0.7",
  gain: "0",
  threshold: "-18",
  ratio: "4",
  attack: "0.01",
  release: "0.1",
  ceiling: "-0.3",
  time: "250",
  feedback: "0.25",
  mix: "0.3",
  sync: "off",
  pingpong: "off",
  size: "0.45",
  decay: "0.5",
  damp: "0.3",
  shape: "sine",
  depth: "1",
  f1: "200",
  f2: "2000",
  width: "1",
  delay: "12",
  bass: "140",
  sub: "1",
  up: "0",
  tone: "120",
  thresh: "0.05",
  bands: "8",
  name: "dirt",
  kanal: "both",
};

export function schemaType(type: string, args: Record<string, string> = {}): string {
  return chipSpec(type, args).id;
}

export function detailKeys(type: string, args: Record<string, string> = {}): string[] {
  return [...chipSpec(type, args).paramJacks];
}

export function defaultArg(key: string, type?: string): string {
  if (type) {
    const hit = chipSpec(type).defaultArgs[key];
    if (hit != null) {
      return hit;
    }
  }
  return DEFAULTS[key] ?? "0";
}

export function detailArgs(
  type: string,
  args: Record<string, string>,
): Array<{ key: string; value: string }> {
  const spec = chipSpec(type, args);
  const keys = spec.paramJacks;
  const seen = new Set<string>();
  const rows: Array<{ key: string; value: string }> = [];
  for (const k of keys) {
    seen.add(k);
    rows.push({ key: k, value: args[k] ?? spec.defaultArgs[k] ?? defaultArg(k) });
  }
  for (const [k, v] of Object.entries(args)) {
    if (seen.has(k)) {
      continue;
    }
    rows.push({ key: k, value: v });
  }
  return rows;
}

export function nextCustomInput(args: Record<string, string>): string {
  for (let n = 2; n < 16; n += 1) {
    const id = `in${n}`;
    if (args[id] == null) {
      return id;
    }
  }
  return "inX";
}

export function isCustomNode(type: string, id?: string): boolean {
  return schemaType(type) === "custom" || (id ?? "").toLowerCase().startsWith("custom");
}
