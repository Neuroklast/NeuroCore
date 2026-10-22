import { entryById } from "./blockCatalog";
import { BOARD_GRID } from "./grid";
import {
  FORK_PITCH,
  JACK_PITCH,
  SOCKET_H,
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  dspFaceSize,
  ioFaceSize,
  isUtilityIo,
  sideJackPitch,
} from "./chipMetrics";

export {
  FORK_PITCH,
  JACK_PITCH,
  SOCKET_H,
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  sideJackPitch,
};

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



const CHANNEL = ["both", "left", "right", "mid", "side"] as const;
const NOTES = [
  "off", "1/1", "1/2.", "1/2", "1/4.", "1/3", "1/4",
  "1/8.", "1/6", "1/8", "1/16.", "1/12", "1/16",
] as const;

const ENUM_ABBR: Record<string, string> = {
  lowpass: "LP", lpf: "LP", lp: "LP",
  highpass: "HP", hpf: "HP", hp: "HP",
  bandpass: "BP", bpf: "BP", bp: "BP",
  allpass: "AP", apf: "AP", ap: "AP",
  db: "DB", dbfs: "DB", lin: "LN",
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

type Draft = Omit<ChipSpec, "minBodyPx" | "paramJackLabels" | "modIns" | "enums" | "ranges" | "defaultArgs"> & {
  paramJackLabels?: Record<string, string>;
  modIns?: string[];
  enums?: Record<string, string[]>;
  ranges?: Record<string, ChipRange>;
  defaultArgs?: Record<string, string>;
};

const ADVANCED_KEYS = new Set(["channel", "kanal", "name"]);

export function essentialParamKeys(spec: { paramJacks: string[] }): string[] {
  return spec.paramJacks.filter((k) => ! ADVANCED_KEYS.has(k));
}

export function advancedParamKeys(spec: { paramJacks: string[] }): string[] {
  return spec.paramJacks.filter((k) => ADVANCED_KEYS.has(k));
}

export function faceParamKeys(_spec: { paramJacks: string[] }): string[] {
  return [];
}

export function overlayParamKeys(spec: { paramJacks: string[] }): string[] {
  return [...spec.paramJacks];
}

export function computeMinBodyPx(spec: {
  id: string;
  paramJacks: string[];
  enums: Record<string, string[]>;
  audioIns: string[];
  audioOuts: string[];
}): number {
  return isUtilityIo(spec.id) ? ioFaceSize().h : dspFaceSize().h;
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
    type: ["lowpass", "highpass", "bandpass", "allpass"],
    channel: [...CHANNEL],
  },
  ranges: {
    cutoff: { min: 20, max: 20000, unit: "Hz" },
    resonance: { min: 0.1, max: 4 },
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
    type: ["peak", "notch", "lowcut", "highcut", "lowshelf", "highshelf"],
    channel: [...CHANNEL],
  },
  ranges: {
    freq: { min: 20, max: 20000, unit: "Hz" },
    q: { min: 0.1, max: 12 },
    gain: { min: -24, max: 24, unit: "dB" },
  },
  defaultArgs: { type: "peak", freq: "1000", q: "0.7", gain: "0", channel: "both" },
  blurb: "Parametric EQ band. lowcut/highcut map to HighCut/LowCut on the DSP side.",
});

add({
  id: "delay",
  label: "Delay",
  typeCodePrefix: "DL",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["time", "feedback", "mix", "wow", "rate", "flutter", "sync", "pingpong"],
  enums: {
    sync: [...NOTES],
    pingpong: ["off", "on"],
  },
  ranges: {
    time: { min: 1, max: 2000, unit: "ms" },
    feedback: { min: 0, max: 0.95 },
    mix: { min: 0, max: 1, unit: "%" },
    wow: { min: 0, max: 1 },
    rate: { min: 0.1, max: 12, unit: "Hz" },
    flutter: { min: 0, max: 1 },
  },
  defaultArgs: { time: "250", feedback: "0.25", mix: "0.3", wow: "0", rate: "0.65", flutter: "0", sync: "off", pingpong: "off" },
  blurb: "Delay line. Wow 0 is still. Wow 1 is ±8 ms. Rate is the wow speed, default 0.65 Hz. Flutter 1 is ±2 ms at 8 Hz.",
});

