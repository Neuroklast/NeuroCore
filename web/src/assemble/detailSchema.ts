/** Editable rows every chip can expand — not only Drive. */
const KEYS: Record<string, string[]> = {
  stage: ["y", "channel"],
  custom: ["y"],
  filter: ["type", "cutoff", "resonance", "channel"],
  eq: ["type", "freq", "q", "gain", "channel"],
  comp: ["threshold", "ratio", "attack", "release"],
  gate: ["threshold", "attack", "release"],
  noisegate: ["threshold", "attack", "release"],
  limit: ["ceiling", "release"],
  delay: ["time", "feedback", "mix", "sync", "pingpong"],
  reverb: ["size", "decay", "damp", "mix"],
  osc: ["shape", "freq", "sync", "depth"],
  env: ["type", "attack", "release", "depth"],
  ott: ["depth", "time", "low", "mid", "high"],
  ir: ["mix", "gain"],
  ms: ["mode"],
  xover: ["f1", "f2"],
  widen: ["width", "delay"],
  octaver: ["sub", "up", "mix", "tone", "thresh"],
  vocoder: ["bands", "mix", "q"],
  send: ["in"],
  out: ["main"],
  in: ["gain"],
};

const DEFAULTS: Record<string, string> = {
  y: "x",
  channel: "main",
  type: "lowpass",
  cutoff: "1000",
  resonance: "0.4",
  freq: "1000",
  q: "0.7",
  gain: "1",
  threshold: "-18",
  ratio: "4",
  attack: "0.01",
  release: "0.1",
  ceiling: "-0.3",
  time: "250",
  feedback: "0.25",
  mix: "0.3",
  sync: "0",
  pingpong: "0",
  size: "0.45",
  decay: "0.5",
  damp: "0.3",
  shape: "sine",
  depth: "1",
  mode: "encode",
  f1: "200",
  f2: "2000",
  width: "1",
  delay: "12",
  sub: "1",
  up: "0",
  tone: "120",
  thresh: "0.05",
  bands: "16",
  in: "1",
  main: "1",
};

export function schemaType(type: string): string {
  const t = type.toLowerCase();
  if (t.startsWith("custom")) return "custom";
  if (t.startsWith("stage") || t.startsWith("drive")) return "stage";
  if (t.startsWith("filter")) return "filter";
  if (t.startsWith("eq")) return "eq";
  if (t.startsWith("comp")) return "comp";
  if (t.startsWith("ngate") || t.startsWith("noisegate") || t === "noise_gate") return "noisegate";
  if (t.startsWith("gate")) return "gate";
  if (t.startsWith("limit")) return "limit";
  if (t.startsWith("delay")) return "delay";
  if (t.startsWith("reverb")) return "reverb";
  if (t.startsWith("osc") || t === "lfo") return "osc";
  if (t.startsWith("env")) return "env";
  if (t === "ott") return "ott";
  if (t.startsWith("ir")) return "ir";
  if (t === "ms") return "ms";
  if (t.startsWith("xover") || t.startsWith("crossover")) return "xover";
  if (t.startsWith("widen")) return "widen";
  if (t.startsWith("octav")) return "octaver";
  if (t.startsWith("vocod")) return "vocoder";
  if (t === "send") return "send";
  if (t === "out") return "out";
  if (t === "in") return "in";
  return t;
}

export function detailKeys(type: string): string[] {
  return KEYS[schemaType(type)] ?? [];
}

export function defaultArg(key: string): string {
  return DEFAULTS[key] ?? "0";
}

export function detailArgs(
  type: string,
  args: Record<string, string>,
): Array<{ key: string; value: string }> {
  const keys = detailKeys(type);
  const seen = new Set<string>();
  const rows: Array<{ key: string; value: string }> = [];
  for (const k of keys) {
    seen.add(k);
    rows.push({ key: k, value: args[k] ?? defaultArg(k) });
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
