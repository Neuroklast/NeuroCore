import type { FaceKind } from "./nodeFace/faceKind";

/** --------------------------------------------------------------------------
 * Zentraler, typisierter Block-Katalog für NEUROKORE.
 *
 * Dies ist die EINZIGE Stelle, an der Block-Metadaten (ID, Kategorie,
 * Parameter mit Bereich/Einheit/Standard, Audio-/Mod-Ports, UI-Familie,
 * Typecode-Prefix, DSL-Template) gepflegt werden. chipSpec.ts, addBlock.ts,
 * dslComplete.ts und dslLanguage.ts leiten sich davon ab. Es gibt keine
 * zweite, parallel gepflegte Blockliste mehr.
 *
 * Die nativen DSP-Factory-Funktionen in src/dsl/SignalChain.cpp bleiben
 * explizit und typsicher registriert; der Katalog definiert Metadaten und
 * Verträge, nicht die komplette DSP-Implementierung.
 *
 * schemaVersion = 1. Bei inkompatiblen Änderungen hochzahlen.
 * -------------------------------------------------------------------------- */

export type ParamScale = "linear" | "log" | "note";

export type LatencyClass = "none" | "fixed" | "variable";

export interface BlockParam {
  key: string;          // DSL-/Parameter-Schlüssel
  label?: string;       // Anzeigename (Default: key)
  min: number;          // Mindestwert (linear)
  max: number;          // Maximalwert (linear)
  scale: ParamScale;    // linear / log / note
  unit?: string;        // Einheit (Hz, dB, ms, %, st, s)
  enum?: string[];      // Zulässige Aufzählungswerte (Dropdown)
  def: string | number; // Standardwert (als String für DSL/Serialisierung)
  automatable: boolean; // Host-Automation / Makro-Ziel
  modulatable: boolean; // Echtzeit-Modulationsziel
}

export interface BlockCatalogEntry {
  id: string;           // Stabile Block-ID (DSL-Typ)
  typeCodePrefix: string; // Zwei- oder dreistelliger Typecode auf dem Chip
  label: string;        // Anzeigename im Add-Menü / Chip
  category: string;     // Picker-Kategorie
  description: string;  // Kurzbeschreibung (Blurb)
  dspType: string;      // Nativer DSP-Block-Typ (SignalChain)
  audioIns: string[];   // Audio-Eingangsports
  audioOuts: string[];  // Audio-Ausgangsports
  modIns: string[];     // Modulations-Eingangsports
  modOuts: string[];    // Modulations-Ausgangsports
  params: BlockParam[]; // Parameter und Standardwerte
  resources: string[];  // Erforderliche Ressourcen (z. B. ["dsp"], ["fft"])
  latencyClass: LatencyClass;
  schemaVersion: number;
  uiFamily: FaceKind;   // Geschlossene Face-Darstellung
  dslTemplate: string;  // Default-Text für `id: ...` (ohne Block-ID)
}

const SCHEMA_VERSION = 1;

const Hz = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "Hz", def: String(def),
});
const dB = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "dB", def: String(def),
});
const percent = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "%", def: String(def),
});
const sec = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "s", def: String(def),
});
const ms = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "ms", def: String(def),
});
const st = (min: number, max: number, def: number) => ({
  min, max, scale: "linear" as const, unit: "st", def: String(def),
});

const NOTES = [
  "off", "1/1", "1/2.", "1/2", "1/4.", "1/3", "1/4",
  "1/8.", "1/6", "1/8", "1/16.", "1/12", "1/16",
];

