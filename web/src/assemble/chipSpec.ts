/** One catalog for circuit chips: jacks, enums, typecode, min body. */

export type ChipRange = { min: number; max: number; unit?: string };

export type ChipSpec = {
  id: string;
  label: string;
  typeCodePrefix: string;
  audioIns: string[];
  audioOuts: string[];
  paramJacks: string[];
  paramJackLabels: Record<string, string>;
  modIns: string[];
  enums: Record<string, string[]>;
  ranges: Record<string, ChipRange>;
  defaultArgs: Record<string, string>;
  blurb: string;
  minBodyPx: number;
};

export type ParamJackSlot = {
  key: string;
  label: string;
  x: number;
  y: number;
};

export const TITLE_H = 26;
export const TYPECODE_H = 14;
export const SOCKET_H = 40;
const BODY_PAD = 10;
const SOUTH_BAND = 16;
const ENUM_EXTRA = 8;
const BODY_FLOOR = 80;
const IO_BODY = 56;
const JACK_PITCH = 24;
const PARAM_JACK_EDGE = 10;

const CHANNEL = ["both", "left", "right", "mid", "side"] as const;
const NOTES = [
  "off", "1/1", "1/2.", "1/2", "1/4.", "1/3", "1/4",
  "1/8.", "1/6", "1/8", "1/16.", "1/12", "1/16",
] as const;

const ENUM_ABBR: Record<string, string> = {
  lowpass: "LP", lpf: "LP", lp: "LP",
  highpass: "HP", hpf: "HP", hp: "HP",
  bandpass: "BP", bpf: "BP", bp: "BP",
  peak: "PK",
  lowshelf: "LS",
  highshelf: "HS",
  notch: "NT",
  lowcut: "LC",
  highcut: "HC",
  sine: "SI",
  triangle: "TR",
  square: "SQ",
  saw: "SW",
  noise: "NS",
  both: "BO",
  left: "L",
  right: "R",
  mid: "M",
  side: "S",
  env: "EN",
  off: "OFF",
  on: "ON",
  rms: "RMS",
};

type Draft = Omit<ChipSpec, "minBodyPx" | "paramJackLabels"> & {
  paramJackLabels?: Record<string, string>;
};

export function computeMinBodyPx(spec: {
  id: string;
  paramJacks: string[];
  enums: Record<string, string[]>;
  audioIns: string[];
  audioOuts: string[];
}): number {
  if (spec.id === "in" || spec.id === "out" || spec.id === "sidechain") {
    return IO_BODY;
  }
  const n = spec.paramJacks.length;
  const enumRows = spec.paramJacks.filter((k) => (spec.enums[k] ?? []).length > 0).length;
  const sides = Math.max(spec.audioIns.length, spec.audioOuts.length, 1);
  const header = TITLE_H + TYPECODE_H + BODY_PAD;
  const sockets = n * SOCKET_H;
  const south = n === 0 ? 0 : SOUTH_BAND;
  const sideBand = sides <= 1 ? BODY_FLOOR : 36 + sides * JACK_PITCH;
  return Math.max(BODY_FLOOR, header + sockets + enumRows * ENUM_EXTRA + south, sideBand);
}

function spec(init: Draft): ChipSpec {
  const paramJackLabels = init.paramJackLabels
    ?? Object.fromEntries(init.paramJacks.map((k) => [k, k]));
  const draft = {
    ...init,
    paramJackLabels,
    modIns: init.modIns ?? [],
    enums: init.enums ?? {},
    ranges: init.ranges ?? {},
    defaultArgs: init.defaultArgs ?? {},
  };
  return { ...draft, minBodyPx: computeMinBodyPx(draft) };
}

const SPECS: Record<string, ChipSpec> = {};

function add(init: Draft): void {
  SPECS[init.id] = spec(init);
}

add({
  id: "filter",
  label: "Filter",
  typeCodePrefix: "FL",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["type", "cutoff", "resonance", "channel"],
  modIns: [],
  enums: {
    type: ["lowpass", "highpass", "bandpass"],
    channel: [...CHANNEL],
  },
  ranges: {
    cutoff: { min: 20, max: 20000, unit: "Hz" },
    resonance: { min: 0.1, max: 10 },
  },
  defaultArgs: { type: "lowpass", cutoff: "1000", resonance: "0.4", channel: "both" },
  blurb: "State-variable filter, cutoff in Hz.",
});