add({
  id: "bitcrush",
  label: "Bitcrush",
  typeCodePrefix: "BC",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["bits", "mix", "tone"],
  ranges: {
    bits: { min: 1, max: 16 },
    mix: { min: 0, max: 1, unit: "%" },
    tone: { min: 800, max: 16000, unit: "Hz" },
  },
  defaultArgs: { bits: "8", mix: "1", tone: "8000" },
  blurb: "Quantize, then a recovery lowpass. The formula alone aliases.",
});

add({
  id: "clip",
  label: "Clipper",
  typeCodePrefix: "CL",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["mode", "drive", "threshold", "knee", "bias", "outputTrim", "mix", "autoGain"],
  enums: {
    mode: ["hard", "soft", "asymmetric"],
    autoGain: ["off", "on"],
  },
  ranges: {
    drive: { min: -24, max: 36, unit: "dB" },
    threshold: { min: -36, max: 0, unit: "dB" },
    knee: { min: 0, max: 1 },
    bias: { min: -1, max: 1 },
    outputTrim: { min: -36, max: 12, unit: "dB" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { mode: "hard", drive: "0", threshold: "0", knee: "0.5", bias: "0", outputTrim: "0", mix: "1", autoGain: "off" },
  blurb: "Standalone Clipper: Hard / Soft / Asymmetrisch mit Drive, Threshold, Knee, Bias, Output-Trim, Mix und optionaler Auto Gain.",
});

add({
  id: "saturate",
  label: "Saturate",
  typeCodePrefix: "SA",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["mode", "drive", "bias", "dynamics", "character", "tone", "outputTrim", "mix", "autoGain"],
  enums: {
    mode: ["tape", "tube", "transistor", "transformer", "diode"],
    autoGain: ["off", "on"],
  },
  ranges: {
    drive: { min: -24, max: 36, unit: "dB" },
    bias: { min: -1, max: 1 },
    dynamics: { min: 0, max: 1 },
    character: { min: 0, max: 1 },
    tone: { min: 0, max: 1 },
    outputTrim: { min: -36, max: 12, unit: "dB" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { mode: "tube", drive: "0", bias: "0", dynamics: "0.5", tone: "0.5", character: "0.5", outputTrim: "0", mix: "1", autoGain: "off" },
  blurb: "Sättigung mit Tape/Tube/Transistor/Transformer/Diode, Auto Gain und Tone.",
});

add({
  id: "shaper",
  label: "Shaper",
  typeCodePrefix: "SH",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["mode", "drive", "outputTrim", "mix"],
  enums: { mode: ["fold", "clip"] },
  ranges: {
    drive: { min: -24, max: 36, unit: "dB" },
    outputTrim: { min: -36, max: 12, unit: "dB" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { mode: "fold", drive: "0", outputTrim: "0", mix: "0" },
  blurb: "Waveshaper: Fold-Back oder Hard-Clip mit Drive, Output-Trim und Mix.",
});

add({
  id: "decimate",
  label: "Decimate",
  typeCodePrefix: "DC",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["rate", "bits", "jitter", "hold", "recon", "mix", "outputTrim"],
  ranges: {
    rate: { min: 0, max: 1 },
    bits: { min: 1, max: 16 },
    jitter: { min: 0, max: 1 },
    hold: { min: 0, max: 1 },
    recon: { min: 0, max: 1 },
    mix: { min: 0, max: 1, unit: "%" },
    outputTrim: { min: -36, max: 12, unit: "dB" },
  },
  defaultArgs: { rate: "0.5", bits: "8", jitter: "0", hold: "0", recon: "0.5", mix: "0", outputTrim: "0" },
  blurb: "Bit- und Sample-Rate-Reduktion mit Jitter, Hold, Rekonstruktions-Filter und Output-Trim.",
});

add({
  id: "amp",
  label: "Amp",
  typeCodePrefix: "AM",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["gain", "outputTrim", "mix"],
  ranges: {
    gain: { min: -24, max: 24, unit: "dB" },
    outputTrim: { min: -36, max: 12, unit: "dB" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { gain: "0", outputTrim: "0", mix: "0" },
  blurb: "Einfacher Verstärker mit Gain (dB), Output-Trim und Mix.",
});

add({
  id: "resonator",
  label: "Resonator",
  typeCodePrefix: "RS",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["freq", "res", "gain", "mix"],
  ranges: {
    freq: { min: 20, max: 20000, unit: "Hz" },
    res: { min: 0.1, max: 10 },
    gain: { min: -24, max: 24, unit: "dB" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { freq: "1000", res: "1", gain: "0", mix: "1" },
  blurb: "Resonantes Bandpass-Filter mit Frequenz, Q, Gain und Mix.",
});

add({
  id: "freqshift",
  label: "FreqShift",
  typeCodePrefix: "FS",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["hz", "mix"],
  ranges: {
    hz: { min: -20000, max: 20000, unit: "Hz" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { hz: "0", mix: "1" },
  blurb: "Einseitiger Frequenzverschieber (Single Sideband) mit Hz-Shift und Mix.",
});

add({
  id: "wavefold",
  label: "WaveFold",
  typeCodePrefix: "WF",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["drive", "fold", "mix"],
  ranges: {
    drive: { min: 0, max: 100 },
    fold: { min: 0.001, max: 4 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { drive: "1", fold: "1", mix: "1" },
  blurb: "Wavefolder: reflektiert das Signal am +/- Schwellenwert.",
});

add({
  id: "nlfilter",
  label: "NlFilter",
  typeCodePrefix: "NL",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["cutoff", "drive", "mix"],
  ranges: {
    cutoff: { min: 20, max: 20000, unit: "Hz" },
    drive: { min: 0, max: 10 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { cutoff: "1000", drive: "1", mix: "1" },
  blurb: "Nichtlinearer Tiefpass: tanh-Sättigung vor dem Filter.",
});

add({
  id: "stepseq",
  label: "StepSeq",
  typeCodePrefix: "SS",
  audioIns: [],
  audioOuts: ["out"],
  paramJacks: ["steps", "rate", "sync", "gate", "glide", "swing", "probability", "direction", "seed", "reset", "step0", "step1", "step2", "step3", "mix"],
  ranges: {
    steps: { min: 1, max: 64 },
    rate: { min: 0, max: 100 },
    sync: { min: 0, max: NOTES.length - 1 },
    direction: { min: 0, max: 3 },
    reset: { min: 0, max: 1 },
    gate: { min: 0, max: 1, unit: "%" },
    glide: { min: 0, max: 1, unit: "%" },
    swing: { min: 0, max: 1, unit: "%" },
    probability: { min: 0, max: 1, unit: "%" },
    seed: { min: 0, max: 2147483647 },
    step0: { min: -1, max: 1 },
    step1: { min: -1, max: 1 },
    step2: { min: -1, max: 1 },
    step3: { min: -1, max: 1 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  enums: {
    sync: [...NOTES],
    direction: ["forward", "reverse", "pingpong", "random"],
    reset: ["off", "transport"],
  },
  defaultArgs: {
    steps: "4", rate: "1", sync: "off", gate: "1", glide: "0", swing: "0",
    probability: "1", direction: "forward", seed: "12345", reset: "transport",
    step0: "1", step1: "0", step2: "-1", step3: "0", mix: "1",
  },
  blurb: "1-64-Step-Sequenzer: frei oder tempo-synchron, mit Swing, Gate, Glide, Probability und Richtung.",
});

add({
  id: "adsr",
  label: "ADSR",
  typeCodePrefix: "AD",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["attack", "decay", "sustain", "release", "velocity", "retrigger", "gate"],
  ranges: {
    attack: { min: 0.001, max: 10 },
    decay: { min: 0.001, max: 10 },
    sustain: { min: 0, max: 1 },
    release: { min: 0.001, max: 10 },
    velocity: { min: 0, max: 1, unit: "%" },
    retrigger: { min: 0, max: 1 },
    gate: { min: 0, max: 2 },
  },
  enums: {
    retrigger: ["off", "on"],
    gate: ["audio", "midi", "either"],
  },
  defaultArgs: {
    attack: "0.1", decay: "0.1", sustain: "0.7", release: "0.1",
    velocity: "0", retrigger: "on", gate: "audio",
  },
  blurb: "ADSR-Hüllkurve mit Audio-/MIDI-Gate, Retrigger, Velocity und linearen Phasen.",
});

add({
  id: "samplehold",
  label: "SampleHold",
  typeCodePrefix: "SH",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["rate", "sync", "hold", "slew", "quantize", "mix"],
  ranges: {
    rate: { min: 0, max: 1000 },
    sync: { min: 0, max: NOTES.length - 1 },
    hold: { min: 0, max: 5, unit: "s" },
    slew: { min: 0, max: 1, unit: "%" },
    quantize: { min: 0, max: 1, unit: "%" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  enums: { sync: [...NOTES] },
  defaultArgs: {
    rate: "10", sync: "off", hold: "0", slew: "0", quantize: "0", mix: "1",
  },
  blurb: "Sample & Hold: tastet Mono-Kontrolle oder Audio-Kanal 0 ab, mit Sync, Hold, Slew und Quantize.",
});

add({
  id: "random",
  label: "Random",
  typeCodePrefix: "RN",
  audioIns: [],
  audioOuts: ["out"],
  paramJacks: ["rate", "sync", "min", "max", "shape", "smooth", "bipolar", "seed", "mix"],
  ranges: {
    rate: { min: 0, max: 1000 },
    sync: { min: 0, max: NOTES.length - 1 },
    min: { min: -10, max: 10 },
    max: { min: -10, max: 10 },
    shape: { min: 0, max: 2 },
    smooth: { min: 0, max: 1, unit: "%" },
    bipolar: { min: 0, max: 1 },
    seed: { min: 0, max: 2147483647 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  enums: {
    sync: [...NOTES],
    shape: ["uniform", "triangle", "gaussian"],
    bipolar: ["off", "on"],
  },
  defaultArgs: {
    rate: "10", sync: "off", min: "-1", max: "1", shape: "uniform",
    smooth: "0", bipolar: "off", seed: "12345", mix: "1",
  },
  blurb: "Reproduzierbare Zufallsquelle mit Seed, Verteilung, Sync, Smoothing und bipolarer Skalierung.",
});

add({
  id: "modmath",
  label: "ModMath",
  typeCodePrefix: "MM",
  audioIns: [],
  audioOuts: ["mod"],
  modIns: ["inA", "inB"],
  paramJacks: ["mode", "amount", "mix", "clampMin", "clampMax", "inMin", "inMax", "outMin", "outMax", "quantize", "slew"],
  ranges: {
    mode: { min: 0, max: 11 },
    amount: { min: 0, max: 4 },
    mix: { min: 0, max: 1, unit: "%" },
    clampMin: { min: -10, max: 10 },
    clampMax: { min: -10, max: 10 },
    inMin: { min: -10, max: 10 },
    inMax: { min: -10, max: 10 },
    outMin: { min: -10, max: 10 },
    outMax: { min: -10, max: 10 },
    quantize: { min: 0, max: 1, unit: "%" },
    slew: { min: 0, max: 1, unit: "%" },
  },
  enums: {
    mode: ["add", "sub", "mul", "div", "min", "max", "abs", "invert", "clamp", "remap", "quantize", "slew"],
  },
  defaultArgs: {
    mode: "add", amount: "1", mix: "1", clampMin: "-1", clampMax: "1",
    inMin: "-1", inMax: "1", outMin: "-1", outMax: "1", quantize: "1", slew: "0",
  },
  blurb: "Rechnet zwei echte Modulationsquellen (inA/inB) per mode; ohne Audio-Kanäle.",
});

add({
  id: "envtrigger",
  label: "EnvTrigger",
  typeCodePrefix: "ET",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["thresholddb", "sensitivity", "hysteresis", "holdoff", "hold", "level", "source"],
  ranges: {
    thresholddb: { min: -60, max: 0, unit: "dB" },
    sensitivity: { min: 0, max: 1, unit: "%" },
    hysteresis: { min: 0, max: 24, unit: "dB" },
    holdoff: { min: 0, max: 1000, unit: "ms" },
    hold: { min: 0, max: 5000, unit: "ms" },
    level: { min: 0, max: 10 },
    source: { min: 0, max: 1 },
  },
  enums: { source: ["peak", "rms"] },
  defaultArgs: {
    thresholddb: "-24", sensitivity: "0", hysteresis: "6", holdoff: "0",
    hold: "100", level: "1", source: "peak",
  },
  blurb: "Transienten-Trigger mit Hysterese, Holdoff, Peak/RMS-Detektor und stabilem Puls.",
});

add({
  id: "modmix",
  label: "ModMix",
  typeCodePrefix: "MX",
  audioIns: [],
  modIns: ["inA", "inB"],
  audioOuts: ["out"],
  paramJacks: ["levela", "levelb", "mix", "offset", "normalize", "clamp", "depth", "polarity"],
  ranges: {
    levela: { min: -4, max: 4 },
    levelb: { min: -4, max: 4 },
    offset: { min: -1, max: 1 },
    depth: { min: 0, max: 1 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  enums: { normalize: ["off", "on"], clamp: ["off", "bipolar", "unipolar"], polarity: ["normal", "invert"] },
  defaultArgs: { levela: "1", levelb: "1", mix: "1", offset: "0", normalize: "off", clamp: "off", depth: "1", polarity: "normal" },
  blurb: "Mischt zwei Modulationsquellen mit eigenen Leveln und einem Master-Mix.",
});

add({
  id: "tapestop",
  label: "TapeStop",
  typeCodePrefix: "TS",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["stop", "depth", "mix"],
  ranges: {
    stop: { min: 0.01, max: 30 },
    depth: { min: 0, max: 1 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { stop: "1", depth: "1", mix: "1" },
  blurb: "Band-Stopp-Effekt: verlangsamt die Wiedergabe über stop Sekunden und pitcht um depth runter.",
});

add({
  id: "reversedelay",
  label: "ReverseDelay",
  typeCodePrefix: "RD",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["time", "feedback", "mix"],
  ranges: {
    time: { min: 1, max: 2000 },
    feedback: { min: 0, max: 0.92 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { time: "250", feedback: "0.35", mix: "0.35" },
  blurb: "Rückwärts-Delay: schreibt in einen Ringpuffer und liest rückwärts mit time-Versatz.",
});

add({
  id: "ringmod",
  label: "Ringmod",
  typeCodePrefix: "RM",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["freq", "mix"],
  ranges: {
    freq: { min: 20, max: 2000, unit: "Hz" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { freq: "440", mix: "1" },
  blurb: "Ringmodulator: Eingang x Sinus-Trägerfrequenz.",
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
  paramJacks: ["threshold", "ratio", "attack", "release", "knee", "makeup", "hpf", "mix", "detector", "lookahead", "ceiling"],
  enums: { detector: ["peak", "rms"] },
  ranges: {
    threshold: { min: -60, max: 0, unit: "dB" },
    ratio: { min: 1, max: 20 },
    attack: { min: 0.00005, max: 0.3, unit: "s" },
    release: { min: 0.005, max: 1, unit: "s" },
    knee: { min: 0, max: 24, unit: "dB" },
    makeup: { min: -24, max: 24, unit: "dB" },
    hpf: { min: 0, max: 800, unit: "Hz" },
    mix: { min: 0, max: 1, unit: "%" },
    lookahead: { min: 0, max: 20, unit: "ms" },
    ceiling: { min: -24, max: 0, unit: "dB" },
  },
  defaultArgs: {
    threshold: "-18", ratio: "4", attack: "0.01", release: "0.1",
    knee: "0", makeup: "0", hpf: "0", mix: "1", detector: "peak", lookahead: "0", ceiling: "0",
  },
  blurb: "One compressor. Peak or RMS into the same envelope. Mix is parallel.",
});

add({
  id: "noisegate",
  label: "Gate",
  typeCodePrefix: "GT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["threshold", "attack", "hold", "release", "hyst", "range", "ceiling"],
  ranges: {
    threshold: { min: -80, max: 0, unit: "dB" },
    attack: { min: 0.001, max: 1, unit: "s" },
    release: { min: 0.001, max: 2, unit: "s" },
    hold: { min: 0, max: 0.5, unit: "s" },
    hyst: { min: 0, max: 24, unit: "dB" },
    range: { min: -80, max: 0, unit: "dB" },
    ceiling: { min: -12, max: 0, unit: "dB" },
  },
  defaultArgs: { threshold: "-18", attack: "0.01", hold: "0", release: "0.1", hyst: "1", range: "-80", ceiling: "0" },
  blurb: "Noise gate.",
});

add({
  id: "limit",
  label: "Limit",
  typeCodePrefix: "LM",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["ceiling", "release", "lookahead"],
  ranges: {
    ceiling: { min: -12, max: 0, unit: "dB" },
    release: { min: 0.001, max: 1, unit: "s" },
    lookahead: { min: 0, max: 20, unit: "ms" },
  },
  defaultArgs: { ceiling: "-0.3", release: "0.1", lookahead: "0" },
  blurb: "Peak limiter. Lookahead 0 is the fast mode. A lookahead joins host delay compensation.",
});

add({
  id: "meter",
  label: "Meter",
  typeCodePrefix: "ME",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["mode"],
  enums: { mode: ["loudness", "peak", "rms"] },
  ranges: {
    mode: { min: 0, max: 2 },
  },
  defaultArgs: { mode: "peak" },
  blurb: "Pass-through-Messung (Peak / RMS / Loudness). Verändert das Signal nicht.",
});

add({
  id: "ott",
  label: "OTT",
  typeCodePrefix: "OT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["depth", "time", "in", "low", "mid", "high", "f1", "f2"],
  ranges: {
    depth: { min: 0, max: 1, unit: "%" },
    time: { min: 0, max: 1 },
    in: { min: 0.25, max: 6 },
    low: { min: 0, max: 1.4, unit: "%" },
    mid: { min: 0, max: 1.4, unit: "%" },
    high: { min: 0, max: 1.4, unit: "%" },
    f1: { min: 40, max: 20000, unit: "Hz" },
    f2: { min: 200, max: 20000, unit: "Hz" },
  },
  defaultArgs: { depth: "1", time: "0.3", in: "1", low: "1", mid: "1", high: "1", f1: "90", f2: "3200" },
  blurb: "Multiband up/down. f1/f2 are crossover frequencies. Band amounts process that band only. In is the level into the detector.",
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
  label: "ENV",
  typeCodePrefix: "EV",
  audioIns: ["in"],
  audioOuts: ["mod"],
  paramJacks: ["type", "unit", "attack", "release", "hold", "min", "max", "invert", "source"],
  enums: {
    type: ["peak", "rms"],
    unit: ["lin", "db"],
    invert: ["off", "on"],
    source: ["in", "sidechain"],
  },
  ranges: {
    attack: { min: 0.001, max: 1, unit: "s" },
    release: { min: 0.001, max: 2, unit: "s" },
    hold: { min: 0, max: 0.5, unit: "s" },
    min: { min: 0, max: 1 },
    max: { min: 0, max: 1 },
  },
  defaultArgs: {
    type: "peak",
    unit: "lin",
    attack: "0.01",
    release: "0.1",
    hold: "0",
    min: "0",
    max: "1",
    invert: "off",
    source: "in",
  },
  blurb: "Envelope follower. Audio in, mod out. lin is 0–1; db is dBFS for a DIY compressor.",
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
  id: "phaser",
  label: "Phaser",
  typeCodePrefix: "PH",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["stages", "rate", "depth", "center", "feedback", "mix"],
  ranges: {
    stages: { min: 2, max: 12 },
    rate: { min: 0.05, max: 8, unit: "Hz" },
    depth: { min: 0, max: 1.5 },
    center: { min: 80, max: 4000, unit: "Hz" },
    feedback: { min: 0, max: 0.95 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { stages: "6", rate: "0.4", depth: "0.7", center: "800", feedback: "0.3", mix: "0.5" },
  blurb: "Allpass cascade. Rate, depth, center, feedback, mix.",
});

add({
  id: "flanger",
  label: "Flanger",
  typeCodePrefix: "FG",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["rate", "depth", "delay", "feedback", "mix", "invert"],
  enums: { invert: ["off", "on"] },
  ranges: {
    rate: { min: 0.05, max: 8, unit: "Hz" },
    depth: { min: 0, max: 1 },
    delay: { min: 0.1, max: 20, unit: "ms" },
    feedback: { min: 0, max: 0.95 },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { rate: "0.25", depth: "0.7", delay: "2", feedback: "0.45", mix: "0.5", invert: "on" },
  blurb: "Short modulated delay. Invert is through-zero.",
});

add({
  id: "chorus",
  label: "Chorus",
  typeCodePrefix: "CH",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["rate", "depth", "delay", "voices", "mix", "width"],
  ranges: {
    rate: { min: 0, max: 5, unit: "Hz" },
    depth: { min: 0, max: 1 },
    delay: { min: 5, max: 40, unit: "ms" },
    voices: { min: 2, max: 4 },
    mix: { min: 0, max: 1, unit: "%" },
    width: { min: 0, max: 1 },
  },
  defaultArgs: { rate: "0.6", depth: "0.5", delay: "18", voices: "3", mix: "0.5", width: "0.7" },
  blurb: "Multi-voice modulated delay. Not a flanger.",
});

add({
  id: "deesser",
  label: "De-esser",
  typeCodePrefix: "DS",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["freq", "threshold", "amount", "attack", "release", "mix", "listen"],
  enums: { listen: ["off", "on"] },
  ranges: {
    freq: { min: 2000, max: 12000, unit: "Hz" },
    threshold: { min: -24, max: 6, unit: "dB" },
    amount: { min: 0, max: 1 },
    attack: { min: 0.0002, max: 0.05, unit: "s" },
    release: { min: 0.005, max: 0.3, unit: "s" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { freq: "6500", threshold: "-8", amount: "0.7", attack: "0.002", release: "0.04", mix: "1", listen: "off" },
  blurb: "Split-band de-esser. Threshold is band level versus the full signal. Listen is the band.",
});

add({
  id: "transient",
  label: "Transient",
  typeCodePrefix: "TR",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["attack", "sustain", "fast", "slow", "mix"],
  ranges: {
    attack: { min: -1, max: 1 },
    sustain: { min: -1, max: 1 },
    fast: { min: 0.0003, max: 0.02, unit: "s" },
    slow: { min: 0.01, max: 0.4, unit: "s" },
    mix: { min: 0, max: 1, unit: "%" },
  },
  defaultArgs: { attack: "0.5", sustain: "0", fast: "0.002", slow: "0.08", mix: "1" },
  blurb: "Fast envelope minus slow. Attack punches the hit, sustain shapes the body. Not a compressor.",
});

add({
  id: "utility",
  label: "Utility",
  typeCodePrefix: "UT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["gain", "pan", "polarity"],
  enums: { polarity: ["off", "on"] },
  ranges: {
    gain: { min: -24, max: 24, unit: "dB" },
    pan: { min: -1, max: 1 },
  },
  defaultArgs: { gain: "0", pan: "0", polarity: "off" },
  blurb: "Linear gain in dB, balance pan, polarity. Not a shaper.",
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
  id: "pitch",
  label: "Pitch",
  typeCodePrefix: "PT",
  audioIns: ["in"],
  audioOuts: ["out"],
  paramJacks: ["semitones", "mix", "formant", "ceiling"],
  ranges: {
    semitones: { min: -24, max: 24, unit: "st" },
    mix: { min: 0, max: 1, unit: "%" },
    formant: { min: 0.25, max: 4 },
    ceiling: { min: -24, max: 0, unit: "dB" },
  },
  defaultArgs: { semitones: "0", mix: "1", formant: "1", ceiling: "-0.3" },
  blurb: "Phase-vocoder pitch shift.",
});

add({
  id: "vocoder",
  label: "Vocoder",
  typeCodePrefix: "VC",
  audioIns: ["in", "voice"],
  audioOuts: ["out"],
  paramJacks: ["bands", "mix", "q", "attack", "release"],
  ranges: {
    bands: { min: 3, max: 32 },
    mix: { min: 0, max: 1, unit: "%" },
    q: { min: 0.7, max: 8 },
    attack: { min: 0.001, max: 0.1 },
    release: { min: 0.005, max: 0.5 },
  },
  defaultArgs: { bands: "16", mix: "0.85", q: "2.2", attack: "0.003", release: "0.030" },
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
  defaultArgs: { name: "dirt" },
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
  audioOuts: ["out"],
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
  audioOuts: ["out", "sc"],
  paramJacks: [],
  blurb: "Locked input. sc is the host sidechain (silent when the pin is off).",
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
  paramJacks: ["gain"],
  ranges: { gain: { min: -24, max: 12, unit: "dB" } },
  defaultArgs: { gain: "0" },
  blurb: "Output mix-ins plus output gain.",
});

// New blocks derive their UI controls from the shared catalog instead of duplicating keys/ranges.
const mseg = entryById("mseg")!;
add({
  id: mseg.id, label: mseg.label, typeCodePrefix: mseg.typeCodePrefix,
  audioIns: mseg.audioIns, audioOuts: mseg.modOuts, modIns: mseg.modIns,
  paramJacks: mseg.params.map(p => p.key),
  ranges: Object.fromEntries(mseg.params.filter(p => !p.enum).map(p => [p.key,{min:p.min,max:p.max,unit:p.unit}])),
  enums: Object.fromEntries(mseg.params.filter(p => p.enum).map(p => [p.key,p.enum!])),
  defaultArgs: Object.fromEntries(mseg.params.map(p => [p.key,String(p.def)])),
  blurb: mseg.description,
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
  if (t.startsWith("envtrigger")) return "envtrigger";
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
  if (t.startsWith("pitch")) return "pitch";
  if (t.startsWith("phaser")) return "phaser";
  if (t.startsWith("flanger") || t.startsWith("flange")) return "flanger";
  if (t.startsWith("chorus")) return "chorus";
  if (t.startsWith("deesser") || t.startsWith("deess")) return "deesser";
  if (t.startsWith("transient") || t === "trans" || t === "td") return "transient";
  if (t.startsWith("utility") || t === "util") return "utility";
  if (t.startsWith("vocod")) return "vocoder";
  if (t === "send") return "send";
  if (t === "out") return "out";
  if (t === "in") return "in";
  if (t === "sidechain" || t === "sc" || t === "scin") return "sidechain";
  if (t === "split_lr") return "split_lr";
  if (t === "join_lr") return "join_lr";
  if (t === "bus") return "bus";
  if (t === "join") return "join";
  if (t.startsWith("saturat")) return "saturate";
  if (t.startsWith("shaper") || t === "waveshaper") return "shaper";
  if (t.startsWith("decim")) return "decimate";
  if (t.startsWith("amp")) return "amp";
  if (t.startsWith("resonator")) return "resonator";
  if (t.startsWith("freqshift")) return "freqshift";
  if (t.startsWith("wavefold")) return "wavefold";
  if (t.startsWith("nlfilter")) return "nlfilter";
  if (t.startsWith("mseg")) return "mseg";
  if (t.startsWith("stepseq")) return "stepseq";
  if (t.startsWith("adsr")) return "adsr";
  if (t.startsWith("samplehold")) return "samplehold";
  if (t.startsWith("random")) return "random";
  if (t.startsWith("modmath")) return "modmath";
  if (t.startsWith("modmix")) return "modmix";
  if (t.startsWith("tapestop")) return "tapestop";
  if (t.startsWith("reversedelay")) return "reversedelay";
  if (t.startsWith("ringmod")) return "ringmod";
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

/** Not a knob letter in the DSL: formula body, bus name, routing, split family. */
const NOT_KNOB_CODE = new Set(["y", "channel", "kanal", "name", "mode", "family"]);

/**
 * South bind jacks: runtime numeric expressions that compile as `key = a`.
 * Routing/name/formula fields stay off the rail.
 */
export function bindableJackKeys(spec: ChipSpec): string[] {
  return spec.paramJacks.filter((k) => {
    if (NOT_KNOB_CODE.has(k.toLowerCase())) {
      return false;
    }
    if (spec.ranges[k]) {
      return true;
    }
    return false;
  });
}

function formulaUsesKnob(value: string): boolean {
  return /\b[a-f]\b/i.test(value);
}

/** Catalog bind keys, Custom extras, and formula fields that already reference a–f. */
export function paintedBindKeys(type: string, args: Record<string, string> = {}): string[] {
  const spec = chipSpec(type, args);
  const keys = [...bindableJackKeys(spec)];
  const skip = new Set(["channel", "kanal", "name", "mode", "family"]);
  for (const [k, v] of Object.entries(args)) {
    if (! formulaUsesKnob(String(v))) {
      continue;
    }
    if (skip.has(k.toLowerCase()) || keys.includes(k)) {
      continue;
    }
    keys.push(k);
  }
  if (spec.id === "custom") {
    for (const k of Object.keys(args)) {
      const low = k.toLowerCase();
      if (! skip.has(low) && low !== "y" && ! keys.includes(k) && ! formulaUsesKnob(String(args[k] ?? ""))) {
        keys.push(k);
      }
    }
  }
  return keys;
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
  const keys = bindableJackKeys(spec);
  const n = keys.length;
  if (n === 0) {
    return [];
  }
  const pad = BOARD_GRID;
  const xs = n === 1
    ? [box.w * 0.5]
    : Array.from({ length: n }, (_, i) => pad + ((i + 0.5) / n) * Math.max(BOARD_GRID, box.w - pad * 2));
  const y = box.h;
  return keys.map((key, i) => ({
    key,
    label: spec.paramJackLabels[key] || key,
    x: xs[i] ?? box.w * 0.5,
    y,
  }));
}