export const BLOCK_CATALOG: BlockCatalogEntry[] = [
  {
    id: "filter", typeCodePrefix: "FL", label: "Filter", category: "Tone",
    description: "State-variable filter, Grenzfrequenz in Hz.",
    dspType: "filter",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "type", min: 0, max: 3, scale: "linear", enum: ["lowpass", "highpass", "bandpass", "allpass"], def: "lowpass", automatable: true, modulatable: false },
      { ...Hz(20, 20000, 1000), key: "cutoff", label: "Cutoff", automatable: true, modulatable: true },
      { key: "resonance", min: 0.1, max: 4, scale: "linear", def: "0.4", automatable: true, modulatable: true },
      { key: "channel", min: 0, max: 4, scale: "linear", enum: ["both", "left", "right", "mid", "side"], def: "both", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "filter", dslTemplate: "type = lowpass; cutoff = 1000; resonance = 0.4",
  },
  {
    id: "eq", typeCodePrefix: "EQ", label: "EQ", category: "Tone",
    description: "Parametrischer EQ-Band.",
    dspType: "eq",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "type", min: 0, max: 5, scale: "linear", enum: ["peak", "notch", "lowcut", "highcut", "lowshelf", "highshelf"], def: "peak", automatable: true, modulatable: false },
      { ...Hz(20, 20000, 1000), key: "freq", label: "Freq", automatable: true, modulatable: true },
      { key: "q", min: 0.1, max: 12, scale: "linear", def: "0.7", automatable: true, modulatable: true },
      { ...dB(-24, 24, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
      { key: "channel", min: 0, max: 4, scale: "linear", enum: ["both", "left", "right", "mid", "side"], def: "both", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "filter", dslTemplate: "type = peak; freq = 1000; gain = 0",
  },
  {
    id: "delay", typeCodePrefix: "DL", label: "Delay", category: "Time",
    description: "Delay-Linie. Wow 0 ist still. Wow 1 ist ±8 ms.",
    dspType: "delay",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...ms(1, 2000, 250), key: "time", label: "Time", automatable: true, modulatable: true },
      { key: "feedback", min: 0, max: 0.95, scale: "linear", def: "0.25", automatable: true, modulatable: true },
      { ...percent(0, 1, 0.3), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "wow", min: 0, max: 1, scale: "linear", def: "0", automatable: true, modulatable: false },
      { ...Hz(0.1, 12, 0.65), key: "rate", label: "Rate", automatable: true, modulatable: false },
      { key: "flutter", min: 0, max: 1, scale: "linear", def: "0", automatable: true, modulatable: false },
      { key: "sync", min: 0, max: NOTES.length - 1, scale: "linear", enum: NOTES, def: "off", automatable: true, modulatable: false },
      { key: "pingpong", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "delay", dslTemplate: "time = 250; mix = 0.3; feedback = 0.2; wow = 0; flutter = 0",
  },
  {
    id: "bitcrush", typeCodePrefix: "BC", label: "Bitcrush", category: "Drive",
    description: "Quantisiert und filtert dann mit einem Recovery-Lowpass.",
    dspType: "bitcrush",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "bits", min: 1, max: 16, scale: "linear", def: "8", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { ...Hz(800, 16000, 8000), key: "tone", label: "Tone", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "bits = 8; mix = 1; tone = 8000",
  },
  {
    id: "clip", typeCodePrefix: "CL", label: "Clipper", category: "Drive",
    description: "Eigenständiger Clipper: Hard / Soft / Asymmetrisch mit Drive, Threshold, Knee, Bias, Output-Trim, Mix und optionaler Auto Gain.",
    dspType: "clip",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "mode", min: 0, max: 2, scale: "linear", enum: ["hard", "soft", "asymmetric"], def: "hard", automatable: false, modulatable: false },
      { ...dB(-24, 36, 0), key: "drive", label: "Drive", automatable: true, modulatable: true },
      { ...dB(-36, 0, 0), key: "threshold", label: "Threshold", automatable: true, modulatable: true },
      { key: "knee", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "bias", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { ...dB(-36, 12, 0), key: "outputTrim", label: "Output Trim", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "autoGain", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "mode = hard; drive = 0; threshold = 0; knee = 0; bias = 0; outputTrim = 0; mix = 1; autoGain = off",
  },
  {
    id: "saturate", typeCodePrefix: "SA", label: "Saturate", category: "Drive",
    description: "Sättigung mit Tape/Tube/Transistor/Transformer/Diode, Auto Gain und Tone.",
    dspType: "saturate",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "mode", min: 0, max: 4, scale: "linear", enum: ["tape", "tube", "transistor", "transformer", "diode"], def: "tube", automatable: false, modulatable: false },
      { ...dB(-24, 36, 0), key: "drive", label: "Drive", automatable: true, modulatable: true },
      { key: "bias", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { key: "dynamics", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "character", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "tone", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { ...dB(-36, 12, 0), key: "outputTrim", label: "Output Trim", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "autoGain", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "mode = tube; drive = 0; bias = 0; dynamics = 0.5; tone = 0.5; character = 0.5; outputTrim = 0; mix = 1; autoGain = off",
  },
  {
    id: "shaper", typeCodePrefix: "SH", label: "Shaper", category: "Drive",
    description: "Waveshaper mit Fold-Back, Drive, Output-Trim und Mix.",
    dspType: "shaper",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "mode", min: 0, max: 1, scale: "linear", enum: ["fold", "clip"], def: "fold", automatable: false, modulatable: false },
      { ...dB(-24, 36, 0), key: "drive", label: "Drive", automatable: true, modulatable: true },
      { ...dB(-36, 12, 0), key: "outputTrim", label: "Output Trim", automatable: true, modulatable: true },
      { ...percent(0, 1, 0), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "mode = fold; drive = 0; outputTrim = 0; mix = 0",
  },
  {
    id: "decimate", typeCodePrefix: "DC", label: "Decimate", category: "Drive",
    description: "Bit- und Sample-Rate-Reduktion mit Jitter, Hold, Rekonstruktions-Filter und Output-Trim.",
    dspType: "decimate",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "rate", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "bits", min: 1, max: 16, scale: "linear", def: "8", automatable: true, modulatable: false },
      { key: "jitter", min: 0, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { key: "hold", min: 0, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { key: "recon", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { ...percent(0, 1, 0), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { ...dB(-36, 12, 0), key: "outputTrim", label: "Output Trim", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "rate = 0.5; bits = 8; jitter = 0; hold = 0; recon = 0.5; mix = 0; outputTrim = 0",
  },
  {
    id: "amp", typeCodePrefix: "AM", label: "Amp", category: "Drive",
    description: "Einfacher Verstärker mit Gain (dB), Output-Trim und Mix.",
    dspType: "amp",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...dB(-24, 24, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
      { ...dB(-36, 12, 0), key: "outputTrim", label: "Output Trim", automatable: true, modulatable: true },
      { ...percent(0, 1, 0), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "utility", dslTemplate: "gain = 0; outputTrim = 0; mix = 0",
  },
  {
    id: "resonator", typeCodePrefix: "RS", label: "Resonator", category: "Tone",
    description: "Resonantes Bandpass-Filter mit Frequenz, Q, Gain und Mix.",
    dspType: "resonator",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...Hz(20, 20000, 1000), key: "freq", label: "Freq", automatable: true, modulatable: true },
      { key: "res", min: 0.1, max: 10, scale: "linear", def: "1", automatable: true, modulatable: true },
      { ...dB(-24, 24, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "filter", dslTemplate: "freq = 1000; res = 1; gain = 0; mix = 1",
  },
  {
    id: "freqshift", typeCodePrefix: "FS", label: "FreqShift", category: "Tone",
    description: "Einseitiger Frequenzverschieber (Single Sideband) mit Hz-Shift und Mix.",
    dspType: "freqshift",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "hz", min: -20000, max: 20000, scale: "linear", def: "0", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "filter", dslTemplate: "hz = 0; mix = 1",
  },
  {
    id: "wavefold", typeCodePrefix: "WF", label: "WaveFold", category: "Drive",
    description: "Wavefolder: reflektiert das Signal am +/- Schwellenwert.",
    dspType: "wavefold",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "drive", min: 0, max: 100, scale: "linear", def: "1", automatable: true, modulatable: true },
      { key: "fold", min: 0.001, max: 4, scale: "linear", def: "1", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "drive = 1; fold = 1; mix = 1",
  },
  {
    id: "nlfilter", typeCodePrefix: "NL", label: "NlFilter", category: "Tone",
    description: "Nichtlinearer Tiefpass: tanh-Sättigung vor dem Filter.",
    dspType: "nlfilter",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...Hz(20, 20000, 1000), key: "cutoff", label: "Cutoff", automatable: true, modulatable: true },
      { key: "drive", min: 0, max: 10, scale: "linear", def: "1", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "filter", dslTemplate: "cutoff = 1000; drive = 1; mix = 1",
  },
  {
    id: "stepseq", typeCodePrefix: "SS", label: "StepSeq", category: "Mod",
    description: "Vier-Schritt-Sequenzer: gibt step0..step3 im Takt von rate Hz aus.",
    dspType: "stepseq",
    audioIns: [], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "steps", min: 1, max: 64, scale: "linear", def: "4", automatable: false, modulatable: false },
      { key: "rate", min: 0, max: 100, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "sync", min: 0, max: NOTES.length - 1, scale: "linear", enum: NOTES, def: "off", automatable: false, modulatable: false },
      { ...percent(0, 1, 1), key: "gate", label: "Gate", automatable: true, modulatable: false },
      { ...percent(0, 1, 0), key: "glide", label: "Glide", automatable: true, modulatable: false },
      { ...percent(0, 1, 0), key: "swing", label: "Swing", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "probability", label: "Probability", automatable: true, modulatable: false },
      { key: "direction", min: 0, max: 3, scale: "linear", enum: ["forward", "reverse", "pingpong", "random"], def: "forward", automatable: false, modulatable: false },
      { key: "seed", min: 0, max: 2147483647, scale: "linear", def: "12345", automatable: false, modulatable: false },
      { key: "reset", min: 0, max: 1, scale: "linear", enum: ["off", "transport"], def: "transport", automatable: false, modulatable: false },
      { key: "step0", min: -1, max: 1, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "step1", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: false },
      { key: "step2", min: -1, max: 1, scale: "linear", def: "-1", automatable: true, modulatable: false },
      { key: "step3", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "rate = 1; step0 = 1; step1 = 0; step2 = -1; step3 = 0; mix = 1",
  },
  {
    id: "adsr", typeCodePrefix: "AD", label: "ADSR", category: "Mod",
    description: "ADSR-Hüllkurve, getriggert über Gate-Eingang (Kanal 0 > 0.5).",
    dspType: "adsr",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "attack", min: 0.001, max: 10, scale: "linear", def: "0.1", automatable: true, modulatable: false },
      { key: "decay", min: 0.001, max: 10, scale: "linear", def: "0.1", automatable: true, modulatable: false },
      { key: "sustain", min: 0, max: 1, scale: "linear", def: "0.7", automatable: true, modulatable: false },
      { key: "release", min: 0.001, max: 10, scale: "linear", def: "0.1", automatable: true, modulatable: false },
      { ...percent(0, 1, 0), key: "velocity", label: "Velocity", automatable: true, modulatable: false },
      { key: "retrigger", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "on", automatable: false, modulatable: false },
      { key: "gate", min: 0, max: 2, scale: "linear", enum: ["audio", "midi", "either"], def: "audio", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "env", dslTemplate: "attack = 0.1; decay = 0.1; sustain = 0.7; release = 0.1",
  },
  {
    id: "samplehold", typeCodePrefix: "SH", label: "SampleHold", category: "Mod",
    description: "Sample & Hold: tastet einen Mono-Kontrolleingang oder Audio-Kanal 0 im Takt von rate/sync ab.",
    dspType: "samplehold",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "rate", min: 0, max: 1000, scale: "linear", def: "10", automatable: true, modulatable: false },
      { key: "sync", min: 0, max: NOTES.length - 1, scale: "linear", enum: NOTES, def: "off", automatable: false, modulatable: false },
      { key: "hold", min: 0, max: 5, scale: "linear", def: "0", automatable: true, modulatable: false, unit: "s" },
      { ...percent(0, 1, 0), key: "slew", label: "Slew", automatable: true, modulatable: false },
      { ...percent(0, 1, 0), key: "quantize", label: "Quantize", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "rate = 10; sync = off; hold = 0; slew = 0; quantize = 0; mix = 1",
  },
  {
    id: "random", typeCodePrefix: "RN", label: "Random", category: "Mod",
    description: "Reproduzierbare Zufallsquelle mit Seed, Verteilung, Sync, Smoothing und bipolarer Skalierung.",
    dspType: "random",
    audioIns: [], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "rate", min: 0, max: 1000, scale: "linear", def: "10", automatable: true, modulatable: false },
      { key: "sync", min: 0, max: NOTES.length - 1, scale: "linear", enum: NOTES, def: "off", automatable: false, modulatable: false },
      { key: "min", min: -10, max: 10, scale: "linear", def: "-1", automatable: true, modulatable: false },
      { key: "max", min: -10, max: 10, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "shape", min: 0, max: 2, scale: "linear", enum: ["uniform", "triangle", "gaussian"], def: "uniform", automatable: false, modulatable: false },
      { ...percent(0, 1, 0), key: "smooth", label: "Smooth", automatable: true, modulatable: false },
      { key: "bipolar", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
      { key: "seed", min: 0, max: 2147483647, scale: "linear", def: "12345", automatable: false, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "rate = 10; sync = off; min = -1; max = 1; shape = uniform; smooth = 0; bipolar = off; seed = 12345; mix = 1",
  },
  {
    id: "modmath", typeCodePrefix: "MM", label: "ModMath", category: "Mod",
    description: "Rechnet zwei echte Modulationsquellen (inA/inB) per mode; ohne Audio-Kanäle.",
    dspType: "modmath",
    audioIns: [], audioOuts: ["mod"], modIns: ["inA", "inB"], modOuts: ["out"],
    params: [
      { key: "mode", min: 0, max: 11, scale: "linear", enum: ["add", "sub", "mul", "div", "min", "max", "abs", "invert", "clamp", "remap", "quantize", "slew"], def: "add", automatable: false, modulatable: false },
      { key: "amount", min: 0, max: 4, scale: "linear", def: "1", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
      { key: "clampMin", min: -10, max: 10, scale: "linear", def: "-1", automatable: true, modulatable: false },
      { key: "clampMax", min: -10, max: 10, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "inMin", min: -10, max: 10, scale: "linear", def: "-1", automatable: true, modulatable: false },
      { key: "inMax", min: -10, max: 10, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "outMin", min: -10, max: 10, scale: "linear", def: "-1", automatable: true, modulatable: false },
      { key: "outMax", min: -10, max: 10, scale: "linear", def: "1", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "quantize", label: "Quantize", automatable: true, modulatable: false },
      { ...percent(0, 1, 0), key: "slew", label: "Slew", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "inA = osc1; inB = env1; mode = add; amount = 1; mix = 1",
  },
  {
    id: "envtrigger", typeCodePrefix: "ET", label: "EnvTrigger", category: "Mod",
    description: "Transienten-Trigger mit Hysterese, Holdoff, Peak/RMS-Detektor und stabilem Puls.",
    dspType: "envtrigger",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "threshold", min: -60, max: 0, scale: "linear", def: "-24", automatable: true, modulatable: false, unit: "dB" },
      { ...percent(0, 1, 0), key: "sensitivity", label: "Sensitivity", automatable: true, modulatable: false },
      { key: "hysteresis", min: 0, max: 24, scale: "linear", def: "6", automatable: true, modulatable: false, unit: "dB" },
      { key: "holdoff", min: 0, max: 1000, scale: "linear", def: "0", automatable: true, modulatable: false, unit: "ms" },
      { key: "hold", min: 0, max: 5000, scale: "linear", def: "100", automatable: true, modulatable: false, unit: "ms" },
      { key: "level", min: 0, max: 10, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "source", min: 0, max: 1, scale: "linear", enum: ["peak", "rms"], def: "peak", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "env", dslTemplate: "threshold = -24; sensitivity = 0; hysteresis = 6; holdoff = 0; hold = 100; level = 1; source = peak",
  },
  {
    id: "modmix", typeCodePrefix: "MX", label: "ModMix", category: "Mod",
    description: "Mischt zwei Modulationsquellen mit eigenen Leveln und einem Master-Mix.",
    dspType: "modmix",
    audioIns: ["inA", "inB"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "levelA", min: 0, max: 4, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "levelB", min: 0, max: 4, scale: "linear", def: "1", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "levelA = 1; levelB = 1; mix = 1",
  },
  {
    id: "tapestop", typeCodePrefix: "TS", label: "TapeStop", category: "Time",
    description: "Band-Stopp-Effekt: verlangsamt die Wiedergabe über stop Sekunden und pitcht um depth runter.",
    dspType: "tapestop",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "stop", min: 0.01, max: 30, scale: "linear", def: "1", automatable: true, modulatable: false, unit: "s" },
      { key: "depth", min: 0, max: 1, scale: "linear", def: "1", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "delay", dslTemplate: "stop = 1; depth = 1; mix = 1",
  },
  {
    id: "reversedelay", typeCodePrefix: "RD", label: "ReverseDelay", category: "Time",
    description: "Rückwärts-Delay: schreibt in einen Ringpuffer und liest rückwärts mit time-Versatz.",
    dspType: "reversedelay",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["out"],
    params: [
      { key: "time", min: 1, max: 2000, scale: "linear", def: "250", automatable: true, modulatable: false, unit: "ms" },
      { key: "feedback", min: 0, max: 0.92, scale: "linear", def: "0.35", automatable: true, modulatable: false },
      { ...percent(0, 1, 0.35), key: "mix", label: "Mix", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "delay", dslTemplate: "time = 250; feedback = 0.35; mix = 0.35",
  },
  {
    id: "ringmod", typeCodePrefix: "RM", label: "Ringmod", category: "Drive",
    description: "Ringmodulator: Eingang x Sinus-Trägerfrequenz.",
    dspType: "ringmod",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "freq", min: 20, max: 2000, scale: "linear", def: "440", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "utility", dslTemplate: "freq = 440; mix = 1",
  },
  {
    id: "reverb", typeCodePrefix: "RV", label: "Reverb", category: "Time",
    description: "Raum, Mix in Prozent.",
    dspType: "reverb",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "size", min: 0, max: 1, scale: "linear", def: "0.45", automatable: true, modulatable: true },
      { key: "decay", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "damp", min: 0, max: 1, scale: "linear", def: "0.3", automatable: true, modulatable: true },
      { ...percent(0, 1, 0.3), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "reverb", dslTemplate: "size = 0.5; decay = 0.5; mix = 0.3",
  },
  {
    id: "stage", typeCodePrefix: "DR", label: "Drive", category: "Dynamics",
    description: "Formula stage.",
    dspType: "stage",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "y", min: 0, max: 1, scale: "linear", def: "x", automatable: true, modulatable: true },
      { key: "channel", min: 0, max: 4, scale: "linear", enum: ["both", "left", "right", "mid", "side"], def: "both", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "y = x",
  },
  {
    id: "custom", typeCodePrefix: "CU", label: "Custom", category: "Custom",
    description: "Formula plus extra inputs.",
    dspType: "custom",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "y", min: 0, max: 1, scale: "linear", def: "x", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "drive", dslTemplate: "y = x",
  },
  {
    id: "comp", typeCodePrefix: "CP", label: "Comp", category: "Dynamics",
    description: "Ein Kompressor. Peak oder RMS in dasselbe Envelope. Mix ist parallel.",
    dspType: "comp",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...dB(-60, 0, -18), key: "threshold", label: "Threshold", automatable: true, modulatable: true },
      { key: "ratio", min: 1, max: 20, scale: "linear", def: "4", automatable: true, modulatable: true },
      { ...sec(0.00005, 0.3, 0.01), key: "attack", label: "Attack", automatable: true, modulatable: false },
      { ...sec(0.005, 1, 0.1), key: "release", label: "Release", automatable: true, modulatable: false },
      { ...dB(0, 24, 0), key: "knee", label: "Knee", automatable: true, modulatable: false },
      { ...dB(-24, 24, 0), key: "makeup", label: "Makeup", automatable: true, modulatable: true },
      { ...Hz(0, 800, 0), key: "hpf", label: "HPF", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "detector", min: 0, max: 1, scale: "linear", enum: ["peak", "rms"], def: "peak", automatable: true, modulatable: false },
      { ...ms(0, 20, 0), key: "lookahead", label: "Lookahead", automatable: true, modulatable: false },
      { ...dB(-24, 0, 0), key: "ceiling", label: "Ceiling", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "threshold = -18; ratio = 4; attack = 0.01; release = 0.1; knee = 0; makeup = 0; hpf = 0; mix = 1; detector = peak; lookahead = 0; ceiling = 0",
  },
  {
    id: "noisegate", typeCodePrefix: "GT", label: "Gate", category: "Dynamics",
    description: "Noise Gate.",
    dspType: "noisegate",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...dB(-80, 0, -18), key: "threshold", label: "Threshold", automatable: true, modulatable: true },
      { ...sec(0.001, 1, 0.01), key: "attack", label: "Attack", automatable: true, modulatable: false },
      { ...sec(0, 0.5, 0.04), key: "hold", label: "Hold", automatable: true, modulatable: false },
      { ...sec(0.001, 1, 0.08), key: "release", label: "Release", automatable: true, modulatable: false },
      { ...dB(0, 24, 1), key: "hyst", label: "Hysteresis", automatable: true, modulatable: false },
      { ...dB(-90, 0, -80), key: "range", label: "Range", automatable: true, modulatable: false },
      { ...dB(-24, 0, 0), key: "ceiling", label: "Ceiling", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "threshold = -18; attack = 0.01; release = 0.1; ceiling = 0",
  },
  {
    id: "limit", typeCodePrefix: "LM", label: "Limit", category: "Dynamics",
    description: "Peak Limiter. Lookahead 0 ist der schnelle Modus.",
    dspType: "limit",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...dB(-12, 0, -0.3), key: "ceiling", label: "Ceiling", automatable: true, modulatable: true },
      { ...sec(0.001, 1, 0.1), key: "release", label: "Release", automatable: true, modulatable: false },
      { ...ms(0, 20, 0), key: "lookahead", label: "Lookahead", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "ceiling = -0.3; release = 0.08; lookahead = 0",
  },
  {
    id: "ott", typeCodePrefix: "OT", label: "OTT", category: "Dynamics",
    description: "Multiband Up+Down. Band-Anteile bearbeiten nur das eigene Band.",
    dspType: "ott",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...percent(0, 1, 1), key: "depth", label: "Depth", automatable: true, modulatable: true },
      { key: "time", min: 0, max: 1, scale: "linear", def: "0.3", automatable: true, modulatable: false },
      { ...Hz(0.25, 6, 1), key: "in", label: "In", automatable: true, modulatable: false },
      { ...percent(0, 1.4, 1), key: "low", label: "Low", automatable: true, modulatable: true },
      { ...percent(0, 1.4, 1), key: "mid", label: "Mid", automatable: true, modulatable: true },
      { ...percent(0, 1.4, 1), key: "high", label: "High", automatable: true, modulatable: true },
      { ...Hz(40, 20000, 90), key: "f1", label: "F1", automatable: true, modulatable: false },
      { ...Hz(200, 20000, 3200), key: "f2", label: "F2", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "depth = 0.52; time = 0.3; in = 1.15; low = 1; mid = 0.92; high = 1.05; f1 = 90; f2 = 3200",
  },
  {
    id: "ir", typeCodePrefix: "IR", label: "IR", category: "Time",
    description: "Convolution Cab.",
    dspType: "ir",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...percent(0, 1, 0.3), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { ...dB(-24, 24, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
    ],
    resources: ["ir"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "cab", dslTemplate: "mix = 0.3; gain = 0",
  },
  {
    id: "env", typeCodePrefix: "EV", label: "ENV", category: "Mod",
    description: "Envelope Follower. Audio-in, mod-out. lin ist 0–1; db ist dBFS für einen DIY-Kompressor.",
    dspType: "env",
    audioIns: ["in"], audioOuts: ["mod"], modIns: [], modOuts: ["mod"],
    params: [
      { key: "type", min: 0, max: 1, scale: "linear", enum: ["peak", "rms"], def: "peak", automatable: false, modulatable: false },
      { key: "unit", min: 0, max: 1, scale: "linear", enum: ["lin", "db"], def: "lin", automatable: false, modulatable: false },
      { ...sec(0.001, 1, 0.01), key: "attack", label: "Attack", automatable: true, modulatable: false },
      { ...sec(0.001, 2, 0.1), key: "release", label: "Release", automatable: true, modulatable: false },
      { ...sec(0, 0.5, 0), key: "hold", label: "Hold", automatable: true, modulatable: false },
      { key: "min", min: 0, max: 1, scale: "linear", def: "0", automatable: true, modulatable: false },
      { key: "max", min: 0, max: 1, scale: "linear", def: "1", automatable: true, modulatable: false },
      { key: "invert", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
      { key: "source", min: 0, max: 1, scale: "linear", enum: ["in", "sidechain"], def: "in", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "env", dslTemplate: "type = peak; unit = lin; attack = 0.01; release = 0.1; hold = 0; min = 0; max = 1; invert = off; source = in",
  },
  {
    id: "osc", typeCodePrefix: "LF", label: "LFO", category: "Mod",
    description: "LFO, kein Audio-In. Nur Bindung.",
    dspType: "osc",
    audioIns: [], audioOuts: ["mod"], modIns: [], modOuts: ["mod"],
    params: [
      { key: "shape", min: 0, max: 4, scale: "linear", enum: ["sine", "triangle", "square", "saw", "noise"], def: "sine", automatable: true, modulatable: false },
      { ...Hz(0.01, 40, 1), key: "freq", label: "Freq", automatable: true, modulatable: false },
      { key: "sync", min: 0, max: NOTES.length - 1, scale: "linear", enum: NOTES, def: "off", automatable: true, modulatable: false },
      { key: "depth", min: 0, max: 1, scale: "linear", def: "1", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "lfo", dslTemplate: "freq = 1; shape = sine",
  },
  {
    id: "phaser", typeCodePrefix: "PH", label: "Phaser", category: "Mod",
    description: "Allpass-Kaskade. Rate, Depth, Center, Feedback, Mix.",
    dspType: "phaser",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "stages", min: 2, max: 12, scale: "linear", def: "6", automatable: true, modulatable: false },
      { ...Hz(0.05, 8, 0.4), key: "rate", label: "Rate", automatable: true, modulatable: false },
      { key: "depth", min: 0, max: 1.5, scale: "linear", def: "0.7", automatable: true, modulatable: true },
      { ...Hz(80, 4000, 800), key: "center", label: "Center", automatable: true, modulatable: true },
      { key: "feedback", min: 0, max: 0.95, scale: "linear", def: "0.3", automatable: true, modulatable: true },
      { ...percent(0, 1, 0.5), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "modfx", dslTemplate: "stages = 6; rate = 0.4; depth = 0.7; center = 800; feedback = 0.3; mix = 0.5",
  },
  {
    id: "flanger", typeCodePrefix: "FG", label: "Flanger", category: "Mod",
    description: "Kurzes moduliertes Delay. Invert ist Through-Zero.",
    dspType: "flanger",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...Hz(0.05, 8, 0.25), key: "rate", label: "Rate", automatable: true, modulatable: false },
      { key: "depth", min: 0, max: 1, scale: "linear", def: "0.7", automatable: true, modulatable: true },
      { ...ms(0.1, 20, 2), key: "delay", label: "Delay", automatable: true, modulatable: true },
      { key: "feedback", min: 0, max: 0.95, scale: "linear", def: "0.45", automatable: true, modulatable: true },
      { ...percent(0, 1, 0.5), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "invert", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "on", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "modfx", dslTemplate: "rate = 0.25; depth = 0.7; delay = 2; feedback = 0.45; mix = 0.5; invert = on",
  },
  {
    id: "chorus", typeCodePrefix: "CH", label: "Chorus", category: "Mod",
    description: "Multi-Stimmen moduliertes Delay. Kein Flanger.",
    dspType: "chorus",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...Hz(0, 5, 0.6), key: "rate", label: "Rate", automatable: true, modulatable: false },
      { key: "depth", min: 0, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { ...ms(5, 40, 18), key: "delay", label: "Delay", automatable: true, modulatable: true },
      { key: "voices", min: 2, max: 4, scale: "linear", def: "3", automatable: true, modulatable: false },
      { ...percent(0, 1, 0.5), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "width", min: 0, max: 1, scale: "linear", def: "0.7", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "modfx", dslTemplate: "rate = 0.6; depth = 0.5; delay = 18; voices = 3; mix = 0.5; width = 0.7",
  },
  {
    id: "deesser", typeCodePrefix: "DS", label: "De-esser", category: "Dynamics",
    description: "Split-Band De-esser. Threshold ist Band-Level versus das Gesamt-Signal.",
    dspType: "deesser",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...Hz(2000, 12000, 6500), key: "freq", label: "Freq", automatable: true, modulatable: true },
      { ...dB(-24, 6, -8), key: "threshold", label: "Threshold", automatable: true, modulatable: true },
      { key: "amount", min: 0, max: 1, scale: "linear", def: "0.7", automatable: true, modulatable: true },
      { ...sec(0.0002, 0.05, 0.002), key: "attack", label: "Attack", automatable: true, modulatable: false },
      { ...sec(0.005, 0.3, 0.04), key: "release", label: "Release", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "listen", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "freq = 6500; threshold = -8; amount = 0.7; attack = 0.002; release = 0.04; mix = 1; listen = off",
  },
  {
    id: "transient", typeCodePrefix: "TR", label: "Transient", category: "Dynamics",
    description: "Schnelles Envelope minus langsames. Attack puncht den Hit, Sustain formt den Körper.",
    dspType: "transient",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "attack", min: -1, max: 1, scale: "linear", def: "0.5", automatable: true, modulatable: true },
      { key: "sustain", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { ...sec(0.0003, 0.02, 0.002), key: "fast", label: "Fast", automatable: true, modulatable: false },
      { ...sec(0.01, 0.4, 0.08), key: "slow", label: "Slow", automatable: true, modulatable: false },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "dynamics", dslTemplate: "attack = 0.5; sustain = 0; fast = 0.002; slow = 0.08; mix = 1",
  },
  {
    id: "utility", typeCodePrefix: "UT", label: "Utility", category: "Routing",
    description: "Linearer Gain in dB, Balance-Pan, Polarität. Kein Shaper.",
    dspType: "utility",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...dB(-24, 24, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
      { key: "pan", min: -1, max: 1, scale: "linear", def: "0", automatable: true, modulatable: true },
      { key: "polarity", min: 0, max: 1, scale: "linear", enum: ["off", "on"], def: "off", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "utility", dslTemplate: "gain = 0; pan = 0; polarity = off",
  },
  {
    id: "octaver", typeCodePrefix: "OC", label: "Octaver", category: "Tone",
    description: "Analog-Oktave.",
    dspType: "octaver",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "sub", min: 0, max: 1.5, scale: "linear", def: "1", automatable: true, modulatable: true },
      { key: "up", min: 0, max: 1.5, scale: "linear", def: "0", automatable: true, modulatable: true },
      { ...percent(0, 1, 0.3), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { ...Hz(20, 4000, 120), key: "tone", label: "Tone", automatable: true, modulatable: true },
      { key: "thresh", min: 0.01, max: 0.25, scale: "linear", def: "0.05", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "pitch", dslTemplate: "sub = 1; up = 0; mix = 0.3; tone = 120; thresh = 0.05",
  },
  {
    id: "pitch", typeCodePrefix: "PT", label: "Pitch", category: "Tone",
    description: "Phase-Vocoder Pitch-Shift.",
    dspType: "pitch",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...st(-24, 24, 0), key: "semitones", label: "Semitones", automatable: true, modulatable: true },
      { ...percent(0, 1, 1), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "formant", min: 0.25, max: 4, scale: "linear", def: "1", automatable: true, modulatable: true },
      { ...dB(-24, 0, -0.3), key: "ceiling", label: "Ceiling", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "fixed", schemaVersion: SCHEMA_VERSION,
    uiFamily: "pitch", dslTemplate: "semitones = 0; mix = 1; formant = 1; ceiling = -0.3",
  },
  {
    id: "vocoder", typeCodePrefix: "VC", label: "Vocoder", category: "Mod",
    description: "Carrier-In plus Voice/Sidechain.",
    dspType: "vocoder",
    audioIns: ["in", "voice"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "bands", min: 3, max: 32, scale: "linear", def: "16", automatable: true, modulatable: false },
      { ...percent(0, 1, 0.85), key: "mix", label: "Mix", automatable: true, modulatable: true },
      { key: "q", min: 0.7, max: 8, scale: "linear", def: "2.2", automatable: true, modulatable: true },
      { ...sec(0.001, 0.1, 0.003), key: "attack", label: "Attack", automatable: true, modulatable: false },
      { ...sec(0.005, 0.5, 0.03), key: "release", label: "Release", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "pitch", dslTemplate: "bands = 16; mix = 0.85; q = 2.2; attack = 0.003; release = 0.030",
  },
  {
    id: "width", typeCodePrefix: "WD", label: "Width", category: "Routing",
    description: "Stereobreite.",
    dspType: "widen",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "width", min: 0, max: 1.4, scale: "linear", def: "1", automatable: true, modulatable: true },
      { ...ms(0, 40, 12), key: "delay", label: "Delay", automatable: true, modulatable: false },
      { ...Hz(20, 400, 140), key: "bass", label: "Bass", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "width = 1; delay = 12; bass = 140",
  },
  {
    id: "split_ms", typeCodePrefix: "SM", label: "Split Mid/Side", category: "Routing",
    description: "Encodiert Mid/Side.",
    dspType: "ms",
    audioIns: ["in"], audioOuts: ["mid", "side"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "mode = split",
  },
  {
    id: "join_ms", typeCodePrefix: "JM", label: "Join Mid/Side", category: "Routing",
    description: "Decodiert Mid/Side.",
    dspType: "ms",
    audioIns: ["mid", "side"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "mode = join",
  },
  {
    id: "split_lr", typeCodePrefix: "SL", label: "Split Left/Right", category: "Routing",
    description: "Split nach Left/Right.",
    dspType: "ms",
    audioIns: ["in"], audioOuts: ["left", "right"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "mode = split; family = lr",
  },
  {
    id: "join_lr", typeCodePrefix: "JL", label: "Join Left/Right", category: "Routing",
    description: "Join Left/Right.",
    dspType: "ms",
    audioIns: ["left", "right"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "mode = join; family = lr",
  },
  {
    id: "msplit", typeCodePrefix: "MB", label: "Multiband Split", category: "Routing",
    description: "Crossover-Split.",
    dspType: "xover",
    audioIns: ["in"], audioOuts: ["low", "mid", "high"], modIns: [], modOuts: [],
    params: [
      { ...Hz(40, 800, 200), key: "f1", label: "F1", automatable: true, modulatable: false },
      { ...Hz(200, 8000, 2000), key: "f2", label: "F2", automatable: true, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "f1 = 200; f2 = 2000",
  },
  {
    id: "meter", typeCodePrefix: "ME", label: "Meter", category: "IO",
    description: "Pass-through-Messung. Peak, RMS oder Loudness. Verändert das Signal nicht.",
    dspType: "meter",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: ["mod"],
    params: [
      { key: "mode", min: 0, max: 2, scale: "linear", enum: ["loudness", "peak", "rms"], def: "peak", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "io", dslTemplate: "mode = peak",
  },
  {
    id: "bus", typeCodePrefix: "BS", label: "Bus", category: "Routing",
    description: "Benannte Schiene.",
    dspType: "bus",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "name", min: 0, max: 1, scale: "linear", def: "dirt", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "name = dirt",
  },
  {
    id: "join", typeCodePrefix: "JN", label: "Join Signal", category: "Routing",
    description: "Mischt zwei Eingänge.",
    dspType: "join",
    audioIns: ["inA", "inB"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { ...percent(0, 1, 0.5), key: "mix", label: "Mix", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "mix = 0.5",
  },
  {
    id: "send", typeCodePrefix: "SD", label: "Send", category: "Routing",
    description: "Send plus Control-Out auf der unteren Kante.",
    dspType: "send",
    audioIns: ["in"], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [
      { key: "kanal", min: 0, max: 5, scale: "linear", enum: ["both", "left", "right", "mid", "side", "env"], def: "both", automatable: false, modulatable: false },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "routing", dslTemplate: "kanal = both",
  },
  {
    id: "in", typeCodePrefix: "IN", label: "IN", category: "IO",
    description: "Locked Input. sc ist der Host-Sidechain (stumm, wenn der Pin aus ist).",
    dspType: "in",
    audioIns: [], audioOuts: ["out", "sc"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "io", dslTemplate: "",
  },
  {
    id: "sidechain", typeCodePrefix: "SC", label: "Sidechain", category: "IO",
    description: "Host-Sidechain-Eingang.",
    dspType: "sidechain",
    audioIns: [], audioOuts: ["out"], modIns: [], modOuts: [],
    params: [],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "io", dslTemplate: "",
  },
  {
    id: "out", typeCodePrefix: "OU", label: "OUT", category: "IO",
    description: "Output-Mix plus Output-Gain.",
    dspType: "out",
    audioIns: ["in"], audioOuts: [], modIns: [], modOuts: [],
    params: [
      { ...dB(-24, 12, 0), key: "gain", label: "Gain", automatable: true, modulatable: true },
    ],
    resources: ["dsp"], latencyClass: "none", schemaVersion: SCHEMA_VERSION,
    uiFamily: "io", dslTemplate: "gain = 0",
  },
];

/** Alle IDs, die als interne Routing-/IO-Konstrukte gelten (kein eigenständiger DSP-Block). */
const INTERNAL_IDS = new Set(["bus", "join", "send", "in", "out", "sidechain"]);

/** Alias-Auflösung (DSL-Typ → Katalog-ID), wie sie bisher in chipSpec.resolveChipId lebte. */
export function resolveCatalogId(type: string, args: Record<string, string> = {}): string {
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
    if (isLr) return isJoin ? "join_lr" : "split_lr";
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
  return t;
}

/** Einträge, die im Add-Menü angeboten werden (öffentliche DSP/IO-Blöcke ohne IN/OUT). */
export const ADDABLE_ENTRIES = BLOCK_CATALOG.filter((e) => e.id !== "in" && e.id !== "out");

export function entryById(id: string): BlockCatalogEntry | undefined {
  return BLOCK_CATALOG.find((e) => e.id === id);
}

export function allCatalogEntries(): BlockCatalogEntry[] {
  return BLOCK_CATALOG;
}

export function isInternal(id: string): boolean {
  return INTERNAL_IDS.has(id);
}

/** Param-Schlüssel, die als Bind-Buchse (south rail) compilierbar sind (`key = a`).
 * Nur numerische (non-enum), automatable Parameter. */
export function bindableParamKeys(entry: BlockCatalogEntry): string[] {
  return entry.params
    .filter((p) => !p.enum && p.automatable)
    .map((p) => p.key)
    .filter((k) => k !== "channel" && k !== "kanal" && k !== "name" && k !== "mode" && k !== "family");
}
