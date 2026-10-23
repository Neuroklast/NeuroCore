import { inferTags } from "../factoryTags.mjs";
import { applyMetadata } from "./metadata.mjs";

const k = (name, min, max, def) => ({ name, min, max, default: def });
const make = (name, category, description, body, knobs = {}) => {
  const defs = Object.entries(knobs).map(([a, p]) => `param ${a} = ${p.name} [${p.min}, ${p.max}]`).join("\n");
  const script = `# ${name}\n# ${description}\n${defs}\n${body.trim()}\n`;
  const preset = { name, category, description, inputGain: 0, outputGain: 0, mix: 1, script, tags: inferTags(script, name, description, category) };
  for (const [a, p] of Object.entries(knobs)) preset["param" + a.toUpperCase()] = p;
  return preset;
};

export const additions = [
  make("Poly Synth Motion", "Synth", "Slow looping MSEG plus seeded random drift mixed into a lowpass. Motion is the curve length; Drift sets how much random moves the cutoff. The dry signal stays on the main path.",
`mseg1: points = 0:0.15:0.5,0.4:1:1.2,0.75:0.35:-0.8,1:0.2:0; duration = a; source = free; mode = loop
random1: rate = c; min = 0; max = 1; smooth = 0.92; seed = 441
modmix1: ina = mseg1; inb = random1; levela = 1; levelb = b; mix = 0.5
stage1: y = x
bus motion:
  filter1: type = lowpass; cutoff = d + modmix1 * 5200; resonance = e
out: main = 1-f; motion = f`,
  { a: k("Motion", 0.6, 12, 3.2), b: k("Drift", 0, 1.4, 0.35), c: k("Rate", 0.05, 2, 0.35), d: k("Tone", 200, 4000, 1200), e: k("Resonance", 0.2, 1.1, 0.45), f: k("Blend", 0.15, 1, 0.55) }),

  make("Velocity Driven Drive", "Synth", "MIDI velocity scales saturation drive through a short ADSR. Soft notes stay close to the dry tone; harder notes add harmonics. Route MIDI notes to the insert.",
`adsr1: gate = midi; attack = 0.004; decay = 0.12; sustain = 0; release = 0.08; velocity = 1
saturate1: mode = transistor; drive = 1.5 + adsr1 * a; tone = b; mix = 1; autoGain = on
filter1: type = lowpass; cutoff = c; resonance = 0.32`,
  { a: k("Drive", 2, 18, 8), b: k("Tone", 0.2, 0.85, 0.5), c: k("Cutoff", 1200, 14000, 6500) }),

  make("Hybrid Dynamic Wah", "Modulation", "Envelope and LFO share one filter. Sensitivity scales the envelope; Motion Balance crossfades envelope versus LFO. Works on guitar, bass, or synth without extra inputs.",
`env1: type = peak; unit = lin; attack = 0.006; release = 0.07; min = 0; max = a
osc1: shape = sine; freq = b; depth = 1
modmix1: ina = env1; inb = osc1; mix = c; levela = 1; levelb = 1
filter1: type = bandpass; center = d + modmix1 * e; width = 900; resonance = f`,
  { a: k("Sensitivity", 0.2, 1, 0.7), b: k("Rate", 0.15, 6, 1.4), c: k("Motion Balance", 0, 1, 0.45), d: k("Cutoff", 200, 1800, 480), e: k("Depth", 400, 5000, 1800), f: k("Resonance", 0.4, 1.4, 0.85) }),

  make("Subsafe Groove Bass", "Bass", "Crossover keeps the low band on a clean path. The upper band gets a host-synced filter groove and light saturation. Blend is the upper return; it is not a measured brickwall for the sub.",
`xover1: f1 = a
stepseq1: steps = 8; sync = 1/16; values = 0.2,0.9,0.35,0.75,0.15,1,0.4,0.65; glide = 0.04
bus high:
  nlfilter1: cutoff = 600 + stepseq1 * b; drive = c; mix = d
  saturate1: mode = transistor; drive = c; tone = e; mix = 0.3
out: low = 1; high = f`,
  { a: k("Crossover", 80, 220, 140), b: k("Motion", 400, 6000, 2400), c: k("Drive", 1, 8, 2.4), d: k("Groove", 0.25, 1, 0.7), e: k("Tone", 0.2, 0.85, 0.5), f: k("Blend", 0.2, 1, 0.65) }),

  make("Resonant Drum Body", "Drums", "Onsets trigger a parallel resonant body. Tune and Resonance set the ring; Decay is the trigger hold. Dry transients stay on the main path.",
`envtrigger1: thresholddb = a; hysteresis = 5; holdoff = 25; hold = d; level = 1; source = peak
stage1: y = x
bus body:
  resonator1: freq = b; res = c; gain = -4; mix = 1
  stage2: y = x * envtrigger1
out: main = 1; body = e`,
  { a: k("Trigger", -36, -8, -22), b: k("Tune", 80, 1800, 220), c: k("Resonance", 1.2, 8, 3.2), d: k("Decay", 20, 220, 70), e: k("Blend", 0.1, 0.8, 0.35) }),

  make("Dynamic Hat Tamer", "Drums", "De-esser on hats and bright percussion. Amount only rises when the band exceeds the threshold. Not a static lowpass; Metal Cymbal Tamer remains the static overhead EQ.",
`deesser1: freq = a; threshold = b; amount = c; attack = 0.002; release = d; mix = 1
eq1: type = highshelf; freq = 11000; q = 0.5; gain = e`,
  { a: k("Frequency", 4000, 11000, 7500), b: k("Threshold", -18, 0, -8), c: k("Amount", 0.2, 1, 0.65), d: k("Release", 0.02, 0.16, 0.05), e: k("Air", -4, 2, 0) }),

  make("Triggered Room Send", "Drums", "Audio onsets open a parallel room. The dry path is unchanged. Trigger must move the send into the reverb; a silent trigger leaves only the dry signal.",
`envtrigger1: thresholddb = a; hysteresis = 6; holdoff = 40; hold = 90; level = 1; source = peak
stage1: y = x
bus room:
  send: in = 1
  stage2: y = x * envtrigger1
  reverb1: size = b; decay = c; damp = 0.55; mix = 1
  filter1: type = lowpass; cutoff = d; resonance = 0.3
out: main = 1; room = e`,
  { a: k("Trigger", -36, -8, -20), b: k("Room", 0.2, 0.85, 0.45), c: k("Decay", 0.2, 2.5, 0.7), d: k("Tone", 2500, 12000, 6500), e: k("Return", 0.08, 0.7, 0.28) }),
];

export function applyOverhaul(input) {
  const have = new Set(input.map((p) => p.name));
  const extra = additions.filter((a) => ! have.has(a.name));
  return [...input, ...extra].map(applyMetadata);
}