add({
  id: "eq",
  label: "EQ",
  typeCodePrefix: "EQ",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["type", "freq", "q", "gain", "channel"],
  enums: {
    type: ["peak", "lowshelf", "highshelf", "lowpass", "highpass"],
    channel: [...CHANNEL],
  },
  ranges: {
    freq: { min: 20, max: 20000, unit: "Hz" },
    q: { min: 0.1, max: 12 },
    gain: { min: -24, max: 24, unit: "dB" },
  },
  defaultArgs: { type: "peak", freq: "1000", q: "0.7", gain: "0", channel: "both" },
  blurb: "Parametric EQ band.",
});

add({
  id: "delay",
  label: "Delay",
  typeCodePrefix: "DL",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["time", "feedback", "mix", "sync", "pingpong"],
  enums: {
    sync: [...NOTES],
    pingpong: ["off", "on"],
  },
  ranges: {
    time: { min: 1, max: 2000, unit: "ms" },
    feedback: { min: 0, max: 0.95 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { time: "250", feedback: "0.25", mix: "0.3", sync: "off", pingpong: "off" },
  blurb: "Delay line, time in ms.",
});

add({
  id: "reverb",
  label: "Reverb",
  typeCodePrefix: "RV",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["size", "decay", "damp", "mix"],
  ranges: {
    size: { min: 0, max: 1 },
    decay: { min: 0, max: 1 },
    damp: { min: 0, max: 1 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { size: "0.45", decay: "0.5", damp: "0.3", mix: "0.3" },
  blurb: "Room, mix in percent.",
});

add({
  id: "stage",
  label: "Drive",
  typeCodePrefix: "DR",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["y", "channel"],
  enums: { channel: [...CHANNEL] },
  defaultArgs: { y: "x", channel: "both" },
  blurb: "Formula stage.",
});

add({
  id: "custom",
  label: "Custom",
  typeCodePrefix: "CU",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["y"],
  defaultArgs: { y: "x" },
  blurb: "Formula plus extra inputs.",
});

add({
  id: "comp",
  label: "Comp",
  typeCodePrefix: "CP",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["threshold", "ratio", "attack", "release"],
  ranges: {
    threshold: { min: -60, max: 0, unit: "dB" },
    ratio: { min: 1, max: 20 },
    attack: { min: 0.001, max: 1, unit: "ms" },
    release: { min: 0.001, max: 2, unit: "ms" },
  },
  defaultArgs: { threshold: "-18", ratio: "4", attack: "0.01", release: "0.1" },
  blurb: "Compressor.",
});

add({
  id: "noisegate",
  label: "Gate",
  typeCodePrefix: "GT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["threshold", "attack", "release"],
  ranges: {
    threshold: { min: -80, max: 0, unit: "dB" },
    attack: { min: 0.001, max: 1, unit: "ms" },
    release: { min: 0.001, max: 2, unit: "ms" },
  },
  defaultArgs: { threshold: "-18", attack: "0.01", release: "0.1" },
  blurb: "Noise gate.",
});

add({
  id: "limit",
  label: "Limit",
  typeCodePrefix: "LM",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["ceiling", "release"],
  ranges: {
    ceiling: { min: -12, max: 0, unit: "dB" },
    release: { min: 0.001, max: 1, unit: "ms" },
  },
  defaultArgs: { ceiling: "-0.3", release: "0.1" },
  blurb: "Brickwall limiter.",
});

add({
  id: "ott",
  label: "OTT",
  typeCodePrefix: "OT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["depth", "time", "low", "mid", "high"],
  ranges: {
    depth: { min: 0, max: 1, unit: "%" },
    time: { min: 0, max: 1 },
    low: { min: 0, max: 1.4, unit: "%" },
    mid: { min: 0, max: 1.4, unit: "%" },
    high: { min: 0, max: 1.4, unit: "%" },
  },
  defaultArgs: { depth: "1", time: "0.3", low: "1", mid: "1", high: "1" },
  blurb: "Multiband up/down compression.",
});

add({
  id: "ir",
  label: "IR",
  typeCodePrefix: "IR",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["mix", "gain"],
  ranges: {
    mix: { min: 0, max: 1, unit: "%" },
    gain: { min: -24, max: 24, unit: "dB" },
  },
  defaultArgs: { mix: "0.3", gain: "0" },
  blurb: "Convolution cab.",
});

add({
  id: "env",
  label: "Env",
  typeCodePrefix: "EV",
  audioIns: ["sc"],
  audioOuts: ["mod"],
  paramJacks: ["type", "attack", "release", "depth"],
  enums: { type: ["peak", "rms"] },
  ranges: {
    attack: { min: 0.001, max: 1, unit: "ms" },
    release: { min: 0.001, max: 2, unit: "ms" },
    depth: { min: 0, max: 1 },
  },
  defaultArgs: { type: "peak", attack: "0.01", release: "0.1", depth: "1" },
  blurb: "Envelope follower, optional sidechain in.",
});

add({
  id: "osc",
  label: "LFO",
  typeCodePrefix: "LF",
  audioIns: [],
  audioOuts: ["mod"],
  paramJacks: ["shape", "freq", "sync", "depth"],
  enums: {
    shape: ["sine", "triangle", "square", "saw", "noise"],
    sync: [...NOTES],
  },
  ranges: {
    freq: { min: 0.01, max: 40, unit: "Hz" },
    depth: { min: 0, max: 1 },
  },
  defaultArgs: { shape: "sine", freq: "1", sync: "off", depth: "1" },
  blurb: "LFO, no audio in. Bind only.",
});

add({
  id: "octaver",
  label: "Octaver",
  typeCodePrefix: "OC",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["sub", "up", "mix", "tone", "thresh"],
  ranges: {
    sub: { min: 0, max: 1.5 },
    up: { min: 0, max: 1.5 },
    mix: { min: 0, max: 1, unit: "%" },
    tone: { min: 20, max: 4000, unit: "Hz" },
    thresh: { min: 0.01, max: 0.25 },
  },
  defaultArgs: { sub: "1", up: "0", mix: "0.3", tone: "120", thresh: "0.05" },
  blurb: "Analog octave.",
});

add({
  id: "vocoder",
  label: "Vocoder",
  typeCodePrefix: "VC",
  audioIns: ["in", "voice"],
  audioOuts: ["out"],
  paramJacks: ["bands", "mix", "q"],
  ranges: {
    bands: { min: 3, max: 8 },
    mix: { min: 0, max: 1, unit: "%" },
    q: { min: 0.7, max: 8 },
  },
  defaultArgs: { bands: "8", mix: "0.3", q: "2.2" },
  blurb: "Carrier in plus voice/sidechain.",
});

add({
  id: "width",
  label: "Width",
  typeCodePrefix: "WD",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["width", "delay", "bass"],
  ranges: {
    width: { min: 0, max: 1.4 },
    delay: { min: 0, max: 40, unit: "ms" },
    bass: { min: 20, max: 400, unit: "Hz" },
  },
  defaultArgs: { width: "1", delay: "12", bass: "140" },
  blurb: "Stereo width.",
});

add({
  id: "split_ms",
  label: "Split Mid/Side",
  typeCodePrefix: "SM",
  audioIns: ["in"],
  audioOuts: ["mid", "side"],
  paramJacks: [],
  blurb: "Encode mid/side.",
});

add({
  id: "join_ms",
  label: "Join Mid/Side",
  typeCodePrefix: "JM",
  audioIns: ["mid", "side"],
  audioOuts: ["out"],
  paramJacks: [],
  blurb: "Decode mid/side.",
});

add({
  id: "split_lr",
  label: "Split Left/Right",
  typeCodePrefix: "SL",
  audioIns: ["in"],
  audioOuts: ["left", "right"],
  paramJacks: [],
  blurb: "Split to left/right.",
});

add({
  id: "join_lr",
  label: "Join Left/Right",
  typeCodePrefix: "JL",
  audioIns: ["left", "right"],
  audioOuts: ["out"],
  paramJacks: [],
  blurb: "Join left/right.",
});

add({
  id: "msplit",
  label: "Multiband Split",
  typeCodePrefix: "MB",
  audioIns: ["in"],
  audioOuts: ["low", "mid", "high"],
  paramJacks: ["f1", "f2"],
  ranges: {
    f1: { min: 40, max: 800, unit: "Hz" },
    f2: { min: 200, max: 8000, unit: "Hz" },
  },
  defaultArgs: { f1: "200", f2: "2000" },
  blurb: "Crossover split.",
});

add({
  id: "bus",
  label: "Bus",
  typeCodePrefix: "BS",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["name"],
  defaultArgs: { name: "bus" },
  blurb: "Named rail.",
});

add({
  id: "join",
  label: "Join Signal",
  typeCodePrefix: "JN",
  audioIns: ["inA", "inB"],
  audioOuts: ["out"],
  paramJacks: ["mix"],
  ranges: { mix: { min: 0, max: 1, unit: "%" } },
  defaultArgs: { mix: "0.5" },
  blurb: "Mix two inputs.",
});

add({
  id: "send",
  label: "Send",
  typeCodePrefix: "SD",
  audioIns: ["in"],
  audioOuts: ["out", "ctrl"],
  paramJacks: ["kanal"],
  enums: { kanal: ["both", "left", "right", "mid", "side", "env"] },
  defaultArgs: { kanal: "both" },
  blurb: "Send plus control out on the south edge.",
});

add({
  id: "in",
  label: "IN",
  typeCodePrefix: "IN",
  audioIns: [],
  audioOuts: ["out"],
  paramJacks: [],
  blurb: "Locked input.",
});

add({
  id: "sidechain",
  label: "Sidechain",
  typeCodePrefix: "SC",
  audioIns: [],
  audioOuts: ["out"],
  paramJacks: [],
  blurb: "Host sidechain input.",
});

add({
  id: "out",
  label: "OUT",
  typeCodePrefix: "OU",
  audioIns: ["in"],
  audioOuts: [],
  paramJacks: [],
  blurb: "Output, mix-ins optional.",
});

export function resolveChipId(type: string, args: Record<string, string> = {}): string {
  const t = type.toLowerCase();
  if (t.startsWith("custom")) return "custom";
  if (t.startsWith("stage") || t.startsWith("drive")) return "stage";
  if (t.startsWith("filter")) return "filter";
  if (t.startsWith("eq")) return "eq";
  if (t.startsWith("comp")) return "comp";
  if (t.startsWith("ngate") || t.startsWith("noisegate") || t === "noise_gate") return "noisegate";
  if (t.startsWith("gate")) return "noisegate";
  if (t.startsWith("limit")) return "limit";
  if (t.startsWith("delay")) return "delay";
  if (t.startsWith("reverb")) return "reverb";
  if (t.startsWith("osc") || t === "lfo") return "osc";
  if (t.startsWith("env")) return "env";
  if (t === "ott") return "ott";
  if (t.startsWith("ir")) return "ir";
  if (t === "split_ms") return "split_ms";
  if (t === "join_ms") return "join_ms";
  if (t === "ms") {
    const mode = (args.mode ?? args.encode ?? "").trim().toLowerCase();
    const fam = (args.family ?? args.rails ?? "").trim().toLowerCase();
    const isLr = fam === "lr" || fam === "leftright" || fam === "l/r"
      || mode === "split_lr" || mode === "join_lr" || mode === "lr_split" || mode === "lr_join";
    const isJoin = mode === "decode" || mode === "join" || mode === "lr" || mode === "stereo"
      || mode === "to_lr" || mode === "join_lr" || mode === "lr_join";
    if (isLr) {
      return isJoin ? "join_lr" : "split_lr";
    }
    return isJoin ? "join_ms" : "split_ms";
  }
  if (t.startsWith("xover") || t.startsWith("crossover") || t === "msplit") return "msplit";
  if (t.startsWith("widen") || t === "width") return "width";
  if (t.startsWith("octav")) return "octaver";
  if (t.startsWith("vocod")) return "vocoder";
  if (t === "send") return "send";
  if (t === "out") return "out";
  if (t === "in") return "in";
  if (t === "sidechain" || t === "sc" || t === "scin") return "sidechain";
  if (t === "split_lr") return "split_lr";
  if (t === "join_lr") return "join_lr";
  if (t === "bus") return "bus";
  if (t === "join") return "join";
  return t;
}

function fallbackSpec(id: string): ChipSpec {
  return spec({
    id,
    label: id,
    typeCodePrefix: id.replace(/[^a-z0-9]/gi, "").slice(0, 2).toUpperCase() || "XX",
    audioIns: ["in"],
    audioOuts: ["out"],
    paramJacks: [],
    modIns: [],
    enums: {},
    ranges: {},
    defaultArgs: {},
    blurb: "",
  });
}

export function chipSpec(type: string, args: Record<string, string> = {}): ChipSpec {
  const id = resolveChipId(type, args);
  return SPECS[id] ?? fallbackSpec(id);
}

export function allChipSpecs(): ChipSpec[] {
  return Object.values(SPECS);
}

function enumToken(raw: string): string {
  const k = raw.trim().toLowerCase();
  if (ENUM_ABBR[k]) {
    return ENUM_ABBR[k];
  }
  if (k.startsWith("band")) return "BP";
  if (k.startsWith("highp")) return "HP";
  if (k.startsWith("lowp")) return "LP";
  if (k.startsWith("tri")) return "TR";
  return k.replace(/[^a-z0-9]/gi, "").slice(0, 3).toUpperCase() || k.toUpperCase();
}

function formatCodeValue(raw: string): string {
  const t = raw.trim();
  const n = Number(t);
  if (! Number.isFinite(n)) {
    return t.toUpperCase().slice(0, 6);
  }
  if (Math.abs(n - Math.round(n)) < 1e-6) {
    return String(Math.round(n));
  }
  return n.toFixed(2);
}

const VALUE_PREFER = ["cutoff", "freq", "width", "time", "threshold", "mix", "size", "y"];

function primaryValueKey(spec: ChipSpec): string | undefined {
  for (const k of VALUE_PREFER) {
    if (spec.paramJacks.includes(k) && spec.ranges[k]) {
      return k;
    }
  }
  return spec.paramJacks.find((k) => spec.ranges[k]);
}

export function typeCode(spec: ChipSpec, args: Record<string, string> = {}): string {
  const parts = [spec.typeCodePrefix];
  const enumKey = spec.paramJacks.find((k) => spec.enums[k]);
  if (enumKey) {
    const raw = args[enumKey] ?? spec.defaultArgs[enumKey] ?? "";
    if (raw) {
      parts.push(enumToken(raw));
    }
  }
  const valueKey = primaryValueKey(spec);
  if (valueKey) {
    const raw = args[valueKey] ?? spec.defaultArgs[valueKey] ?? "";
    if (raw) {
      parts.push(formatCodeValue(raw));
    }
  }
  return parts.join("-");
}

export function collapsedFace(
  typeOrSpec: string | ChipSpec,
  args: Record<string, string> = {},
): { title: string; code: string } {
  const spec = typeof typeOrSpec === "string" ? chipSpec(typeOrSpec, args) : typeOrSpec;
  return { title: spec.label.toUpperCase(), code: typeCode(spec, args) };
}

export function paramJackSlots(spec: ChipSpec, box: { w: number; h: number }): ParamJackSlot[] {
  const n = spec.paramJacks.length;
  if (n === 0) {
    return [];
  }
  const pad = 36;
  const xs = n === 1
    ? [box.w * 0.5]
    : Array.from({ length: n }, (_, i) => pad + ((i + 0.5) / n) * Math.max(12, box.w - pad * 2));
  const y = box.h - PARAM_JACK_EDGE;
  return spec.paramJacks.map((key, i) => ({
    key,
    label: spec.paramJackLabels[key] || key,
    x: xs[i] ?? box.w * 0.5,
    y,
  }));
}
