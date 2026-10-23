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
];

export function applyOverhaul(input) {
  const have = new Set(input.map((p) => p.name));
  const extra = additions.filter((a) => ! have.has(a.name));
  return [...input, ...extra].map(applyMetadata);
}
