/**
 * Professional factory preset library for NeuroKore.
 *
 * Modeling notes (abstractions, not full SPICE):
 * - tube(x,d): asymmetric 12AX7-style transfer (even harmonics + soft compression)
 * - diode(x,d) / asinh: op-amp diode clipper / soft knee (C-inf)
 * - softclip(x,d): smooth algebraic y = x/sqrt(1+x^2) -- low HF alias vs piecewise cubic
 * - hardclip(x,lim): soft-knee Hermite brickwall (~3%) -- never pure clamp
 *
 * Anti-alias clip topology (ALL clip-heavy presets follow this):
 *   1. Prefer softclip / tube / diode over bare hardclip
 *   2. If hard ceiling needed: hardclip(softclip(x, drive), ceiling)
 *   3. Always LPF after heavy clip (tone / cab / anti-alias recovery)
 *   4. Optional mild HPF pre-clip (less HF into the nonlinearity)
 *   5. Parallel lerp(dry, wet) for transparent peak control
 *   6. Keep resonance <= ~3.2 (engine caps ~4.5); avoid Q self-osc crackle
 *
 * Multi-stage chains emulate preamp -> tone -> power -> cab (HPF/LPF band-limit).
 * outputGain defaults to 0 dB (host meter friendly; AutoGain handles loudness).
 */
import fs from "fs";
import { registerWave2 } from "./factory/wave2.mjs";

const p = (name, min, max, def) => ({ name, min, max, default: def });

const inferTags = (script, name, description, category, extra = []) => {
  const blob = `${script}\n${name}\n${description}\n${category}`.toLowerCase();
  const tags = new Set((extra || []).map((t) => String(t).toLowerCase().trim()).filter(Boolean));
  if (category) tags.add(String(category).toLowerCase());
  const addIf = (re, ...ts) => {
    if (re.test(blob)) ts.forEach((t) => tags.add(t));
  };
  addIf(/delay/, "delay");
  addIf(/reverb|verb\d+\s*:/, "reverb", "space");
  addIf(/ms\d+\s*:|mode\s*=\s*encode|channel\s*=\s*(mid|side)|ms_encode|ms_decode|type\s*=\s*midside|split\d*\s*:/,
        "mid-side", "midside", "ms", "mid", "side");
  addIf(/type\s*=\s*leftright|type\s*=\s*crossover/, "split", "stereo");
  addIf(/\bbus\b|send:|out:/, "bus", "parallel");
  addIf(/tube/, "tube");
  addIf(/softclip|hardclip/, "clip");
  addIf(/hardclip/, "hardclip");
  addIf(/bitcrush/, "bitcrush", "lo-fi");
  addIf(/fold/, "fold");
  addIf(/diode/, "diode");
  addIf(/comp\d*\s*:/, "compressor");
  addIf(/ott\d*\s*:/, "ott", "compressor", "multiband");
  addIf(/widen\d*\s*:|stereo\d*\s*:/, "widen", "stereo", "width");
  addIf(/gate\d*\s*:/, "gate");
  addIf(/limit\d*\s*:/, "limiter");
  addIf(/ir\d*\s*:|convolve/, "ir", "cabinet");
  addIf(/filter/, "filter");
  addIf(/eq\d*\s*:/, "eq", "equalizer");
  addIf(/lowpass|highcut/, "lowpass");
  addIf(/highpass|lowcut/, "highpass");
  addIf(/vocod|sidechain|\bsc\b/, "vocoder", "sidechain");
  addIf(/octav|subharmonic/, "octaver", "pitch");
  addIf(/octaver\d*\s*:/, "octaver", "pitch");
  addIf(/vocoder\d*\s*:/, "vocoder", "sidechain");
  addIf(/pingpong/, "pingpong", "stereo");
  addIf(/osc\d*\s*:/, "modulation", "lfo");
  addIf(/env\d*\s*:/, "envelope");
  const word = (w) => new RegExp("(^|[^a-z0-9])" + w + "([^a-z0-9]|$)", "i").test(blob);
  for (const w of [
    "tape", "crunch", "vocal", "drum", "kick", "snare", "hat", "bass", "guitar",
    "amp", "fuzz", "overdrive", "chorus", "phaser", "tremolo", "shimmer", "hall",
    "plate", "slap", "glue", "air", "width", "mono", "room", "master", "crush",
    "lofi", "edm", "synth", "pad", "lead", "send", "drive", "saturate", "clipper",
    "haas", "cinematic", "trailer", "score", "dialogue", "boom", "impact",
    "octaver", "vocoder", "svt", "jcm", "metal", "cyberpunk", "digital", "stereo",
    "techno", "hardcore", "gabber", "rumble", "acid", "industrial", "glitch",
    "ott", "multiband",
  ]) {
    if (word(w)) tags.add(w);
  }
  return [...tags].sort();
};

/** One-line note for a DSL block so the operator can read the formula. */
const describeBlock = (line) => {
  const t = line.trim();
  const m = t.match(/^([a-zA-Z_][\w]*)\s*:/);
  if (!m) return null;
  const id = m[1];
  const low = id.toLowerCase();
  if (low.startsWith("param")) return null;
  if (low.startsWith("stage")) {
    if (/tube/.test(t)) return `${id}: tube saturation`;
    if (/diode/.test(t)) return `${id}: diode clip`;
    if (/softclip/.test(t)) return `${id}: soft clip`;
    if (/hardclip/.test(t)) return `${id}: hard clip`;
    if (/bitcrush/.test(t)) return `${id}: bitcrush`;
    if (/fold/.test(t)) return `${id}: wavefold`;
    if (/lerp/.test(t)) return `${id}: dry/wet blend`;
    if (/\*\s*0\.0|\*\s*0\b/.test(t)) return `${id}: mute this channel`;
    return `${id}: waveshape / mix`;
  }
  if (low.startsWith("filter")) {
    if (/highpass/.test(t)) return `${id}: highpass`;
    if (/lowpass/.test(t)) return `${id}: lowpass`;
    if (/bandpass/.test(t)) return `${id}: bandpass`;
    return `${id}: filter`;
  }
  if (low.startsWith("delay")) return `${id}: delay line`;
  if (low.startsWith("reverb") || low.startsWith("verb")) return `${id}: reverb`;
  if (low.startsWith("env")) return `${id}: envelope follower`;
  if (low.startsWith("osc")) return `${id}: LFO`;
  if (low.startsWith("comp")) return `${id}: compressor`;
  if (low.startsWith("ott")) return `${id}: OTT (3-band up+down)`;
  if (low.startsWith("widen") || low.startsWith("stereo")) return `${id}: mono to stereo`;
  if (low.startsWith("eq")) {
    if (/notch/.test(t)) return `${id}: notch`;
    if (/lowcut|highpass/.test(t)) return `${id}: low cut`;
    if (/highcut|lowpass/.test(t)) return `${id}: high cut`;
    if (/lowshelf/.test(t)) return `${id}: low shelf`;
    if (/highshelf/.test(t)) return `${id}: high shelf`;
    return `${id}: peak EQ`;
  }
  if (low.startsWith("octaver") || low === "octave") return `${id}: analog octaver`;
  if (low.startsWith("vocoder")) return `${id}: vocoder (voice on Sidechain)`;
  if (low.startsWith("gate")) return `${id}: noise gate`;
  if (low.startsWith("limit")) return `${id}: limiter`;
  if (low.startsWith("xover") || low.startsWith("crossover")) return `${id}: crossover`;
  if (low.startsWith("ir") || low.startsWith("convolve")) return `${id}: impulse response`;
  if (low.startsWith("meter") || low === "probe") return `${id}: meter (dry)`;
  if (low.startsWith("sidechain") || low === "scin") return `${id}: host sidechain`;
  if (low.startsWith("ms"))
    return /decode/.test(t) ? `${id}: mid/side decode` : `${id}: mid/side encode`;
  if (low === "bus") {
    const rest = t.split(":")[1]?.trim() || "";
    return rest ? `bus ${rest}: parallel path` : "bus: parallel path";
  }
  if (low === "send") return "send: feed this bus from the input";
  if (low === "out") return "out: mix buses back to the output";
  return null;
};

const annotateScript = (name, description, script, opts = {}) => {
  const sound = String(opts.sound || description).trim();
  const header = [
    `# ${name}`,
    `# ${sound}`,
  ];
  for (const [k, v] of [
    ["a", opts.a],
    ["b", opts.b],
    ["c", opts.c],
    ["d", opts.d],
    ["e", opts.e],
    ["f", opts.f],
  ]) {
    if (v && v.name)
      header.push(`# ${k} ${v.name}: ${v.min} to ${v.max}, default ${v.default}`);
  }
  const body = [];
  for (const raw of script.split(/\r?\n/)) {
    if (raw.trim().startsWith("#") || raw.trim().startsWith("//")) {
      body.push(raw);
      continue;
    }
    const note = describeBlock(raw);
    if (note && !/^\s*param\b/i.test(raw)) {
      const short = note.replace(/^[^:]+:\s*/, "");
      const base = raw.replace(/\s+$/, "");
      if (base.includes("#") || base.includes("//"))
        body.push(raw);
      else
        body.push(`${base}  # ${short}`);
    } else {
      body.push(raw);
    }
  }
  return [...header, "", ...body].join("\n");
};

const preset = (name, category, description, script, opts = {}) => {
  const trimmed = script.trim();
  const out = {
    name,
    category,
    description,
    inputGain: opts.inG ?? 0,
    outputGain: opts.outG ?? 0,
    mix: opts.mix ?? 1,
    script: annotateScript(name, description, trimmed, opts),
    tags: inferTags(trimmed, name, description, category, opts.tags || []),
  };
  if (opts.a) out.paramA = opts.a;
  if (opts.b) out.paramB = opts.b;
  if (opts.c) out.paramC = opts.c;
  if (opts.d) out.paramD = opts.d;
  if (opts.e) out.paramE = opts.e;
  if (opts.f) out.paramF = opts.f;
  if (opts.irs && typeof opts.irs === "object")
    out.irs = opts.irs;
  return out;
};

const list = [];
const add = (...a) => list.push(preset(...a));

// =============================================================================
// DISTORTION / AMPS (circuit-inspired multi-stage)
// =============================================================================
add(
  "Fender Clean",
  "Distortion",
  "Airy clean: soft single tube, bright-cap high shelf, open cab 9 kHz — airy, not mid-crush.",
  `param a = Drive [0.5, 2.8]
param b = Bright [0.0, 7.0]
param c = Level [0.55, 1.45]
filter1: type = highpass; cutoff = 45; resonance = 0.22
stage1: y = tube(x, a * 0.5)
eq1: type = highshelf; freq = 4800; q = 0.65; gain = b
stage2: y = softclip(y, 0.75) * c
filter2: type = lowpass; cutoff = 9500; resonance = 0.25`,
  { a: p("Drive", 0.5, 2.8, 1.1), b: p("Bright", 0, 7, 3.2), c: p("Level", 0.55, 1.45, 1.1), outG: 0 }
);

add(
  "Marshall Crunch",
  "Distortion",
  "Plexi bark: dual tube, 750 Hz mid peak (not a bandpass), presence shelf, 5.5 kHz cab.",
  `param a = Drive [1.4, 7.5]
param b = Presence [2000, 7000]
param c = Level [0.38, 1.15]
filter1: type = highpass; cutoff = 100; resonance = 0.38
stage1: y = tube(x, a * 0.6)
stage2: y = tube(y, a * 0.7)
eq1: type = peak; freq = 750; q = 1.05; gain = 5
stage3: y = softclip(y * 1.2, 1.45) * c
eq2: type = highshelf; freq = b; q = 0.7; gain = 2.8
filter2: type = lowpass; cutoff = 5500; resonance = 0.4`,
  { a: p("Drive", 1.4, 7.5, 3.6), b: p("Presence", 2000, 7000, 3800), c: p("Level", 0.38, 1.15, 0.72), outG: 0 }
);

add(
  "Mesa High Gain",
  "Distortion",
  "Rectifier wall: tight HPF, triple tube, hard soft-knee ceiling, dark cab — densest amp.",
  `param a = Gain [3.5, 12.0]
param b = Tight [45, 200]
param c = Level [0.25, 0.9]
gate1: threshold = -46; hyst = 6; hold = 0.03; range = -80
filter1: type = highpass; cutoff = b; resonance = 0.6
stage1: y = tube(x, a * 0.5)
stage2: y = tube(y, a * 0.6)
stage3: y = tube(y, a * 0.45)
stage4: y = hardclip(softclip(y, 1.55), 0.68)
filter2: type = lowpass; cutoff = 4500; resonance = 0.58
stage5: y = diode(y, 1.6) * c
ir1: mix = 0.45; gain = 0
limit1: ceiling = -0.3; release = 0.08`,
  { a: p("Gain", 3.5, 12, 7.8), b: p("Tight", 45, 200, 85), c: p("Level", 0.25, 0.9, 0.42), tags: ["amp", "gate", "ir"], outG: 0, irs: { ir1: "American IR 01.wav" } }
);

add(
  "Vox Top Boost",
  "Distortion",
  "AC chime: aggressive bass cut, 1.8 kHz peak (top-boost), glassy softclip, open cab.",
  `param a = Drive [1.2, 6.5]
param b = Cut [280, 1400]
param c = Level [0.45, 1.25]
filter1: type = highpass; cutoff = b; resonance = 0.5
stage1: y = tube(x, a * 0.85)
eq1: type = peak; freq = 1800; q = 1.15; gain = 4.5
stage2: y = softclip(y * 1.15, 1.5) * c
filter2: type = lowpass; cutoff = 8200; resonance = 0.32`,
  { a: p("Drive", 1.2, 6.5, 2.9), b: p("Cut", 280, 1400, 520), c: p("Level", 0.45, 1.25, 0.92), outG: 0 }
);

add(
  "Tube Screamer",
  "Distortion",
  "TS808: ~720 Hz HPF into clipper, then a real 720 Hz mid peak (not a bandpass), tone LPF.",
  `param a = Drive [1.2, 11.0]
param b = Tone [350, 4500]
param c = Level [0.28, 1.15]
filter1: type = highpass; cutoff = 720; resonance = 0.28
stage1: y = softclip(x, a)
eq1: type = peak; freq = 720; q = 1.65; gain = 6
stage2: y = diode(y * 1.15, 1.3) * c
filter2: type = lowpass; cutoff = b; resonance = 0.48
ir1: mix = 0.4; gain = 6`,
  { a: p("Drive", 1.2, 11, 5.2), b: p("Tone", 350, 4500, 1800), c: p("Level", 0.28, 1.15, 0.7), tags: ["ir"], outG: 0, irs: { ir1: "Vintage IR 01.wav" } }
);

add(
  "Klon Centaur",
  "Distortion",
  "Transparent OD: clean path blend + soft diode clip (Klon-style clean blend).",
  `param a = Drive [0.8, 8.0]
param b = Blend [0.2, 0.95]
param c = Level [0.4, 1.3]
stage1: y = x * c
bus dirt:
  send: in = 1
  filter1: type = highpass; cutoff = 90; resonance = 0.3
  stage2: y = diode(x, a) * c
  eq1: type = peak; freq = 1050; q = 0.85; gain = 3.5
  filter2: type = lowpass; cutoff = 9000; resonance = 0.3
out: main = 1-b; dirt = b`,
  { a: p("Drive", 0.8, 8, 3.0), b: p("Blend", 0.2, 0.95, 0.62), c: p("Level", 0.4, 1.3, 0.9), outG: 0 }
);

add(
  "ProCo RAT",
  "Distortion",
  "RAT: soft-pre hard ceiling + steep LPF tone (classic filter-after-clip, low alias).",
  `param a = Dist [2.0, 12.0]
param b = Filter [300, 6500]
param c = Level [0.25, 1.0]
filter1: type = highpass; cutoff = 70; resonance = 0.35
stage1: y = hardclip(softclip(x, a * 0.55), 0.55)
filter2: type = lowpass; cutoff = b; resonance = 0.6
stage2: y = y * c`,
  { a: p("Dist", 2, 12, 6.5), b: p("Filter", 300, 6500, 1800), c: p("Level", 0.25, 1, 0.58), outG: 0 }
);

add(
  "Fuzz Face",
  "Distortion",
  "Germanium fuzz: bias + soft-pre hard knee + heavy LPF + tube polish (no bare brickwall).",
  `param a = Fuzz [3.0, 16.0]
param b = Tone [250, 4500]
param c = Level [0.15, 0.85]
gate1: threshold = -44; hyst = 8; hold = 0.04; range = -80
filter1: type = highpass; cutoff = 45; resonance = 0.3
stage1: y = hardclip(softclip(x + 0.06, a * 0.4), 0.38)
filter2: type = lowpass; cutoff = b; resonance = 0.7
stage2: y = tube(y, 1.4) * c
ir1: mix = 0.35; gain = 8
limit1: ceiling = -0.4; release = 0.1`,
  { a: p("Fuzz", 3, 16, 9), b: p("Tone", 250, 4500, 1400), c: p("Level", 0.15, 0.85, 0.42), inG: 1.5, tags: ["gate", "ir"], outG: 0, irs: { ir1: "Vintage IR 01.wav" } }
);

add(
  "Soft Overdrive",
  "Distortion",
  "Open softclip OD: gentle HPF, pure softclip (no mid hump), wide tone LPF — cleanest dirt.",
  `param a = Drive [0.6, 6.0]
param b = Tone [800, 11000]
param c = Level [0.45, 1.35]
filter1: type = highpass; cutoff = 50; resonance = 0.22
stage1: y = softclip(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.32
stage2: y = y * c`,
  { a: p("Drive", 0.6, 6, 2.4), b: p("Tone", 800, 11000, 5500), c: p("Level", 0.45, 1.35, 0.95), outG: 0 }
);

add(
  "Bitcrusher",
  "Distortion",
  "Soft-pre bit reduction + anti-alias LPF after quantize (never bitcrush without LPF).",
  `param a = Bits [3.0, 12.0]
param b = Drive [0.8, 4.0]
param c = LPF [600, 10000]
param d = Level [0.4, 1.2]
stage1: y = bitcrush(softclip(x, b), a) * d
filter1: type = lowpass; cutoff = c; resonance = 0.45`,
  { a: p("Bits", 3, 12, 6), b: p("Drive", 0.8, 4, 1.8), c: p("LPF", 600, 10000, 4500), d: p("Level", 0.4, 1.2, 0.8), outG: 0 }
);

add(
  "Cyberpunk Drive",
  "Distortion",
  "Guitar-shaped digital dirt: HPF, soft preamp, bitcrush, fold, short metal comb, cab LPF. Loud on purpose — not Glitch Laboratory.",
  `param a = Drive [1.8, 9.5]
param b = Bits [5.0, 12.0]
param c = Fold [0.22, 0.68]
param d = Metal [5, 26]
param e = Tone [1400, 7200]
param f = Level [0.85, 1.5]
filter1: type = highpass; cutoff = 115; resonance = 0.4
stage1: y = softclip(x, a * 0.5)
eq1: type = peak; freq = 1350; q = 0.95; gain = 3.2
stage2: y = bitcrush(y, b)
stage3: y = fold(y, -c, c)
delay1: time = d; feedback = 0.34; mix = 0.16; damp = 6800
filter2: type = lowpass; cutoff = e; resonance = 0.38
stage4: y = diode(y, 1.32) * f * 1.35`,
  {
    a: p("Drive", 1.8, 9.5, 4.6),
    b: p("Bits", 5, 12, 8.5),
    c: p("Fold", 0.22, 0.68, 0.4),
    d: p("Metal", 5, 26, 11),
    e: p("Tone", 1400, 7200, 4200),
    f: p("Level", 0.85, 1.5, 1.15),
    tags: ["cyberpunk", "digital", "guitar", "distortion"],
    outG: 0,
  }
);

add(
  "Wave Folder",
  "Distortion",
  "West-coast fold -> diode recovery -> LPF (fold is harsh; always band-limit after).",
  `param a = Drive [1.5, 12.0]
param b = Fold [0.2, 0.85]
param c = Level [0.25, 1.0]
stage1: y = fold(x * a, -b, b)
stage2: y = softclip(diode(y, 1.3), 1.1) * c
filter1: type = lowpass; cutoff = 7500; resonance = 0.35`,
  { a: p("Drive", 1.5, 12, 5.5), b: p("Fold", 0.2, 0.85, 0.48), c: p("Level", 0.25, 1, 0.62), outG: 0 }
);

// =============================================================================
// MODULATION
// =============================================================================
add(
  "Classic Tremolo",
  "Modulation",
  "Opto-style sine tremolo. Rate is a note grid (1/1..1/16), one cycle per note.",
  `param a = Rate [1/1, 1/16]
param b = Depth [0.2, 0.85]
param c = Floor [0.2, 0.7]
osc1: shape = sine; sync = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0, 1, 0.4), b: p("Depth", 0.2, 0.85, 0.5), c: p("Floor", 0.2, 0.7, 0.4), outG: 0 }
);

add(
  "Tremolo Free",
  "Modulation",
  "Sine tremolo with free Hz rate.",
  `param a = Rate [0.2, 10.0]
param b = Depth [0.2, 0.85]
param c = Floor [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0.2, 10, 3.5), b: p("Depth", 0.2, 0.85, 0.5), c: p("Floor", 0.2, 0.7, 0.4), outG: 0 }
);

add(
  "Chopper",
  "Modulation",
  "Soft-square trem chopper. Rate is a note grid (1/1..1/16). Dynamics preserved (floor).",
  `param a = Rate [1/1, 1/16]
param b = Depth [0.25, 0.85]
param c = Floor [0.15, 0.55]
osc1: shape = softsquare; sync = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0, 1, 0.45), b: p("Depth", 0.25, 0.85, 0.55), c: p("Floor", 0.15, 0.55, 0.28), outG: 0 }
);

add(
  "Chopper Free",
  "Modulation",
  "Same soft chopper with free Hz rate (not tempo-locked).",
  `param a = Rate [0.5, 12.0]
param b = Depth [0.25, 0.85]
param c = Floor [0.15, 0.55]
osc1: shape = softsquare; freq = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0.5, 12, 4.0), b: p("Depth", 0.25, 0.85, 0.55), c: p("Floor", 0.15, 0.55, 0.28), outG: 0 }
);

add(
  "Ring Modulator",
  "Modulation",
  "Four-quadrant ring mod (carrier * signal).",
  `param a = Freq [15.0, 1500.0]
param b = Depth [0.35, 1.0]
param c = Level [0.4, 1.2]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = x * (1.0 - b + b * osc1) * c
filter1: type = lowpass; cutoff = 12000; resonance = 0.3`,
  { a: p("Freq", 15, 1500, 180), b: p("Depth", 0.35, 1, 0.88), c: p("Level", 0.4, 1.2, 0.85), outG: 0 }
);

add(
  "Uni-Vibe AM",
  "Modulation",
  "Vibe-ish: dual path AM with slow LFO and soft saturation.",
  `param a = Rate [0.15, 6.0]
param b = Depth [0.2, 0.85]
param c = Drive [1.0, 3.0]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = tube(x, c)
stage2: y = y * (1.0 - b + b * (0.5 + 0.5 * osc1))`,
  { a: p("Rate", 0.15, 6, 1.1), b: p("Depth", 0.2, 0.85, 0.55), c: p("Drive", 1, 3, 1.6), outG: 0 }
);

add(
  "Auto-Wah",
  "Modulation",
  "Envelope follower -> resonant bandpass + softclip polish + AA LPF (Res on LPF kept moderate).",
  `param a = Sens [0.8, 5.0]
param b = Min [180, 700]
param c = Range [800, 4500]
param d = Res [0.8, 2.8]
env1: type = peak; attack = 0.004; release = 0.11
filter1: type = bandpass; center = b; + = env1; * = c; width = 650
stage1: y = softclip(x, a)
filter2: type = lowpass; cutoff = 8500; resonance = d`,
  { a: p("Sens", 0.8, 5, 2.2), b: p("Min", 180, 700, 280), c: p("Range", 800, 4500, 2600), d: p("Res", 0.8, 2.8, 1.4), outG: 0 }
);

add(
  "Phaser Sweep",
  "Modulation",
  "Stable phaser-ish: HP stays below LP for the whole LFO, dry/wet blend — no band invert, no image slam.",
  `param a = Rate [0.08, 3.0]
param b = Center [300, 1600]
param c = Depth [200, 2200]
param d = Mix [0.25, 0.9]
osc1: shape = sine; freq = a; depth = 1.0
bus wet:
  send: in = 1
  filter1: type = highpass; cutoff = b + (0.5 + 0.5 * osc1) * (c * 0.22); resonance = 0.55
  filter2: type = lowpass; cutoff = b + c * 0.7 + (0.5 + 0.5 * osc1) * (c * 0.28); resonance = 0.45
  stage1: y = softclip(x, 1.12)
  filter3: type = lowpass; cutoff = 12000; resonance = 0.25
out: main = 1-d; wet = d`,
  {
    a: p("Rate", 0.08, 3, 0.45),
    b: p("Center", 300, 1600, 520),
    c: p("Depth", 200, 2200, 1400),
    d: p("Mix", 0.25, 0.9, 0.55),
    outG: 0,
  }
);

add(
  "Chorus Delay",
  "Modulation",
  "Real short delay chorus + dual-rate LFO thickness — not fake AM-only chorus.",
  `param a = Rate [0.25, 3.5]
param b = Depth [0.12, 0.5]
param c = Time [8, 32]
param d = Mix [0.18, 0.55]
osc1: shape = sine; freq = a; depth = 1.0
osc2: shape = sine; freq = a * 1.31; depth = 1.0
stage1: y = tube(x, 1.35)
delay1: time = c; feedback = 0.14; mix = d; damp = 9500
stage2: y = softclip(y * (1.0 - b + b * 0.5 * ((0.5 + 0.5 * osc1) + (0.5 + 0.5 * osc2))), 1.08)`,
  {
    a: p("Rate", 0.25, 3.5, 1.05),
    b: p("Depth", 0.12, 0.5, 0.32),
    c: p("Time", 8, 32, 18),
    d: p("Mix", 0.18, 0.55, 0.34),
    outG: 0,
  }
);

// =============================================================================
// FILTER
// =============================================================================
add(
  "Low Pass Sweep",
  "Filter",
  "Musical LFO lowpass: cutoff walks Min to Min+Depth (never Min-Depth). Rate is a note grid.",
  `param a = Rate [1/1, 1/16]
param b = Min [180, 800]
param c = Depth [400, 5000]
param d = Res [0.4, 3.0]
osc1: shape = sine; sync = a; depth = 1.0
filter1: type = lowpass; cutoff = b + (0.5 + 0.5 * osc1) * c; resonance = d
stage1: y = diode(x, 1.08)`,
  { a: p("Rate", 0, 1, 0.45), b: p("Min", 180, 800, 420), c: p("Depth", 400, 5000, 2400), d: p("Res", 0.4, 3, 1.4), outG: 0 }
);

add(
  "High Pass Gate",
  "Filter",
  "Envelope opens HPF for rhythmic filtering + softclip ceiling + mild AA LPF.",
  `param a = Floor [50, 350]
param b = Range [400, 7000]
param c = Attack [0.001, 0.04]
param d = Release [0.04, 0.35]
env1: type = peak; attack = c; release = d
filter1: type = highpass; cutoff = a; + = env1; * = b; resonance = 1.0
stage1: y = softclip(x, 1.2)
filter2: type = lowpass; cutoff = 11000; resonance = 0.3`,
  { a: p("Floor", 50, 350, 100), b: p("Range", 400, 7000, 3000), c: p("Attack", 0.001, 0.04, 0.006), d: p("Release", 0.04, 0.35, 0.14), outG: 0 }
);

add(
  "Reso Peak",
  "Filter",
  "Resonant peak filter - Q limited for stability.",
  `param a = Cutoff [150, 5000]
param b = Res [0.6, 3.5]
param c = Drive [1.0, 3.5]
filter1: type = lowpass; cutoff = a; resonance = b
stage1: y = tube(x, c)`,
  { a: p("Cutoff", 150, 5000, 850), b: p("Res", 0.6, 3.5, 2.2), c: p("Drive", 1, 3.5, 1.8), outG: 0 }
);

add(
  "Bandpass Vocal",
  "Filter",
  "Formant-ish bandpass for vocal/synth body.",
  `param a = Center [350, 2800]
param b = Width [100, 1000]
param c = Drive [0.9, 2.8]
filter1: type = bandpass; center = a; width = b
stage1: y = diode(x, c)`,
  { a: p("Center", 350, 2800, 950), b: p("Width", 100, 1000, 380), c: p("Drive", 0.9, 2.8, 1.5), outG: 0 }
);

// =============================================================================
// DYNAMICS
// =============================================================================
add(
  "Heavy Comp",
  "Dynamics",
  "FET-ish compressor + softclip makeup ceiling + mild AA LPF (no bare hard peaks).",
  `param a = Threshold [-36.0, -8.0]
param b = Ratio [2.5, 12.0]
param c = Attack [0.001, 0.03]
param d = Makeup [1.0, 3.5]
comp1: threshold = a; ratio = b; attack = c; release = 0.11
stage1: y = softclip(x * d, 1.1)
filter1: type = lowpass; cutoff = 14000; resonance = 0.25`,
  { a: p("Threshold", -36, -8, -16), b: p("Ratio", 2.5, 12, 5.5), c: p("Attack", 0.001, 0.03, 0.004), d: p("Makeup", 1, 3.5, 1.9), outG: 0 }
);

add(
  "Optical Comp",
  "Dynamics",
  "Slower opto-style attack/release, gentle ratio.",
  `param a = Threshold [-28.0, -6.0]
param b = Ratio [1.8, 6.0]
param c = Makeup [1.0, 2.5]
comp1: threshold = a; ratio = b; attack = 0.02; release = 0.28
stage1: y = diode(x * c, 1.05)`,
  { a: p("Threshold", -28, -6, -14), b: p("Ratio", 1.8, 6, 3), c: p("Makeup", 1, 2.5, 1.45), outG: 0 }
);

add(
  "Parallel Crush",
  "Dynamics",
  "NY parallel: dry + soft-pre hard crush + LPF recovery (transparent peaks).",
  `param a = Drive [1.5, 10.0]
param b = Blend [0.25, 0.9]
param c = Level [0.45, 1.2]
stage1: y = x * c
bus crush:
  send: in = 1
  stage2: y = hardclip(softclip(tube(x, a), 1.15), 0.65) * c
  filter1: type = lowpass; cutoff = 10000; resonance = 0.3
out: main = 1-b; crush = b`,
  { a: p("Drive", 1.5, 10, 4.5), b: p("Blend", 0.25, 0.9, 0.58), c: p("Level", 0.45, 1.2, 0.85), outG: 0 }
);

add(
  "Transient Bite",
  "Dynamics",
  "Peak envelope boosts softclip drive on attacks + mild AA LPF.",
  `param a = Drive [1.2, 6.0]
param b = Attack [0.001, 0.015]
param c = Release [0.02, 0.2]
param d = Amount [0.25, 0.95]
env1: type = peak; attack = b; release = c
stage1: y = softclip(x * (a + env1 * a * d), 1.0)
filter1: type = lowpass; cutoff = 12000; resonance = 0.28`,
  { a: p("Drive", 1.2, 6, 2.8), b: p("Attack", 0.001, 0.015, 0.003), c: p("Release", 0.02, 0.2, 0.07), d: p("Amount", 0.25, 0.95, 0.65), outG: 0 }
);

add(
  "OTT Smash",
  "Dynamics",
  "Xfer-style 3-band upward + downward compressor. Depth is the smash. Time is the envelope. Insert on a bus or a group.",
  `param a = Depth [0.18, 1.0]
param b = Time [0.06, 0.82]
param c = Input [0.7, 2.8]
param d = Low [0.4, 1.2]
param e = Mid [0.4, 1.2]
param f = High [0.4, 1.2]
ott1: depth = a; time = b; in = c; low = d; mid = e; high = f
limit1: ceiling = -0.4; release = 0.05`,
  {
    a: p("Depth", 0.18, 1.0, 0.52),
    b: p("Time", 0.06, 0.82, 0.3),
    c: p("Input", 0.7, 2.8, 1.15),
    d: p("Low", 0.4, 1.2, 1.0),
    e: p("Mid", 0.4, 1.2, 0.92),
    f: p("High", 0.4, 1.2, 1.05),
    tags: ["ott", "edm", "glue", "multiband", "compressor"],
    outG: 0,
  }
);

// =============================================================================
// GUITAR
// =============================================================================
add(
  "Amp Crunch",
  "Guitar",
  "Edge-of-breakup: mild tube, 1.1 kHz peak, open cab — between clean and Marshall.",
  `param a = Drive [0.9, 5.5]
param b = Tone [900, 7000]
param c = Level [0.4, 1.2]
param d = Width [0.0, 1.2]
filter1: type = highpass; cutoff = 70; resonance = 0.3
stage1: y = tube(x, a * 0.7)
eq1: type = peak; freq = 1100; q = 0.9; gain = 3.5
stage2: y = softclip(y, 1.05) * c
filter2: type = lowpass; cutoff = b; resonance = 0.38
widen1: width = d`,
  { a: p("Drive", 0.9, 5.5, 2.2), b: p("Tone", 900, 7000, 3600), c: p("Level", 0.4, 1.2, 0.88), d: p("Width", 0.0, 1.2, 0.0), outG: 0 }
);

add(
  "Metal Gate",
  "Guitar",
  "Tight metal: HPF -> tube cascade -> soft-pre hard knee -> cab LPF (filter after clip).",
  `param a = Drive [4.0, 14.0]
param b = LowCut [70, 350]
param c = Level [0.2, 0.85]
param d = Width [0.0, 1.2]
gate1: threshold = -42; hyst = 8; hold = 0.025; range = -80
filter1: type = highpass; cutoff = b; resonance = 0.5
stage1: y = tube(x, a * 0.45)
stage2: y = hardclip(softclip(tube(y, a * 0.5), 1.1), 0.5)
filter2: type = lowpass; cutoff = 4800; resonance = 0.6
stage3: y = y * c
widen1: width = d
ir1: mix = 0.5; gain = 0
limit1: ceiling = -0.3; release = 0.08`,
  { a: p("Drive", 4, 14, 8.5), b: p("LowCut", 70, 350, 140), c: p("Level", 0.2, 0.85, 0.45), d: p("Width", 0.0, 1.2, 0.0), tags: ["gate", "ir"], outG: 0, irs: { ir1: "Medium IR 01.wav" } }
);

add(
  "Blues Breakup",
  "Guitar",
  "Touch-sensitive blues OD with bias.",
  `param a = Drive [1.0, 5.5]
param b = Bias [0.0, 0.22]
param c = Level [0.5, 1.25]
param d = Width [0.0, 1.2]
stage1: y = tube(x + b * 0.5, a) * c
filter1: type = lowpass; cutoff = 5500; resonance = 0.4
widen1: width = d`,
  { a: p("Drive", 1, 5.5, 2.5), b: p("Bias", 0, 0.22, 0.07), c: p("Level", 0.5, 1.25, 0.95), d: p("Width", 0.0, 1.2, 0.0), outG: 0 }
);

add(
  "Lead Boost",
  "Guitar",
  "Lead boost into saturated power stage + presence shelf (not a darker LPF).",
  `param a = Boost [1.5, 7.0]
param b = Presence [1800, 7000]
param c = Level [0.4, 1.15]
param d = Width [0.0, 1.2]
filter1: type = highpass; cutoff = 100; resonance = 0.35
stage1: y = tube(x, a)
eq1: type = highshelf; freq = b; q = 0.7; gain = 3.2
stage2: y = diode(y, 1.35) * c
filter2: type = lowpass; cutoff = 6200; resonance = 0.42
widen1: width = d`,
  { a: p("Boost", 1.5, 7, 3.8), b: p("Presence", 1800, 7000, 3400), c: p("Level", 0.4, 1.15, 0.8), d: p("Width", 0.0, 1.2, 0.0), outG: 0 }
);

add(
  "Pedal OD",
  "Guitar",
  "Asymmetric diode pedal: soft pre + diode stack + dark tone — different grit than softclip-only OD.",
  `param a = Drive [1.2, 9.5]
param b = Tone [400, 4200]
param c = Level [0.28, 1.1]
filter1: type = highpass; cutoff = 85; resonance = 0.32
stage1: y = softclip(x, a * 0.55)
stage2: y = diode(y, a * 0.5)
filter2: type = lowpass; cutoff = b; resonance = 0.5
stage3: y = softclip(y, 1.1) * c`,
  { a: p("Drive", 1.2, 9.5, 4.2), b: p("Tone", 400, 4200, 1900), c: p("Level", 0.28, 1.1, 0.68), outG: 0 }
);

add(
  "Riff Saturate",
  "Guitar",
  "Comp into tube for tight riff saturation.",
  `param a = Drive [1.4, 6.5]
param b = Ratio [2.0, 7.0]
param c = Level [0.5, 1.3]
comp1: threshold = -15; ratio = b; attack = 0.007; release = 0.14
stage1: y = tube(x, a) * c`,
  { a: p("Drive", 1.4, 6.5, 3.0), b: p("Ratio", 2, 7, 3.8), c: p("Level", 0.5, 1.3, 0.92), outG: 0 }
);

// =============================================================================
// BASS
// =============================================================================
add(
  "Bass Growl",
  "Bass",
  "Parallel clean + tube dirt, keep low end.",
  `param a = Drive [1.3, 7.0]
param b = Blend [0.25, 0.85]
param c = Level [0.45, 1.25]
filter1: type = highpass; cutoff = 35; resonance = 0.25
stage1: y = x * c
bus dirt:
  send: main = 1
  stage2: y = tube(x, a)
  filter2: type = lowpass; cutoff = 7000; resonance = 0.35
out: main = 1-b; dirt = b`,
  { a: p("Drive", 1.3, 7, 3.2), b: p("Blend", 0.25, 0.85, 0.55), c: p("Level", 0.45, 1.25, 0.9), outG: 0 }
);

add(
  "Bass Folder",
  "Bass",
  "Parallel fold for bass harmonics + diode + LPF (fold always band-limited).",
  `param a = Drive [1.8, 10.0]
param b = Fold [0.25, 0.8]
param c = Mix [0.25, 0.85]
stage1: y = x
bus fold:
  send: in = 1
  stage2: y = fold(x * a, -b, b)
  stage3: y = softclip(diode(y, 1.2), 1.05)
  filter1: type = lowpass; cutoff = 5500; resonance = 0.4
out: main = 1-c; fold = c`,
  { a: p("Drive", 1.8, 10, 4.5), b: p("Fold", 0.25, 0.8, 0.5), c: p("Mix", 0.25, 0.85, 0.6), outG: 0 }
);

add(
  "Sub Push",
  "Bass",
  "Sub-safe mild tube with gentle HPF.",
  `param a = Drive [1.0, 4.0]
param b = HPF [25, 100]
param c = Level [0.6, 1.35]
filter1: type = highpass; cutoff = b; resonance = 0.25
stage1: y = tube(x, a) * c`,
  { a: p("Drive", 1, 4, 2.0), b: p("HPF", 25, 100, 40), c: p("Level", 0.6, 1.35, 1.0), outG: 0 }
);

add(
  "Bass Comp Drive",
  "Bass",
  "Compress then tube - thick DI bass.",
  `param a = Threshold [-26.0, -8.0]
param b = Drive [1.3, 5.0]
param c = Level [0.5, 1.25]
comp1: threshold = a; ratio = 4.5; attack = 0.012; release = 0.2
stage1: y = tube(x, b) * c`,
  { a: p("Threshold", -26, -8, -15), b: p("Drive", 1.3, 5, 2.6), c: p("Level", 0.5, 1.25, 0.95), outG: 0 }
);

// =============================================================================
// VOCALS
// =============================================================================
add(
  "Vocal Grit",
  "Vocals",
  "Broadcast grit: HPF, tube, air LPF.",
  `param a = Drive [1.2, 5.5]
param b = LowCut [80, 280]
param c = Air [2500, 11000]
param d = Level [0.5, 1.25]
filter1: type = highpass; cutoff = b; resonance = 0.35
stage1: y = tube(x, a) * d
filter2: type = lowpass; cutoff = c; resonance = 0.4`,
  { a: p("Drive", 1.2, 5.5, 2.6), b: p("LowCut", 80, 280, 130), c: p("Air", 2500, 11000, 7000), d: p("Level", 0.5, 1.25, 0.92), outG: 0 }
);

add(
  "Radio Voice",
  "Vocals",
  "AM radio bandpass + softclip + light bitcrush + LPF (quantize always filtered).",
  `param a = Center [500, 2200]
param b = Width [180, 900]
param c = Drive [1.2, 4.0]
filter1: type = bandpass; center = a; width = b
stage1: y = softclip(x, c)
stage2: y = bitcrush(y, 10)
filter2: type = lowpass; cutoff = 6500; resonance = 0.4`,
  { a: p("Center", 500, 2200, 1050), b: p("Width", 180, 900, 420), c: p("Drive", 1.2, 4, 2.2), outG: 0 }
);

add(
  "Vocal Comp",
  "Vocals",
  "Vocal bus compressor + soft ceiling.",
  `param a = Threshold [-28.0, -8.0]
param b = Ratio [2.5, 10.0]
param c = Drive [1.0, 2.5]
comp1: threshold = a; ratio = b; attack = 0.005; release = 0.12
stage1: y = diode(x, c)`,
  { a: p("Threshold", -28, -8, -14), b: p("Ratio", 2.5, 10, 5), c: p("Drive", 1, 2.5, 1.35), outG: 0 }
);

add(
  "Whisper Edge",
  "Vocals",
  "Airy HPF into soft tube.",
  `param a = Drive [1.1, 3.5]
param b = HPF [250, 1100]
param c = Level [0.55, 1.35]
filter1: type = highpass; cutoff = b; resonance = 0.55
stage1: y = tube(x, a) * c`,
  { a: p("Drive", 1.1, 3.5, 1.9), b: p("HPF", 250, 1100, 480), c: p("Level", 0.55, 1.35, 1.0), outG: 0 }
);

add(
  "Doubler AM",
  "Vocals",
  "Short slap double with mild AM on delay time + diode grit.",
  `param a = Time [12, 42]
param b = Mix [0.18, 0.55]
param c = Drive [1.0, 2.2]
param d = Rate [0.2, 4.0]
osc1: shape = sine; freq = d; depth = 1.0
stage1: y = diode(x, c)
delay1: time = a + osc1 * 4; feedback = 0.08; mix = b; damp = 7000
filter1: type = lowpass; cutoff = 12000; resonance = 0.25`,
  {
    a: p("Time", 12, 42, 22),
    b: p("Mix", 0.18, 0.55, 0.32),
    c: p("Drive", 1, 2.2, 1.3),
    d: p("Rate", 0.2, 4, 1.4),
    outG: 0,
  }
);

// =============================================================================
// DRUMS
// =============================================================================
add(
  "Drum Smash",
  "Drums",
  "Drum bus: tube -> soft-pre hard ceiling -> glue comp -> diode + mild LPF.",
  `param a = Drive [1.5, 9.0]
param b = Threshold [-26.0, -6.0]
param c = Level [0.4, 1.15]
stage1: y = hardclip(softclip(tube(x, a), 1.15), 0.7)
comp1: threshold = b; ratio = 5.5; attack = 0.002; release = 0.08
stage2: y = diode(y, 1.15) * c
filter1: type = lowpass; cutoff = 11000; resonance = 0.3`,
  { a: p("Drive", 1.5, 9, 4.0), b: p("Threshold", -26, -6, -13), c: p("Level", 0.4, 1.15, 0.75), outG: 0 }
);

add(
  "Kick Punch",
  "Drums",
  "Kick: LPF body, tube punch, fast comp.",
  `param a = Drive [1.3, 6.0]
param b = Tone [90, 700]
param c = Punch [2.0, 8.0]
filter1: type = lowpass; cutoff = b; resonance = 0.7
stage1: y = tube(x, a)
comp1: threshold = -11; ratio = c; attack = 0.002; release = 0.055`,
  { a: p("Drive", 1.3, 6, 2.7), b: p("Tone", 90, 700, 260), c: p("Punch", 2, 8, 4.5), outG: 0 }
);

add(
  "Snare Crack",
  "Drums",
  "Snare crack: HPF -> soft-pre hard ceiling -> body LPF (filter after clip).",
  `param a = Crack [1.5, 8.0]
param b = HPF [140, 550]
param c = Level [0.4, 1.15]
filter1: type = highpass; cutoff = b; resonance = 0.55
stage1: y = hardclip(softclip(x, a), 0.75) * c
filter2: type = lowpass; cutoff = 9500; resonance = 0.35`,
  { a: p("Crack", 1.5, 8, 3.8), b: p("HPF", 140, 550, 240), c: p("Level", 0.4, 1.15, 0.8), outG: 0 }
);

add(
  "Room Crush",
  "Drums",
  "Drum room: tube crush into real small-room reverb + tone LPF.",
  `param a = Drive [1.5, 8.0]
param b = Room [0.2, 0.7]
param c = Mix [0.15, 0.55]
param d = Tone [2000, 12000]
stage1: y = tube(x, a)
reverb1: size = b; decay = 0.42; damp = 0.55; mix = c; width = 0.8
filter1: type = lowpass; cutoff = d; resonance = 0.35`,
  {
    a: p("Drive", 1.5, 8, 3.8),
    b: p("Room", 0.2, 0.7, 0.4),
    c: p("Mix", 0.15, 0.55, 0.32),
    d: p("Tone", 2000, 12000, 7000),
    outG: 0,
  }
);

add(
  "Hat Sizzle",
  "Drums",
  "Cymbal sizzle: steep HPF + softclip (drive capped; soft only - no hardclip on air).",
  `param a = Drive [1.1, 3.5]
param b = HPF [2500, 9000]
param c = Level [0.55, 1.35]
filter1: type = highpass; cutoff = b; resonance = 0.65
stage1: y = softclip(x, a) * c
filter2: type = lowpass; cutoff = 14000; resonance = 0.25`,
  { a: p("Drive", 1.1, 3.5, 1.9), b: p("HPF", 2500, 9000, 4800), c: p("Level", 0.55, 1.35, 1.0), outG: 0 }
);

// =============================================================================
// SYNTH
// =============================================================================
add(
  "Acid Line",
  "Synth",
  "303-ish: env opens LPF with musical Q, then diode.",
  `param a = Res [0.8, 3.4]
param b = Min [90, 500]
param c = Range [500, 4500]
param d = Drive [1.1, 4.0]
env1: type = peak; attack = 0.002; release = 0.16
filter1: type = lowpass; cutoff = b; + = env1; * = c; resonance = a
stage1: y = diode(x, d)`,
  { a: p("Res", 0.8, 3.4, 2.4), b: p("Min", 90, 500, 180), c: p("Range", 500, 4500, 2400), d: p("Drive", 1.1, 4, 2.0), outG: 0 }
);

add(
  "Pad Swell",
  "Synth",
  "Slow tremolo pad with gentle tube.",
  `param a = Rate [0.05, 1.8]
param b = Depth [0.2, 0.8]
param c = Drive [1.0, 2.8]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = tube(x, c)
stage2: y = y * (1.0 - b + b * (0.5 + 0.5 * osc1))`,
  { a: p("Rate", 0.05, 1.8, 0.22), b: p("Depth", 0.2, 0.8, 0.48), c: p("Drive", 1, 2.8, 1.5), outG: 0 }
);

add(
  "Lead Scream",
  "Synth",
  "Lead: tube -> restrained LPF -> softclip ceiling (clip after filter = less alias).",
  `param a = Drive [1.8, 9.0]
param b = Cutoff [500, 7000]
param c = Res [0.6, 2.8]
stage1: y = tube(x, a)
filter1: type = lowpass; cutoff = b; resonance = c
stage2: y = softclip(y, 1.25)`,
  { a: p("Drive", 1.8, 9, 4.5), b: p("Cutoff", 500, 7000, 2400), c: p("Res", 0.6, 2.8, 1.6), outG: 0 }
);

add(
  "PWM Texture",
  "Synth",
  "Saw LFO AM texture on tube core.",
  `param a = Rate [0.4, 11.0]
param b = Depth [0.25, 0.9]
param c = Drive [1.0, 3.5]
osc1: shape = saw; freq = a; depth = 1.0
stage1: y = tube(x, c)
stage2: y = y * (1.0 - b + b * (0.5 + 0.5 * osc1))`,
  { a: p("Rate", 0.4, 11, 3.5), b: p("Depth", 0.25, 0.9, 0.6), c: p("Drive", 1, 3.5, 1.8), outG: 0 }
);

add(
  "Supersaw Dirt",
  "Synth",
  "Softclip dirt + slow AM + LPF recovery (always band-limit after softclip drive).",
  `param a = Drive [1.4, 7.0]
param b = Rate [0.15, 3.5]
param c = Depth [0.1, 0.45]
osc1: shape = sine; freq = b; depth = 1.0
stage1: y = softclip(x, a)
stage2: y = y * (1.0 - c + c * (0.5 + 0.5 * osc1))
filter1: type = lowpass; cutoff = 9000; resonance = 0.35`,
  { a: p("Drive", 1.4, 7, 3.2), b: p("Rate", 0.15, 3.5, 1.0), c: p("Depth", 0.1, 0.45, 0.22), outG: 0 }
);

// =============================================================================
// MASTERING
// =============================================================================
add(
  "Tape Saturate",
  "Mastering",
  "Tape-ish: soft magnetic saturation + HF roll-off.",
  `param a = Drive [1.0, 3.2]
param b = Blend [0.3, 0.9]
param c = HF [5000, 16000]
stage1: y = x
bus tape:
  send: in = 1
  stage2: y = tube(x, a)
  filter1: type = lowpass; cutoff = c; resonance = 0.3
out: main = 1-b; tape = b`,
  { a: p("Drive", 1, 3.2, 1.85), b: p("Blend", 0.3, 0.9, 0.58), c: p("HF", 5000, 16000, 11000), outG: 0 }
);

add(
  "Bus Glue",
  "Mastering",
  "Mix-bus glue: gentle ratio, soft makeup.",
  `param a = Threshold [-20.0, -6.0]
param b = Ratio [1.4, 4.0]
param c = Makeup [1.0, 2.0]
comp1: threshold = a; ratio = b; attack = 0.018; release = 0.25
stage1: y = diode(x * c, 1.02)`,
  { a: p("Threshold", -20, -6, -11), b: p("Ratio", 1.4, 4, 2.2), c: p("Makeup", 1, 2, 1.35), outG: 0 }
);

add(
  "Air Exciter",
  "Mastering",
  "HF excite: HPF -> softclip (low drive) -> blend - soft only, no hardclip on air.",
  `param a = Drive [1.2, 3.5]
param b = Freq [2500, 9000]
param c = Blend [0.2, 0.65]
bus air:
  send: in = 1
  filter1: type = highpass; cutoff = b; resonance = 0.35
  stage1: y = softclip(x, a)
  filter2: type = lowpass; cutoff = 16000; resonance = 0.22
out: main = 1-c; air = c`,
  { a: p("Drive", 1.2, 3.5, 2.0), b: p("Freq", 2500, 9000, 4800), c: p("Blend", 0.2, 0.65, 0.4), outG: 0 }
);

add(
  "Loudness Clip",
  "Mastering",
  "Peak clipper best-practice: softclip -> soft-knee hard ceiling -> gentle AA LPF.",
  `param a = Ceiling [0.55, 0.98]
param b = Drive [1.0, 2.2]
param c = Level [0.75, 1.15]
stage1: y = hardclip(softclip(x, b), a) * c
filter1: type = lowpass; cutoff = 16000; resonance = 0.22`,
  { a: p("Ceiling", 0.55, 0.98, 0.9), b: p("Drive", 1, 2.2, 1.2), c: p("Level", 0.75, 1.15, 0.95), outG: 0 }
);

// =============================================================================
// LO-FI
// =============================================================================
add(
  "Lo-Fi Crush",
  "Lo-Fi",
  "Soft-pre bitcrush + post LPF (quantize always filtered to tame HF trash).",
  `param a = Bits [3.0, 11.0]
param b = Drive [1.0, 3.5]
param c = LPF [500, 7000]
stage1: y = bitcrush(softclip(x, b), a)
filter1: type = lowpass; cutoff = c; resonance = 0.5`,
  { a: p("Bits", 3, 11, 5.5), b: p("Drive", 1, 3.5, 1.7), c: p("LPF", 500, 7000, 2800), outG: 0 }
);

add(
  "Cassette",
  "Lo-Fi",
  "Wobble AM + tube + muted LPF.",
  `param a = Rate [0.25, 3.5]
param b = Depth [0.08, 0.4]
param c = Drive [1.1, 3.2]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = tube(x * (1.0 + osc1 * b), c)
filter1: type = lowpass; cutoff = 4800; resonance = 0.45`,
  { a: p("Rate", 0.25, 3.5, 1.0), b: p("Depth", 0.08, 0.4, 0.22), c: p("Drive", 1.1, 3.2, 1.9), outG: 0 }
);

add(
  "Phone Line",
  "Lo-Fi",
  "Telephone band + soft-pre bitcrush + LPF (band-limit after quantize).",
  `param a = Center [600, 1800]
param b = Width [200, 800]
param c = Bits [4.0, 11.0]
filter1: type = bandpass; center = a; width = b
stage1: y = bitcrush(softclip(x, 1.5), c)
filter2: type = lowpass; cutoff = 4500; resonance = 0.4`,
  { a: p("Center", 600, 1800, 1000), b: p("Width", 200, 800, 450), c: p("Bits", 4, 11, 7), outG: 0 }
);

add(
  "Vinyl Dirt",
  "Lo-Fi",
  "Gentle feedback dirt + roll-off (stable fb).",
  `param a = Drive [1.1, 3.5]
param b = Dirt [0.04, 0.28]
param c = LPF [1500, 8000]
stage1: y = tube(x + y_prev * b, a)
filter1: type = lowpass; cutoff = c; resonance = 0.5`,
  { a: p("Drive", 1.1, 3.5, 1.9), b: p("Dirt", 0.04, 0.28, 0.12), c: p("LPF", 1500, 8000, 3800), outG: 0 }
);

// =============================================================================
// EDM
// =============================================================================
add(
  "Sidechain Pump",
  "EDM",
  "Sine ducking envelope (sidechain pump). Rate is note-grid: 1/4 at 120 BPM = 2 Hz, not milliseconds-as-Hz.",
  `param a = Rate [1/1, 1/16]
param b = Depth [0.4, 1.0]
param c = Floor [0.0, 0.35]
osc1: shape = sine; sync = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0, 1, 0.45), b: p("Depth", 0.4, 1, 0.85), c: p("Floor", 0, 0.35, 0.08), outG: 0 }
);

add(
  "Dubstep Growl",
  "EDM",
  "LFO filter growl + soft-pre hard ceiling + recovery LPF (Q restrained).",
  `param a = Rate [0.5, 9.0]
param b = Min [80, 350]
param c = Depth [400, 4200]
param d = Drive [1.8, 8.0]
osc1: shape = sine; freq = a; depth = 1.0
filter1: type = lowpass; cutoff = b; + = osc1; * = c; resonance = 2.0
stage1: y = hardclip(softclip(tube(x, d), 1.2), 0.55)
filter2: type = lowpass; cutoff = 7500; resonance = 0.4`,
  { a: p("Rate", 0.5, 9, 3.2), b: p("Min", 80, 350, 130), c: p("Depth", 400, 4200, 2000), d: p("Drive", 1.8, 8, 4.5), outG: 0 }
);

add(
  "Riser Noise",
  "EDM",
  "HPF riser + softclip + saw AM + mild AA LPF.",
  `param a = Cutoff [250, 7500]
param b = Rate [0.2, 5.0]
param c = Drive [1.0, 3.2]
osc1: shape = saw; freq = b; depth = 1.0
filter1: type = highpass; cutoff = a; resonance = 0.75
stage1: y = softclip(x, c) * (0.55 + 0.45 * (0.5 + 0.5 * osc1))
filter2: type = lowpass; cutoff = 12000; resonance = 0.3`,
  { a: p("Cutoff", 250, 7500, 1600), b: p("Rate", 0.2, 5, 1.4), c: p("Drive", 1, 3.2, 1.7), outG: 0 }
);

add(
  "Club Clip",
  "EDM",
  "Club loudness: softclip -> hard ceiling -> AA LPF (same recipe as Loudness Clip, lower ceiling).",
  `param a = Drive [1.2, 4.5]
param b = Ceiling [0.4, 0.9]
param c = Level [0.5, 1.05]
stage1: y = hardclip(softclip(x, a), b) * c
filter1: type = lowpass; cutoff = 12000; resonance = 0.28`,
  { a: p("Drive", 1.2, 4.5, 2.4), b: p("Ceiling", 0.4, 0.9, 0.62), c: p("Level", 0.5, 1.05, 0.78), outG: 0 }
);

// =============================================================================
// AMBIENT / CREATIVE / SOUND DESIGN
// =============================================================================
add(
  "Shimmer Drive",
  "Ambient",
  "Tube drive into a bright high-passed hall; slow AM on the tail.",
  `param a = Drive [1.0, 3.2]
param b = Size [0.35, 0.9]
param c = Mix [0.2, 0.7]
param d = Rate [0.05, 1.2]
param e = Depth [0.12, 0.5]
param f = Damp [0.15, 0.65]
osc1: shape = sine; freq = d; depth = 1.0
stage1: y = tube(x, a)
bus shine:
  send: in = 1
  reverb1: size = b; decay = 0.78; damp = f; mix = 1; width = 0.95
  stage2: y = x * (1.0 - e + e * (0.5 + 0.5 * osc1))
  filter1: type = highpass; cutoff = 1800; resonance = 0.25
  filter2: type = lowpass; cutoff = 12000; resonance = 0.25
out: main = 1-c; shine = c`,
  {
    a: p("Drive", 1, 3.2, 1.7),
    b: p("Size", 0.35, 0.9, 0.62),
    c: p("Mix", 0.2, 0.7, 0.42),
    d: p("Rate", 0.05, 1.2, 0.18),
    e: p("Depth", 0.12, 0.5, 0.32),
    f: p("Damp", 0.15, 0.65, 0.38),
    outG: 0,
  }
);

add(
  "Drone Layer",
  "Ambient",
  "Stable feedback drone with LFO level (fb < 0.55) + recovery LPF.",
  `param a = Drive [1.0, 3.0]
param b = Feedback [0.08, 0.52]
param c = Rate [0.05, 1.5]
osc1: shape = sine; freq = c; depth = 1.0
stage1: y = tube(x + y_prev * b, a)
stage2: y = y * (0.75 + 0.25 * (0.5 + 0.5 * osc1))
filter1: type = lowpass; cutoff = 9000; resonance = 0.3`,
  { a: p("Drive", 1, 3, 1.6), b: p("Feedback", 0.08, 0.52, 0.28), c: p("Rate", 0.05, 1.5, 0.12), outG: 0 }
);

add(
  "Crystal Edge",
  "Ambient",
  "Air crystal: HPF -> softclip blend (soft only) + mild AA LPF.",
  `param a = Freq [1800, 10000]
param b = Drive [1.2, 3.8]
param c = Blend [0.25, 0.75]
bus air:
  send: in = 1
  filter1: type = highpass; cutoff = a; resonance = 0.5
  stage1: y = softclip(x, b)
  filter2: type = lowpass; cutoff = 15000; resonance = 0.22
out: main = 1-c; air = c`,
  { a: p("Freq", 1800, 10000, 4200), b: p("Drive", 1.2, 3.8, 2.2), c: p("Blend", 0.25, 0.75, 0.5), outG: 0 }
);

add(
  "Wide Motion",
  "Ambient",
  "Opposite-phase AM L/R for width.",
  `param a = Rate [0.08, 2.5]
param b = Depth [0.15, 0.7]
param c = Drive [1.0, 2.6]
osc1: shape = sine; freq = a; depth = 1.0
split1: type = leftright {
  left {
    stage1: y = tube(x, c) * (1.0 - b + b * (0.5 + 0.5 * osc1))
  }
  right {
    stage2: y = tube(x, c) * (1.0 - b + b * (0.5 - 0.5 * osc1))
  }
}`,
  { a: p("Rate", 0.08, 2.5, 0.35), b: p("Depth", 0.15, 0.7, 0.42), c: p("Drive", 1, 2.6, 1.45), outG: 0 }
);

add(
  "Glitch Gate",
  "Creative",
  "Fast square gate + softclip + mild LPF (gate edges need soft ceiling).",
  `param a = Rate [5.0, 28.0]
param b = Depth [0.65, 1.0]
param c = Drive [1.0, 3.0]
osc1: shape = square; freq = a; depth = 1.0
stage1: y = softclip(x, c) * (1.0 - b + b * (0.5 + 0.5 * osc1))
filter1: type = lowpass; cutoff = 11000; resonance = 0.3`,
  { a: p("Rate", 5, 28, 11), b: p("Depth", 0.65, 1, 0.95), c: p("Drive", 1, 3, 1.6), outG: 0 }
);

add(
  "Feedback Screamer",
  "Creative",
  "Controlled feedback (fb capped) -> LPF -> softclip ceiling (filter before/around clip).",
  `param a = Drive [1.3, 6.0]
param b = Feedback [0.15, 0.55]
param c = LPF [700, 7000]
stage1: y = tube(x + y_prev * b, a)
filter1: type = lowpass; cutoff = c; resonance = 0.95
stage2: y = softclip(y, 1.2)`,
  { a: p("Drive", 1.3, 6, 3.0), b: p("Feedback", 0.15, 0.55, 0.36), c: p("LPF", 700, 7000, 2800), outG: 0 }
);

add(
  "Fold Universe",
  "Creative",
  "Deep fold -> softclip recovery -> lower LPF (fold is HF-heavy; band-limit hard).",
  `param a = Drive [2.5, 12.0]
param b = Fold [0.15, 0.7]
param c = Level [0.25, 0.9]
stage1: y = fold(x * a, -b, b)
stage2: y = softclip(y, 1.35) * c
filter1: type = lowpass; cutoff = 7000; resonance = 0.35`,
  { a: p("Drive", 2.5, 12, 6.5), b: p("Fold", 0.15, 0.7, 0.38), c: p("Level", 0.25, 0.9, 0.55), outG: 0 }
);

add(
  "Noise Blow",
  "Sound Design",
  "Env HPF blast into diode sat.",
  `param a = Floor [120, 900]
param b = Range [800, 10000]
param c = Drive [1.3, 5.5]
env1: type = peak; attack = 0.002; release = 0.18
filter1: type = highpass; cutoff = a; + = env1; * = b; resonance = 1.2
stage1: y = diode(x, c)`,
  { a: p("Floor", 120, 900, 320), b: p("Range", 800, 10000, 4500), c: p("Drive", 1.3, 5.5, 2.8), outG: 0 }
);

add(
  "Formant Crush",
  "Sound Design",
  "Dual formant peaks + softclip + mild LPF (clip between formants stays band-limited).",
  `param a = F1 [320, 1100]
param b = F2 [1000, 2800]
param c = Drive [1.3, 4.5]
filter1: type = bandpass; center = a; width = 220
stage1: y = softclip(x, c)
filter2: type = bandpass; center = b; width = 320
filter3: type = lowpass; cutoff = 9000; resonance = 0.3`,
  { a: p("F1", 320, 1100, 580), b: p("F2", 1000, 2800, 1500), c: p("Drive", 1.3, 4.5, 2.4), outG: 0 }
);

add(
  "Alien Ring",
  "Sound Design",
  "Low-frequency ring mod + tube.",
  `param a = Freq [8.0, 160.0]
param b = Depth [0.45, 1.0]
param c = Drive [1.1, 4.0]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = tube(x * (1.0 - b + b * osc1), c)`,
  { a: p("Freq", 8, 160, 32), b: p("Depth", 0.45, 1, 0.85), c: p("Drive", 1.1, 4, 2.1), outG: 0 }
);

add(
  "Stutter Gate",
  "Sound Design",
  "Extreme square stutter.",
  `param a = Rate [6.0, 36.0]
param b = Depth [0.75, 1.0]
osc1: shape = square; freq = a; depth = 1.0
stage1: y = x * (1.0 - b + b * (0.5 + 0.5 * osc1))`,
  { a: p("Rate", 6, 36, 14), b: p("Depth", 0.75, 1, 1), outG: 0 }
);

add(
  "Comb Taste",
  "Sound Design",
  "Metallic comb via very short delay + high feedback (true delay line, not y_prev).",
  `param a = Time [3, 28]
param b = Feedback [0.55, 0.9]
param c = Mix [0.25, 0.85]
param d = Drive [1.0, 2.8]
stage1: y = tube(x, d)
delay1: time = a; feedback = b; mix = c; damp = 7000
filter1: type = lowpass; cutoff = 10000; resonance = 0.4`,
  {
    a: p("Time", 3, 28, 11),
    b: p("Feedback", 0.55, 0.9, 0.72),
    c: p("Mix", 0.25, 0.85, 0.55),
    d: p("Drive", 1, 2.8, 1.5),
    outG: 0,
  }
);

// =============================================================================
// MULTI-STAGE + y_prev (high performance topology)
// Rule: at most ONE stage uses y_prev/x_prev (scalar only there).
// Pre/post stages: pure softclip/tube/diode (SIMD). Filters: always block path.
// Never put Osc + multi feedback stages together if you care about CPU.
// =============================================================================
add(
  "Preamp Regen Stack",
  "Guitar",
  "HPF -> tube (SIMD) -> ONE y_prev regen stage -> cab LPF -> soft power (SIMD). Fast multi-stage.",
  `param a = Drive [1.2, 7.0]
param b = Regen [0.08, 0.48]
param c = Tone [800, 6500]
param d = Level [0.35, 1.15]
filter1: type = highpass; cutoff = 70; resonance = 0.35
stage1: y = tube(x, a * 0.55)
stage2: y = tube(x + y_prev * b, a * 0.4)
filter2: type = lowpass; cutoff = c; resonance = 0.45
stage3: y = softclip(y, 1.15) * d`,
  {
    a: p("Drive", 1.2, 7, 3.4),
    b: p("Regen", 0.08, 0.48, 0.28),
    c: p("Tone", 800, 6500, 3200),
    d: p("Level", 0.35, 1.15, 0.8),
    outG: 0,
  }
);

add(
  "Slapback Drive",
  "Guitar",
  "Touch-sensitive OD into a real short delay slap (80–140 ms), damped feedback, diode polish.",
  `param a = Drive [1.0, 6.5]
param b = Time [60, 160]
param c = Mix [0.12, 0.45]
param d = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 80; resonance = 0.3
stage1: y = softclip(x, a * 0.75)
delay1: time = b; feedback = 0.18; mix = c; damp = 4200
stage2: y = diode(y, 1.15) * d
filter2: type = lowpass; cutoff = 7500; resonance = 0.35`,
  {
    a: p("Drive", 1, 6.5, 3.0),
    b: p("Time", 60, 160, 105),
    c: p("Mix", 0.12, 0.45, 0.28),
    d: p("Level", 0.4, 1.2, 0.88),
    outG: 0,
    mix: 1,
  }
);

add(
  "Cascade Loop Dirt",
  "Distortion",
  "3-stage dirt: softclip -> feedback tube (only prev stage) -> diode polish + LPF. One scalar stage.",
  `param a = Drive [1.5, 9.0]
param b = Loop [0.1, 0.5]
param c = Tone [500, 6000]
param d = Level [0.3, 1.05]
filter1: type = highpass; cutoff = 65; resonance = 0.35
stage1: y = softclip(x, a * 0.45)
stage2: y = tube(x + y_prev * b, a * 0.5)
stage3: y = diode(y, 1.35)
filter2: type = lowpass; cutoff = c; resonance = 0.5
stage4: y = softclip(y, 1.05) * d`,
  {
    a: p("Drive", 1.5, 9, 4.2),
    b: p("Loop", 0.1, 0.5, 0.3),
    c: p("Tone", 500, 6000, 2800),
    d: p("Level", 0.3, 1.05, 0.7),
    outG: 0,
  }
);

add(
  "Bass Regen Growl",
  "Bass",
  "Keep lows: tube pre (SIMD) + single y_prev regen blend + LPF. Low CPU, thick DI.",
  `param a = Drive [1.2, 6.0]
param b = Regen [0.06, 0.4]
param c = Blend [0.2, 0.85]
param d = Level [0.5, 1.25]
filter1: type = highpass; cutoff = 30; resonance = 0.25
stage1: y = tube(x, a * 0.5)
stage2: y = lerp(x, softclip(x + y_prev * b, 1.3), c) * d
filter2: type = lowpass; cutoff = 6500; resonance = 0.35`,
  {
    a: p("Drive", 1.2, 6, 2.8),
    b: p("Regen", 0.06, 0.4, 0.2),
    c: p("Blend", 0.2, 0.85, 0.55),
    d: p("Level", 0.5, 1.25, 0.95),
    outG: 0,
  }
);

add(
  "Vocal Warm Loop",
  "Vocals",
  "Broadcast grit with tiny regen for body. HPF -> tube -> mild y_prev softclip -> air LPF.",
  `param a = Drive [1.1, 4.5]
param b = Body [0.04, 0.32]
param c = Air [3000, 12000]
param d = Level [0.55, 1.25]
filter1: type = highpass; cutoff = 100; resonance = 0.3
stage1: y = tube(x, a)
stage2: y = softclip(x + y_prev * b, 1.1)
filter2: type = lowpass; cutoff = c; resonance = 0.35
stage3: y = diode(y, 1.05) * d`,
  {
    a: p("Drive", 1.1, 4.5, 2.2),
    b: p("Body", 0.04, 0.32, 0.14),
    c: p("Air", 3000, 12000, 8000),
    d: p("Level", 0.55, 1.25, 0.95),
    outG: 0,
  }
);

add(
  "Drum Bus Regen",
  "Drums",
  "Bus glue dirt: softclip smash -> controlled y_prev heat -> LPF. One feedback stage only.",
  `param a = Drive [1.3, 7.0]
param b = Heat [0.08, 0.45]
param c = Glue [1200, 10000]
param d = Level [0.4, 1.1]
stage1: y = softclip(x, a * 0.6)
stage2: y = hardclip(softclip(x + y_prev * b, 1.2), 0.85)
filter1: type = lowpass; cutoff = c; resonance = 0.35
stage3: y = diode(y, 1.1) * d`,
  {
    a: p("Drive", 1.3, 7, 3.5),
    b: p("Heat", 0.08, 0.45, 0.22),
    c: p("Glue", 1200, 10000, 5500),
    d: p("Level", 0.4, 1.1, 0.78),
    outG: 0,
  }
);

add(
  "Synth Feedback Lead",
  "Synth",
  "Lead stack: tube pre -> y_prev scream (capped) -> tone LPF -> soft ceiling. No Osc = max speed.",
  `param a = Drive [1.5, 8.0]
param b = Feed [0.12, 0.52]
param c = Cutoff [600, 7000]
param d = Level [0.3, 1.0]
stage1: y = tube(x, a * 0.55)
stage2: y = tube(x + y_prev * b, a * 0.45)
filter1: type = lowpass; cutoff = c; resonance = 0.55
stage3: y = softclip(y, 1.2) * d`,
  {
    a: p("Drive", 1.5, 8, 4.0),
    b: p("Feed", 0.12, 0.52, 0.32),
    c: p("Cutoff", 600, 7000, 2800),
    d: p("Level", 0.3, 1.0, 0.7),
    outG: 0,
  }
);

add(
  "Dual Path Regen",
  "Creative",
  "Pre softclip (SIMD) + y_prev path blended in ONE stage. Teaches: prev in a single stage only.",
  `param a = Drive [1.2, 6.5]
param b = Regen [0.1, 0.5]
param c = Blend [0.2, 0.9]
param d = Tone [1000, 9000]
stage1: y = softclip(x, a * 0.4)
stage2: y = lerp(x, softclip(x + y_prev * b, a * 0.55), c)
filter1: type = lowpass; cutoff = d; resonance = 0.35`,
  {
    a: p("Drive", 1.2, 6.5, 3.2),
    b: p("Regen", 0.1, 0.5, 0.28),
    c: p("Blend", 0.2, 0.9, 0.6),
    d: p("Tone", 1000, 9000, 5000),
    outG: 0,
  }
);

add(
  "Tight Metal Regen",
  "Guitar",
  "Tight metal: HPF -> dual tube (SIMD) -> ONE hard-knee regen -> cab LPF. High gain, still hybrid-fast.",
  `param a = Gain [4.0, 14.0]
param b = Regen [0.05, 0.35]
param c = LowCut [80, 300]
param d = Level [0.2, 0.85]
filter1: type = highpass; cutoff = c; resonance = 0.5
stage1: y = tube(x, a * 0.4)
stage2: y = tube(y, a * 0.45)
stage3: y = hardclip(softclip(x + y_prev * b, 1.15), 0.55)
filter2: type = lowpass; cutoff = 4800; resonance = 0.55
stage4: y = y * d`,
  {
    a: p("Gain", 4, 14, 8.5),
    b: p("Regen", 0.05, 0.35, 0.16),
    c: p("LowCut", 80, 300, 140),
    d: p("Level", 0.2, 0.85, 0.48),
    outG: 0,
  }
);

add(
  "Stereo Guitar Wall",
  "Guitar",
  "Two amps from one mono DI: left Mesa-style (dark, dense), right 5150-style (tight presence). Works with a mono guitar in (silent side is copied).",
  `param a = GainL [3.5, 12.0]
param b = GainR [3.5, 12.0]
param c = Tight [55, 200]
param d = Presence [2800, 7000]
param e = Level [0.28, 0.95]
gate1: threshold = -46; hyst = 7; hold = 0.03; range = -80
split1: type = leftright {
  left {
    filter1: type = highpass; cutoff = c; resonance = 0.55
    stage1: y = tube(x, a * 0.5)
    stage2: y = tube(y, a * 0.58)
    eq1: type = peak; freq = 780; q = 1.05; gain = 3.8
    stage3: y = hardclip(softclip(y, 1.5), 0.68)
    filter2: type = lowpass; cutoff = 4500; resonance = 0.52
    stage4: y = diode(y, 1.5) * e
  }
  right {
    filter3: type = highpass; cutoff = c * 1.12; resonance = 0.58
    stage5: y = tube(x, b * 0.46)
    stage6: y = tube(y, b * 0.55)
    eq2: type = highshelf; freq = d; q = 0.7; gain = 3.6
    stage7: y = hardclip(softclip(y, 1.48), 0.7)
    filter4: type = lowpass; cutoff = 4200; resonance = 0.48
    stage8: y = diode(y, 1.48) * e
  }
}
ir1: mix = 0.5; gain = 0
limit1: ceiling = -0.3; release = 0.08`,
  {
    a: p("GainL", 3.5, 12, 7.6),
    b: p("GainR", 3.5, 12, 7.2),
    c: p("Tight", 55, 200, 95),
    d: p("Presence", 2800, 7000, 4800),
    e: p("Level", 0.28, 0.95, 0.52),
    tags: ["metal", "guitar", "amp", "stereo", "gate", "ir"],
    outG: 0,
    irs: { ir1: "American IR 01.wav" },
  }
);

add(
  "Tape Echo Dirt",
  "Lo-Fi",
  "Real tape-style echo: tube preamp, medium delay, high feedback, dark damp, soft ceiling.",
  `param a = Drive [1.1, 4.0]
param b = Time [180, 520]
param c = Feedback [0.25, 0.72]
param d = Age [800, 5500]
stage1: y = tube(x, a)
delay1: time = b; feedback = c; mix = 0.42; damp = d
stage2: y = softclip(y, 1.12)
filter1: type = lowpass; cutoff = 11000; resonance = 0.3`,
  {
    a: p("Drive", 1.1, 4, 2.0),
    b: p("Time", 180, 520, 320),
    c: p("Feedback", 0.25, 0.72, 0.48),
    d: p("Age", 800, 5500, 2800),
    outG: 0,
    mix: 1,
  }
);

// =============================================================================
// DELAY (true delay lines)
// =============================================================================
add(
  "Slap Echo",
  "Delay",
  "Classic slapback: 70–150 ms, low feedback, bright-ish damp — vocals/guitar.",
  `param a = Time [70, 150]
param b = Feedback [0.0, 0.35]
param c = Mix [0.15, 0.5]
param d = Damp [2500, 12000]
stage1: y = softclip(x, 1.08)
delay1: time = a; feedback = b; mix = c; damp = d`,
  {
    a: p("Time", 70, 150, 105),
    b: p("Feedback", 0, 0.35, 0.12),
    c: p("Mix", 0.15, 0.5, 0.3),
    d: p("Damp", 2500, 12000, 7000),
    outG: 0,
    mix: 1,
  }
);

add(
  "Eighth Note Echo",
  "Delay",
  "Tempo-synced 1/8 delay, moderate feedback, ping-pong stereo.",
  `param a = Feedback [0.2, 0.65]
param b = Mix [0.18, 0.55]
param c = Damp [1000, 10000]
param d = Drive [1.0, 2.2]
stage1: y = softclip(x, d)
delay1: sync = 1/8; feedback = a; mix = b; damp = c; pingpong = true`,
  {
    a: p("Feedback", 0.2, 0.65, 0.4),
    b: p("Mix", 0.18, 0.55, 0.35),
    c: p("Damp", 1000, 10000, 5500),
    d: p("Drive", 1, 2.2, 1.2),
    outG: 0,
  }
);

add(
  "Quarter Dub",
  "Delay",
  "Tempo 1/4 dub delay: dark damp, long feedback for rhythmic trails.",
  `param a = Feedback [0.4, 0.85]
param b = Mix [0.2, 0.55]
param c = Damp [400, 4000]
param d = Drive [1.0, 3.0]
stage1: y = tube(x, d)
delay1: sync = 1/4; feedback = a; mix = b; damp = c
filter1: type = lowpass; cutoff = 11000; resonance = 0.28`,
  {
    a: p("Feedback", 0.4, 0.85, 0.62),
    b: p("Mix", 0.2, 0.55, 0.38),
    c: p("Damp", 400, 4000, 1800),
    d: p("Drive", 1, 3, 1.55),
    outG: 0,
  }
);

add(
  "Analog Delay",
  "Delay",
  "Free-time analog-style delay with tube pre and age damp.",
  `param a = Time [120, 700]
param b = Feedback [0.2, 0.75]
param c = Mix [0.2, 0.55]
param d = Age [600, 7000]
stage1: y = tube(x, 1.6)
delay1: time = a; feedback = b; mix = c; damp = d
stage2: y = softclip(y, 1.1)`,
  {
    a: p("Time", 120, 700, 340),
    b: p("Feedback", 0.2, 0.75, 0.45),
    c: p("Mix", 0.2, 0.55, 0.36),
    d: p("Age", 600, 7000, 3200),
    outG: 0,
  }
);

add(
  "Ping Pong Wide",
  "Delay",
  "Wide stereo ping-pong at dotted-ish free time with soft pre.",
  `param a = Time [180, 480]
param b = Feedback [0.25, 0.7]
param c = Mix [0.2, 0.5]
param d = Damp [1500, 9000]
stage1: y = softclip(x, 1.1)
delay1: time = a; feedback = b; mix = c; damp = d; pingpong = true`,
  {
    a: p("Time", 180, 480, 280),
    b: p("Feedback", 0.25, 0.7, 0.42),
    c: p("Mix", 0.2, 0.5, 0.34),
    d: p("Damp", 1500, 9000, 5000),
    outG: 0,
  }
);

// =============================================================================
// REVERB (algorithmic Freeverb-style)
// =============================================================================
add(
  "Studio Room",
  "Reverb",
  "Tight recording-room reverb — small size, short decay, musical damp.",
  `param a = Size [0.12, 0.55]
param b = Decay [0.15, 0.55]
param c = Damp [0.25, 0.75]
param d = Mix [0.1, 0.45]
reverb1: size = a; decay = b; damp = c; mix = d; width = 0.75`,
  {
    a: p("Size", 0.12, 0.55, 0.32),
    b: p("Decay", 0.15, 0.55, 0.35),
    c: p("Damp", 0.25, 0.75, 0.48),
    d: p("Mix", 0.1, 0.45, 0.28),
    outG: 0,
  }
);

add(
  "Concert Hall",
  "Reverb",
  "Large hall: high size/decay, moderate damp, full width.",
  `param a = Size [0.5, 1.0]
param b = Decay [0.45, 0.9]
param c = Mix [0.15, 0.5]
param d = Width [0.6, 1.0]
stage1: y = softclip(x, 1.05)
reverb1: size = a; decay = b; damp = 0.32; mix = c; width = d`,
  {
    a: p("Size", 0.5, 1, 0.78),
    b: p("Decay", 0.45, 0.9, 0.68),
    c: p("Mix", 0.15, 0.5, 0.32),
    d: p("Width", 0.6, 1, 0.95),
    outG: 0,
  }
);

add(
  "Bright Plate",
  "Reverb",
  "Plate-ish: HPF pre, medium room, low damp for air and sheen.",
  `param a = Size [0.3, 0.75]
param b = Decay [0.35, 0.8]
param c = Mix [0.15, 0.55]
param d = Damp [0.05, 0.4]
filter1: type = highpass; cutoff = 180; resonance = 0.28
reverb1: size = a; decay = b; damp = d; mix = c; width = 1.0`,
  {
    a: p("Size", 0.3, 0.75, 0.5),
    b: p("Decay", 0.35, 0.8, 0.55),
    c: p("Mix", 0.15, 0.55, 0.35),
    d: p("Damp", 0.05, 0.4, 0.18),
    outG: 0,
  }
);

add(
  "Dark Ambient Verb",
  "Reverb",
  "Slow dark pad space: large size, high decay, heavy damp, soft pre.",
  `param a = Size [0.55, 1.0]
param b = Decay [0.55, 0.92]
param c = Mix [0.2, 0.6]
param d = Damp [0.45, 0.9]
stage1: y = tube(x, 1.25)
reverb1: size = a; decay = b; damp = d; mix = c; width = 0.9
filter1: type = lowpass; cutoff = 9000; resonance = 0.3`,
  {
    a: p("Size", 0.55, 1, 0.82),
    b: p("Decay", 0.55, 0.92, 0.75),
    c: p("Mix", 0.2, 0.6, 0.4),
    d: p("Damp", 0.45, 0.9, 0.65),
    outG: 0,
  }
);

add(
  "Drum Chamber",
  "Reverb",
  "Punchy drum chamber: small-medium room, short decay, width for kit.",
  `param a = Size [0.2, 0.6]
param b = Decay [0.15, 0.5]
param c = Mix [0.12, 0.45]
param d = Width [0.5, 1.0]
stage1: y = softclip(x, 1.12)
reverb1: size = a; decay = b; damp = 0.5; mix = c; width = d`,
  {
    a: p("Size", 0.2, 0.6, 0.38),
    b: p("Decay", 0.15, 0.5, 0.3),
    c: p("Mix", 0.12, 0.45, 0.26),
    d: p("Width", 0.5, 1, 0.85),
    outG: 0,
  }
);

// =============================================================================
// MID/SIDE
// =============================================================================
add(
  "MS Width",
  "Mastering",
  "True mid/side stereo width: encode, scale side, decode.",
  `param a = Width [0.0, 1.6]
param b = Level [0.7, 1.2]
split1: type = midside {
  mid {
    stage1: y = x * b
  }
  side {
    stage2: y = x * a * b
  }
}`,
  {
    a: p("Width", 0, 1.6, 1.0),
    b: p("Level", 0.7, 1.2, 1.0),
    outG: 0,
  }
);

add(
  "MS Mid Focus",
  "Mastering",
  "Compress the mid, leave side open — focused center with width.",
  `param a = Threshold [-30.0, -8.0]
param b = Ratio [1.5, 6.0]
param c = Side [0.7, 1.35]
param d = Makeup [0.9, 1.4]
split1: type = midside {
  mid {
    stage1: y = x
    comp1: threshold = a; ratio = b; attack = 0.012; release = 0.16
    stage2: y = softclip(x * d, 1.08)
  }
  side {
    stage3: y = softclip(x * c, 1.05)
  }
}`,
  {
    a: p("Threshold", -30, -8, -18),
    b: p("Ratio", 1.5, 6, 2.8),
    c: p("Side", 0.7, 1.35, 1.05),
    d: p("Makeup", 0.9, 1.4, 1.1),
    outG: 0,
  }
);

add(
  "MS Side Air",
  "Mastering",
  "HPF the side channel only (channel=side filter) for airy width without muddy mids.",
  `param a = SideHPF [120, 800]
param b = SideGain [0.6, 1.4]
param c = Level [0.8, 1.15]
ms1: mode = encode
stage1: channel = mid; y = x * c
filter1: type = highpass; cutoff = a; resonance = 0.35; channel = side
stage2: channel = side; y = softclip(x * b, 1.05)
ms2: mode = decode`,
  {
    a: p("SideHPF", 120, 800, 280),
    b: p("SideGain", 0.6, 1.4, 1.05),
    c: p("Level", 0.8, 1.15, 1.0),
    outG: 0,
  }
);

// =============================================================================
// UTILITY + CLIPPER TEACHING PRESETS
// =============================================================================
add(
  "Clean Boost",
  "Utility",
  "Clean boost with optional soft ceiling (lerp dry gain vs softclip).",
  `param a = Gain [1.0, 4.5]
param b = Soft [0.0, 0.85]
stage1: y = lerp(x * a, softclip(x, a), b)
filter1: type = lowpass; cutoff = 16000; resonance = 0.2`,
  { a: p("Gain", 1, 4.5, 2.0), b: p("Soft", 0, 0.85, 0.25), outG: 0 }
);

add(
  "Mono Punch",
  "Utility",
  "Stereo-safe mild tube punch + gentle ceiling LPF.",
  `param a = Drive [1.1, 4.0]
param b = Level [0.55, 1.3]
stage1: y = tube(x, a) * b
filter1: type = lowpass; cutoff = 14000; resonance = 0.25`,
  { a: p("Drive", 1.1, 4, 2.1), b: p("Level", 0.55, 1.3, 0.95), outG: 0 }
);

add(
  "Safety Clip",
  "Utility",
  "Safety peak clip: softclip -> hard ceiling -> high AA LPF (never bare clamp).",
  `param a = Ceiling [0.5, 1.0]
param b = Drive [1.0, 1.8]
stage1: y = hardclip(softclip(x, b), a)
filter1: type = lowpass; cutoff = 16000; resonance = 0.2`,
  { a: p("Ceiling", 0.5, 1, 0.94), b: p("Drive", 1, 1.8, 1.08), outG: 0 }
);

add(
  "Soft Clip Tone",
  "Utility",
  "Canonical softclip recipe: HPF -> softclip(drive) -> tone LPF -> level.",
  `param a = Drive [0.8, 8.0]
param b = Tone [800, 9000]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 55; resonance = 0.3
stage1: y = softclip(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.4
stage2: y = y * c`,
  { a: p("Drive", 0.8, 8, 3.0), b: p("Tone", 800, 9000, 4500), c: p("Level", 0.4, 1.2, 0.85), outG: 0 }
);

add(
  "Soft-Knee Ceiling",
  "Utility",
  "Mastering-style peak clip: softclip then hardclip ceiling; gentle AA LPF.",
  `param a = Ceiling [0.6, 0.99]
param b = Drive [1.0, 2.5]
param c = Air [8000, 18000]
stage1: y = hardclip(softclip(x, b), a)
filter1: type = lowpass; cutoff = c; resonance = 0.22`,
  { a: p("Ceiling", 0.6, 0.99, 0.92), b: p("Drive", 1, 2.5, 1.15), c: p("Air", 8000, 18000, 14000), outG: 0 }
);

add(
  "Parallel Soft Clip",
  "Utility",
  "Transparent peaks: lerp(dry, softclip) - best when you need control without dirt.",
  `param a = Drive [1.0, 5.0]
param b = Blend [0.15, 0.9]
param c = Level [0.6, 1.15]
stage1: y = x * c
bus clip:
  send: in = 1
  stage2: y = softclip(x, a) * c
  filter1: type = lowpass; cutoff = 14000; resonance = 0.25
out: main = 1-b; clip = b`,
  { a: p("Drive", 1, 5, 2.2), b: p("Blend", 0.15, 0.9, 0.55), c: p("Level", 0.6, 1.15, 0.95), outG: 0 }
);

add(
  "Hard Clip Pedal",
  "Utility",
  "RAT-style hard character WITHOUT crackle: soft-pre hardclip + tone LPF after.",
  `param a = Dist [1.5, 10.0]
param b = Tone [400, 7000]
param c = Level [0.3, 1.0]
filter1: type = highpass; cutoff = 65; resonance = 0.35
stage1: y = hardclip(softclip(x, a * 0.5), 0.55)
filter2: type = lowpass; cutoff = b; resonance = 0.55
stage2: y = y * c`,
  { a: p("Dist", 1.5, 10, 5.5), b: p("Tone", 400, 7000, 2200), c: p("Level", 0.3, 1, 0.65), outG: 0 }
);

add(
  "Diode Clip Stack",
  "Utility",
  "Diode soft-knee (asinh) + optional soft ceiling + LPF - smooth analog-ish clip.",
  `param a = Drive [1.0, 8.0]
param b = Tone [1000, 10000]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 50; resonance = 0.28
stage1: y = diode(x, a)
stage2: y = softclip(y, 1.05) * c
filter2: type = lowpass; cutoff = b; resonance = 0.35`,
  { a: p("Drive", 1, 8, 3.2), b: p("Tone", 1000, 10000, 5500), c: p("Level", 0.4, 1.2, 0.88), outG: 0 }
);

// =============================================================================
// COMPLEX MULTI-KNOB (a…h) — full signal chains
// =============================================================================
add(
  "Studio Channel Strip",
  "Mastering",
  "Full strip: HPF → soft drive → real mid peak (dB) → air LPF → glue comp → soft ceiling.",
  `param a = Drive [0.8, 3.5]
param b = LowCut [30, 180]
param c = Mid [400, 3500]
param d = MidGain [-6.0, 8.0]
param e = High [4000, 14000]
param f = Thresh [-28.0, -6.0]
filter1: type = highpass; cutoff = b; resonance = 0.32
stage1: y = softclip(x, a)
eq1: type = peak; freq = c; q = 1.15; gain = d
filter2: type = lowpass; cutoff = e; resonance = 0.28
comp1: threshold = f; ratio = 3.0; attack = 0.008; release = 0.15; knee = 4; makeup = 4; hpf = 80
stage2: y = softclip(x, 1.12)`,
  {
    a: p("Drive", 0.8, 3.5, 1.35),
    b: p("LowCut", 30, 180, 55),
    c: p("Mid", 400, 3500, 1200),
    d: p("MidGain", -6, 8, 2.0),
    e: p("High", 4000, 14000, 9000),
    f: p("Thresh", -28, -6, -16),
    outG: 0,
  }
);

add(
  "Dual Amp Stack",
  "Distortion",
  "Parallel clean tube + hot tube, 800 Hz scoop in dB (not a bandpass), cab LPF, blend.",
  `param a = Clean [0.6, 2.5]
param b = Hot [2.0, 9.0]
param c = Blend [0.2, 0.9]
param d = Scoop [2.0, 9.0]
param e = Cab [2500, 8000]
param f = Level [0.35, 1.15]
bus clean:
  send: in = 1
  filter1: type = highpass; cutoff = 70; resonance = 0.35
  stage1: y = tube(x, a)
  eq1: type = peak; freq = 800; q = 0.95; gain = -d
  filter2: type = lowpass; cutoff = e; resonance = 0.4
  stage2: y = softclip(y, 1.1) * f
bus hot:
  send: in = 1
  filter3: type = highpass; cutoff = 70; resonance = 0.35
  stage3: y = tube(x, b)
  eq2: type = peak; freq = 800; q = 0.95; gain = -d
  filter4: type = lowpass; cutoff = e; resonance = 0.4
  stage4: y = softclip(y, 1.1) * f
out: clean = 1-c; hot = c`,
  {
    a: p("Clean", 0.6, 2.5, 1.2),
    b: p("Hot", 2, 9, 4.5),
    c: p("Blend", 0.2, 0.9, 0.55),
    d: p("Scoop", 2, 9, 4.5),
    e: p("Cab", 2500, 8000, 4800),
    f: p("Level", 0.35, 1.15, 0.78),
    outG: 0,
  }
);

add(
  "Rhythmic Gate Delay",
  "Delay",
  "Send delay: dry stays on main. Echo bus is 100% wet, then ducked by the input envelope. Never lerp(x,y) after mix=1 — that is a no-op and leaves only repeating wet.",
  `param a = Time [80, 480]
param b = Feedback [0.12, 0.72]
param c = Mix [0.15, 0.7]
param d = Damp [800, 9000]
param e = Duck [0.0, 0.85]
param f = Drive [0.8, 2.8]
env1: type = peak; attack = 0.006; release = 0.16
stage1: y = x
bus echo:
  send: in = 1
  delay1: time = a; feedback = b; mix = 1; damp = d
  stage2: y = softclip(x * (1.0 + f * 0.22), 1.08)
  stage3: y = x * max(0.08, 1.0 - env1 * e)
out: main = 1; echo = c`,
  {
    a: p("Time", 80, 480, 220),
    b: p("Feedback", 0.12, 0.72, 0.38),
    c: p("Mix", 0.15, 0.7, 0.36),
    d: p("Damp", 800, 9000, 4200),
    e: p("Duck", 0, 0.85, 0.42),
    f: p("Drive", 0.8, 2.8, 1.35),
    outG: 0,
  }
);

add(
  "Cinematic Space",
  "Reverb",
  "Dry stays. Wet is true predelay into the hall, then MS width. No slap comb.",
  `param a = Predelay [20, 180]
param b = Size [0.25, 0.95]
param c = Decay [0.35, 0.92]
param d = Damp [0.15, 0.85]
param e = Mix [0.2, 0.85]
param f = Width [0.4, 1.6]
stage1: y = x
bus space:
  send: in = 1
  delay1: time = a; feedback = 0; mix = 1; damp = 8000
  reverb1: size = b; decay = c; damp = d; mix = 1; width = 0.95
  filter1: type = lowpass; cutoff = 8000; resonance = 0.25
  ms1: mode = encode
  stage2: channel = mid; y = x
  stage3: channel = side; y = x * f
  ms2: mode = decode
  stage4: y = softclip(x, 1.08)
out: main = 1-e; space = e`,
  {
    a: p("Predelay", 20, 180, 65),
    b: p("Size", 0.25, 0.95, 0.72),
    c: p("Decay", 0.35, 0.92, 0.7),
    d: p("Damp", 0.15, 0.85, 0.45),
    e: p("Mix", 0.2, 0.85, 0.48),
    f: p("Width", 0.4, 1.6, 1.0),
    outG: 0,
  }
);

add(
  "Phaser Lab",
  "Modulation",
  "LFO swirl: HP stays below LP, no 1-sample feedback. Rate, depth, center, drive, wet.",
  `param a = Rate [0.05, 6.0]
param b = Depth [200, 2500]
param c = Center [400, 4000]
param d = Drive [0.8, 1.6]
param e = Mix [0.25, 0.95]
param f = Resonance [0.35, 1.1]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = x
bus swirl:
  send: in = 1
  filter1: type = highpass; cutoff = c + (0.5 + 0.5 * osc1) * (b * 0.22); resonance = f
  filter2: type = lowpass; cutoff = c + b * 0.55 + (0.5 + 0.5 * osc1) * (b * 0.35); resonance = 0.5
  stage2: y = softclip(x, d)
  filter3: type = lowpass; cutoff = 12000; resonance = 0.28
out: main = 1-e; swirl = e`,
  {
    a: p("Rate", 0.05, 6, 0.35),
    b: p("Depth", 200, 2500, 900),
    c: p("Center", 400, 4000, 1200),
    d: p("Drive", 0.8, 1.6, 1.12),
    e: p("Mix", 0.25, 0.95, 0.7),
    f: p("Resonance", 0.35, 1.1, 0.65),
    outG: 0,
  }
);

add(
  "Vocal Chain Pro",
  "Vocals",
  "Broadcast vocal: de-mud HPF, tube, presence peak in dB, glue, air shelf, de-ess cut.",
  `param a = LowCut [60, 220]
param b = Drive [0.9, 3.5]
param c = Presence [1800, 5500]
param d = Amount [1.0, 8.0]
param e = Thresh [-24.0, -8.0]
param f = Air [4000, 12000]
filter1: type = highpass; cutoff = a; resonance = 0.35
stage1: y = tube(x, b)
eq1: type = peak; freq = c; q = 1.1; gain = d
comp1: threshold = e; ratio = 3.5; attack = 0.006; release = 0.12
eq2: type = highshelf; freq = f; q = 0.7; gain = 2.4
filter2: type = lowpass; cutoff = 10500; resonance = 0.28
stage2: y = softclip(x, 1.06)`,
  {
    a: p("LowCut", 60, 220, 110),
    b: p("Drive", 0.9, 3.5, 1.6),
    c: p("Presence", 1800, 5500, 3200),
    d: p("Amount", 1, 8, 3.5),
    e: p("Thresh", -24, -8, -14),
    f: p("Air", 4000, 12000, 8000),
    outG: 0,
  }
);

add(
  "Bass Architect",
  "Bass",
  "Sub-safe: HPF, parallel grit, mid peak (keeps the fundamental), glue, cab LPF.",
  `param a = HPF [25, 90]
param b = Drive [1.2, 6.0]
param c = Blend [0.2, 0.8]
param d = Mid [200, 1200]
param e = Thresh [-22.0, -8.0]
param f = Level [0.5, 1.3]
filter1: type = highpass; cutoff = a; resonance = 0.25
stage1: y = lerp(x, tube(x, b), c)
eq1: type = peak; freq = d; q = 1.05; gain = 3.8
comp1: threshold = e; ratio = 4.0; attack = 0.015; release = 0.22
filter2: type = lowpass; cutoff = 6500; resonance = 0.3
stage2: y = softclip(x * f, 1.08)`,
  {
    a: p("HPF", 25, 90, 40),
    b: p("Drive", 1.2, 6, 2.8),
    c: p("Blend", 0.2, 0.8, 0.5),
    d: p("Mid", 200, 1200, 550),
    e: p("Thresh", -22, -8, -14),
    f: p("Level", 0.5, 1.3, 0.95),
    outG: 0,
  }
);

add(
  "Glitch Laboratory",
  "Creative",
  "Musical digital smash: HPF, crush, fold, one short damped delay on a note grid, cab LPF. No ping-pong, no LFO on the filter — light on the CPU.",
  `param a = Bits [5.0, 12.0]
param b = Fold [0.16, 0.52]
param c = Time [1/16, 1/4]
param d = Repeat [0.04, 0.28]
param e = Tone [2600, 7200]
param f = Level [0.85, 1.4]
filter1: type = highpass; cutoff = 95; resonance = 0.22
stage1: y = bitcrush(softclip(x, 1.55), a)
stage2: y = fold(y, -b, b)
delay1: time = c; feedback = d; mix = 0.12; damp = 2800
filter2: type = lowpass; cutoff = e; resonance = 0.26
stage3: y = diode(y, 1.1) * f`,
  {
    a: p("Bits", 5, 12, 8.2),
    b: p("Fold", 0.16, 0.52, 0.32),
    c: p("Time", 0, 1, 0.65),
    d: p("Repeat", 0.04, 0.28, 0.12),
    e: p("Tone", 2600, 7200, 4800),
    f: p("Level", 0.85, 1.4, 1.12),
    tags: ["glitch", "digital", "crush", "cyberpunk"],
    outG: 0,
  }
);

add(
  "Neon Clip",
  "Distortion",
  "Cyberpunk hard ceiling: HPF, mid punch, softclip into a brick, short metal comb, dark cab.",
  `param a = Drive [2.2, 10.0]
param b = Ceiling [0.38, 0.82]
param c = Mid [700, 1800]
param d = Metal [6, 22]
param e = Tone [1800, 6200]
param f = Level [0.75, 1.4]
filter1: type = highpass; cutoff = 110; resonance = 0.32
eq1: type = peak; freq = c; q = 1.1; gain = 4.2
stage1: y = hardclip(softclip(x, a), b)
delay1: time = d; feedback = 0.28; mix = 0.12; damp = 5200
filter2: type = lowpass; cutoff = e; resonance = 0.34
stage2: y = diode(y, 1.18) * f`,
  {
    a: p("Drive", 2.2, 10, 5.2),
    b: p("Ceiling", 0.38, 0.82, 0.62),
    c: p("Mid", 700, 1800, 1250),
    d: p("Metal", 6, 22, 11),
    e: p("Tone", 1800, 6200, 3800),
    f: p("Level", 0.75, 1.4, 1.08),
    tags: ["cyberpunk", "clip", "digital", "distortion"],
    outG: 0,
  }
);

add(
  "Chrome Fold",
  "Distortion",
  "West-coast fold into diode chrome. Tight HPF, recovery LPF — harsh but band-limited.",
  `param a = Drive [2.0, 11.0]
param b = Fold [0.2, 0.62]
param c = Chrome [1.0, 2.2]
param d = Tone [2200, 7800]
param e = Level [0.55, 1.25]
filter1: type = highpass; cutoff = 90; resonance = 0.26
stage1: y = fold(x * a, -b, b)
stage2: y = diode(y, c)
filter2: type = lowpass; cutoff = d; resonance = 0.3
stage3: y = softclip(y, 1.08) * e`,
  {
    a: p("Drive", 2, 11, 5.4),
    b: p("Fold", 0.2, 0.62, 0.36),
    c: p("Chrome", 1, 2.2, 1.45),
    d: p("Tone", 2200, 7800, 4600),
    e: p("Level", 0.55, 1.25, 0.92),
    tags: ["cyberpunk", "fold", "digital", "distortion"],
    outG: 0,
  }
);

add(
  "Data Mosher",
  "Distortion",
  "Bitcrush alley: preamp, bits, anti-alias LPF. Digital dirt without a delay tax.",
  `param a = Bits [3.5, 11.0]
param b = Drive [1.2, 5.5]
param c = Tone [900, 6500]
param d = Level [0.55, 1.3]
filter1: type = highpass; cutoff = 80; resonance = 0.24
stage1: y = bitcrush(softclip(x, b), a)
filter2: type = lowpass; cutoff = c; resonance = 0.4
stage2: y = diode(y, 1.12) * d`,
  {
    a: p("Bits", 3.5, 11, 6.8),
    b: p("Drive", 1.2, 5.5, 2.4),
    c: p("Tone", 900, 6500, 3200),
    d: p("Level", 0.55, 1.3, 0.95),
    tags: ["cyberpunk", "bitcrush", "lo-fi", "digital"],
    outG: 0,
  }
);

add(
  "Kick Rumble",
  "Club",
  "Split kick: main is click+mids (HPF ~95, dip at 320 Hz), scream is the bright hit, body is a tight held sine+sub. Tune is the resonator. Insert on the kick, Mix 100.",
  `param a = Sub [0.55, 1.35]
param b = Floor [48, 88]
param c = Tune [11, 20]
param d = Drive [5.0, 16.0]
param e = Decay [0.62, 0.9]
param f = Level [1.05, 1.5]
env1: type = peak; attack = 0.001; release = 0.09
filter1: type = highpass; cutoff = 95; resonance = 0.22
eq1: type = peak; freq = 320; q = 1.15; gain = -5.5
eq2: type = peak; freq = 1150; q = 1.3; gain = 5.2
eq3: type = peak; freq = 3800; q = 1.7; gain = 6.4
eq4: type = highshelf; freq = 6800; q = 0.7; gain = 3.2
stage1: y = hardclip(softclip(x * f * (0.82 + 0.28 * env1), 1.12), 0.72)
bus scream:
  send: in = 1
  filter2: type = highpass; cutoff = 200; resonance = 0.42
  eq5: type = peak; freq = 3400; q = 1.35; gain = 7.4
  eq6: type = peak; freq = 7200; q = 1.25; gain = 3.6
  stage2: y = tube(x * d * 0.22 * (0.15 + 0.85 * env1), 1.35)
  filter3: type = lowpass; cutoff = 9800; resonance = 0.24
  stage3: y = diode(y, 1.45) * (0.08 + 0.92 * env1)
bus body:
  send: in = 1
  filter4: type = highpass; cutoff = 28; resonance = 0.16
  filter5: type = lowpass; cutoff = b; resonance = 0.42
  delay1: time = c; feedback = e; mix = 0.68; damp = 280
  octaver1: sub = 0.95; up = 0; mix = 0.48; tone = 95; thresh = 0.03
  stage4: y = tube(x, d * 0.22)
  stage5: y = hardclip(softclip(y, 1.1), 0.7)
  eq7: type = peak; freq = 56; q = 0.95; gain = 5.2
  filter6: type = lowpass; cutoff = 132; resonance = 0.24
out: main = 0.7; scream = 0.82; body = a`,
  {
    a: p("Sub", 0.55, 1.35, 1.02),
    b: p("Floor", 48, 88, 62),
    c: p("Tune", 11, 20, 15.5),
    d: p("Drive", 5, 16, 11.8),
    e: p("Decay", 0.62, 0.9, 0.76),
    f: p("Level", 1.05, 1.5, 1.26),
    tags: ["techno", "kick", "rumble", "octaver", "hardcore", "club"],
    outG: 0,
  }
);

add(
  "Warehouse Rumble",
  "Club",
  "Same split, lower floor: click stays above 80 Hz, 320 Hz scooped, body stays under 130 Hz. Insert on the kick.",
  `param a = Sub [0.6, 1.4]
param b = Floor [42, 80]
param c = Tune [14, 24]
param d = Drive [5.0, 16.0]
param e = Decay [0.68, 0.92]
param f = Level [1.05, 1.5]
env1: type = peak; attack = 0.001; release = 0.14
filter1: type = highpass; cutoff = 82; resonance = 0.2
eq1: type = peak; freq = 310; q = 1.1; gain = -5.0
eq2: type = peak; freq = 1050; q = 1.2; gain = 4.6
eq3: type = peak; freq = 2800; q = 1.45; gain = 5.0
eq4: type = highshelf; freq = 6200; q = 0.7; gain = 2.4
stage1: y = hardclip(softclip(x * f * (0.84 + 0.24 * env1), 1.12), 0.72)
bus scream:
  send: in = 1
  filter2: type = highpass; cutoff = 170; resonance = 0.38
  eq5: type = peak; freq = 2400; q = 1.25; gain = 6.2
  eq6: type = peak; freq = 6000; q = 1.2; gain = 2.8
  stage2: y = tube(x * d * 0.2 * (0.12 + 0.88 * env1), 1.3)
  filter3: type = lowpass; cutoff = 8600; resonance = 0.22
  stage3: y = diode(y, 1.4) * (0.08 + 0.9 * env1)
bus body:
  send: in = 1
  filter4: type = highpass; cutoff = 24; resonance = 0.16
  filter5: type = lowpass; cutoff = b; resonance = 0.4
  delay1: time = c; feedback = e; mix = 0.7; damp = 240
  octaver1: sub = 1.05; up = 0; mix = 0.5; tone = 88; thresh = 0.028
  stage4: y = tube(x, d * 0.24)
  stage5: y = hardclip(softclip(y, 1.1), 0.7)
  eq7: type = peak; freq = 50; q = 0.9; gain = 6.2
  filter6: type = lowpass; cutoff = 120; resonance = 0.22
out: main = 0.68; scream = 0.72; body = a`,
  {
    a: p("Sub", 0.6, 1.4, 1.1),
    b: p("Floor", 42, 80, 56),
    c: p("Tune", 14, 24, 18),
    d: p("Drive", 5, 16, 11.4),
    e: p("Decay", 0.68, 0.92, 0.82),
    f: p("Level", 1.05, 1.5, 1.24),
    tags: ["techno", "kick", "rumble", "octaver", "hardcore", "club"],
    outG: 0,
  }
);

add(
  "Hardcore Clip",
  "Club",
  "160 BPM brick: mid bark + air on the hit, 320 Hz scooped so it stays crisp, tight body underneath. Insert on the kick.",
  `param a = Drive [6.5, 18.0]
param b = Ceiling [0.16, 0.44]
param c = HPF [90, 220]
param d = Body [0.45, 1.2]
param e = Tune [12, 20]
param f = Level [1.05, 1.5]
env1: type = peak; attack = 0.001; release = 0.08
filter1: type = highpass; cutoff = c; resonance = 0.48
eq1: type = peak; freq = 320; q = 1.2; gain = -5.8
eq2: type = peak; freq = 1050; q = 1.35; gain = 8.2
eq3: type = peak; freq = 3600; q = 1.55; gain = 5.8
eq4: type = highshelf; freq = 7500; q = 0.7; gain = 3.2
stage1: y = hardclip(softclip(x * f * (0.78 + 0.28 * env1), 1.15), b)
filter2: type = lowpass; cutoff = 9800; resonance = 0.26
stage2: y = diode(y, 1.45)
bus body:
  send: in = 1
  filter3: type = highpass; cutoff = 26; resonance = 0.16
  filter4: type = lowpass; cutoff = 62; resonance = 0.4
  delay1: time = e; feedback = 0.7; mix = 0.64; damp = 260
  octaver1: sub = 0.88; up = 0; mix = 0.46; tone = 100; thresh = 0.03
  stage3: y = tube(x, a * 0.2)
  stage4: y = hardclip(softclip(y, 1.1), 0.7)
  filter5: type = lowpass; cutoff = 128; resonance = 0.22
out: main = 0.92; body = d`,
  {
    a: p("Drive", 6.5, 18, 13.6),
    b: p("Ceiling", 0.16, 0.44, 0.24),
    c: p("HPF", 90, 220, 145),
    d: p("Body", 0.45, 1.2, 0.88),
    e: p("Tune", 12, 20, 15),
    f: p("Level", 1.05, 1.5, 1.26),
    tags: ["hardcore", "techno", "clip", "octaver", "club"],
    outG: 0,
  }
);

add(
  "Gabber Drive",
  "Club",
  "New-style gabber: scooped 320 Hz, envelope-crushed highs, tight clipped sub that stays under. Insert on the kick, Mix 100.",
  `param a = Drive [6.0, 18.0]
param b = Scoop [90, 220]
param c = Scream [1600, 3400]
param d = Sub [0.65, 1.3]
param e = Dyn [0.45, 1.15]
param f = Level [1.0, 1.5]
env1: type = peak; attack = 0.001; release = 0.1
filter1: type = highpass; cutoff = 88; resonance = 0.24
eq1: type = peak; freq = 320; q = 1.15; gain = -6.0
eq2: type = peak; freq = 78; q = 1.05; gain = 2.4
eq3: type = peak; freq = 3000; q = 1.4; gain = 4.6
eq4: type = highshelf; freq = 7000; q = 0.7; gain = 2.6
stage1: y = hardclip(softclip(x * f * (0.76 + 0.24 * env1), 1.14), 0.7)
bus scream:
  send: in = 1
  filter2: type = highpass; cutoff = b; resonance = 0.42
  eq5: type = peak; freq = c; q = 1.35; gain = 8.6
  eq6: type = peak; freq = 6800; q = 1.35; gain = 4.0
  stage2: y = tube(x * a * 0.18 * (0.12 + 0.88 * e * env1), 1.4)
  filter3: type = lowpass; cutoff = 10200; resonance = 0.24
  stage3: y = diode(y, 1.5) * (0.08 + 0.9 * env1)
bus sub:
  send: in = 1
  filter4: type = highpass; cutoff = 24; resonance = 0.16
  filter5: type = lowpass; cutoff = 58; resonance = 0.4
  delay1: time = 16; feedback = 0.7; mix = 0.66; damp = 240
  octaver1: sub = 1.0; up = 0; mix = 0.48; tone = 90; thresh = 0.028
  stage4: y = tube(x, 2.6)
  stage5: y = hardclip(softclip(y, 1.1), 0.7)
  filter6: type = lowpass; cutoff = 124; resonance = 0.22
out: main = 0.58; scream = 0.95; sub = d`,
  {
    a: p("Drive", 6, 18, 13.4),
    b: p("Scoop", 90, 220, 145),
    c: p("Scream", 1600, 3400, 2300),
    d: p("Sub", 0.65, 1.3, 1.05),
    e: p("Dyn", 0.45, 1.15, 0.98),
    f: p("Level", 1.0, 1.5, 1.22),
    tags: ["gabber", "hardcore", "techno", "clip", "sub", "octaver", "club"],
    outG: 0,
  }
);

add(
  "Acid Hash",
  "Club",
  "Nasty 303 hash: slammed diode into a brick, then a screaming resonant LPF. Cutoff and Reso are the performance.",
  `param a = Drive [4.5, 15.0]
param b = Cutoff [180, 3600]
param c = Reso [1.4, 3.25]
param d = Level [0.95, 1.5]
filter1: type = highpass; cutoff = 42; resonance = 0.32
stage1: y = diode(x, a)
stage2: y = hardclip(softclip(y, 2.2), 0.48)
filter2: type = lowpass; cutoff = b; resonance = c
stage3: y = hardclip(softclip(y, 1.7) * d, 0.92)
filter3: type = lowpass; cutoff = 7200; resonance = 0.28`,
  {
    a: p("Drive", 4.5, 15, 10.4),
    b: p("Cutoff", 180, 3600, 880),
    c: p("Reso", 1.4, 3.25, 2.78),
    d: p("Level", 0.95, 1.5, 1.24),
    tags: ["acid", "techno", "filter", "club"],
    outG: 0,
  }
);

add(
  "Tekno Comb",
  "Club",
  "Metallic tekno that rings and slams: brick clip into a loud short comb. For stabs, hoovers, off-grid hats.",
  `param a = Drive [5.0, 15.0]
param b = Metal [4, 26]
param c = Feedback [0.38, 0.82]
param d = Tone [1200, 5200]
param e = Level [0.95, 1.5]
filter1: type = highpass; cutoff = 85; resonance = 0.45
stage1: y = hardclip(softclip(x, a), 0.38)
delay1: time = b; feedback = c; mix = 0.7; damp = 3400
filter2: type = lowpass; cutoff = d; resonance = 0.55
stage2: y = hardclip(diode(y, 1.85) * e, 0.9)
filter3: type = lowpass; cutoff = 6800; resonance = 0.28`,
  {
    a: p("Drive", 5, 15, 11.2),
    b: p("Metal", 4, 26, 10),
    c: p("Feedback", 0.38, 0.82, 0.64),
    d: p("Tone", 1200, 5200, 2800),
    e: p("Level", 0.95, 1.5, 1.22),
    tags: ["techno", "metal", "comb", "club"],
    outG: 0,
  }
);

add(
  "Industrial Gate",
  "Club",
  "Industrial smash: brick clip when open, silence when shut. Threshold is the groove.",
  `param a = Drive [5.5, 16.0]
param b = Thresh [-40.0, -10.0]
param c = Range [-80.0, -18.0]
param d = Tone [1400, 6200]
param e = Level [1.0, 1.5]
gate1: threshold = b; hyst = 4; hold = 0.016; range = c
filter1: type = highpass; cutoff = 78; resonance = 0.42
stage1: y = hardclip(softclip(x, a), 0.28)
filter2: type = lowpass; cutoff = d; resonance = 0.42
stage2: y = hardclip(diode(y, 1.85) * e, 0.9)
filter3: type = lowpass; cutoff = 7000; resonance = 0.26`,
  {
    a: p("Drive", 5.5, 16, 12.0),
    b: p("Thresh", -40, -10, -24),
    c: p("Range", -80, -18, -58),
    d: p("Tone", 1400, 6200, 3600),
    e: p("Level", 1.0, 1.5, 1.26),
    tags: ["industrial", "gate", "hardcore", "club"],
    outG: 0,
  }
);

add(
  "Hoover Dirt",
  "Club",
  "Hoover / off-key stab that rasps: hard fold, chorus smear, then a diode slap. For rave leads, not kicks.",
  `param a = Drive [3.2, 12.0]
param b = Fold [0.28, 0.68]
param c = Width [6, 24]
param d = Tone [2000, 7800]
param e = Level [0.85, 1.4]
filter1: type = highpass; cutoff = 78; resonance = 0.3
stage1: y = fold(softclip(x, a), -b, b)
delay1: time = c; feedback = 0.28; mix = 0.22; damp = 5200
filter2: type = lowpass; cutoff = d; resonance = 0.38
stage2: y = hardclip(diode(y, 1.55) * e, 0.88)
filter3: type = lowpass; cutoff = 7600; resonance = 0.26`,
  {
    a: p("Drive", 3.2, 12, 7.2),
    b: p("Fold", 0.28, 0.68, 0.48),
    c: p("Width", 6, 24, 14),
    d: p("Tone", 2000, 7800, 4800),
    e: p("Level", 0.85, 1.4, 1.16),
    tags: ["hoover", "rave", "techno", "fold", "club"],
    outG: 0,
  }
);

add(
  "909 Newstyle",
  "Club",
  "909 kick to new-style hardcore: click HPF, 320 Hz scooped, env-bricked mids, side scream, tight tube sub. Insert on the kick, Mix 100.",
  `param a = Drive [6.0, 18.0]
param b = Click [90, 220]
param c = Scoop [-9.0, -2.0]
param d = Width [0.25, 1.2]
param e = Sub [0.4, 1.2]
param f = Level [0.95, 1.45]
env1: type = peak; attack = 0.0008; release = 0.07
filter1: type = highpass; cutoff = b; resonance = 0.46
eq1: type = peak; freq = 320; q = 1.22; gain = c
eq2: type = peak; freq = 1180; q = 1.4; gain = 7.6
eq3: type = highshelf; freq = 7800; q = 0.7; gain = 3.4
stage1: y = hardclip(softclip(x * f * (0.68 + 0.38 * env1), 1.2), 0.22)
filter2: type = lowpass; cutoff = 10800; resonance = 0.22
ms1: mode = encode
stage2: channel = side; y = diode(x * a * 0.15 * d, 1.48)
stage3: channel = mid; y = x
ms2: mode = decode
bus sub:
  send: in = 1
  filter3: type = highpass; cutoff = 24; resonance = 0.15
  filter4: type = lowpass; cutoff = 66; resonance = 0.38
  stage4: y = tube(x, a * 0.18)
  stage5: y = hardclip(softclip(y, 1.08), 0.68)
  filter5: type = lowpass; cutoff = 118; resonance = 0.2
out: main = 0.94; sub = e`,
  {
    a: p("Drive", 6, 18, 13.8),
    b: p("Click", 90, 220, 148),
    c: p("Scoop", -9, -2, -5.8),
    d: p("Width", 0.25, 1.2, 0.82),
    e: p("Sub", 0.4, 1.2, 0.88),
    f: p("Level", 0.95, 1.45, 1.22),
    tags: ["909", "hardcore", "techno", "kick", "club", "mid-side"],
    outG: 0,
  }
);

add(
  "Crisp Brick",
  "Club",
  "Extreme crisp distortion: transient-weighted diode into a low ceiling, then a hard recovery LPF. Kicks, hats, stabs.",
  `param a = Drive [5.5, 18.0]
param b = Ceiling [0.12, 0.4]
param c = Snap [0.35, 1.2]
param d = Air [2500, 9000]
param e = Body [80, 220]
param f = Level [0.95, 1.45]
env1: type = peak; attack = 0.0006; release = 0.055
filter1: type = highpass; cutoff = e; resonance = 0.4
eq1: type = peak; freq = 300; q = 1.15; gain = -5.2
eq2: type = peak; freq = 3400; q = 1.45; gain = 6.8
eq3: type = highshelf; freq = d; q = 0.7; gain = 4.2
stage1: y = diode(x * f * (0.55 + c * env1), a * 0.12)
stage2: y = hardclip(softclip(y, 1.22), b)
filter2: type = lowpass; cutoff = 11200; resonance = 0.24
stage3: y = hardclip(y, 0.92)`,
  {
    a: p("Drive", 5.5, 18, 13.2),
    b: p("Ceiling", 0.12, 0.4, 0.2),
    c: p("Snap", 0.35, 1.2, 0.92),
    d: p("Air", 2500, 9000, 7200),
    e: p("Body", 80, 220, 128),
    f: p("Level", 0.95, 1.45, 1.2),
    tags: ["hardcore", "techno", "clip", "club", "crisp"],
    outG: 0,
  }
);

add(
  "Techno Snap",
  "Club",
  "Transient designer for techno: fast peak punch vs slower RMS body. Kicks, hats, claps, stabs.",
  `param a = Attack [0.4, 2.4]
param b = Sustain [0.25, 1.4]
param c = Rel [0.04, 0.28]
param d = Drive [1.2, 8.0]
param e = Tone [1800, 8000]
param f = Level [0.85, 1.35]
env1: type = peak; attack = 0.0005; release = 0.04
env2: type = rms; attack = 0.01; release = c
filter1: type = highpass; cutoff = 42; resonance = 0.22
stage1: y = x * (1 + env1 * a) / (1 + env2 * b * 0.55)
stage2: y = softclip(y * f, d * 0.28)
eq1: type = peak; freq = 320; q = 0.95; gain = -2.4
eq2: type = highshelf; freq = e; q = 0.7; gain = 2.8
filter2: type = lowpass; cutoff = 12000; resonance = 0.2
stage3: y = hardclip(softclip(y, 1.08), 0.88)`,
  {
    a: p("Attack", 0.4, 2.4, 1.35),
    b: p("Sustain", 0.25, 1.4, 0.72),
    c: p("Rel", 0.04, 0.28, 0.12),
    d: p("Drive", 1.2, 8, 3.4),
    e: p("Tone", 1800, 8000, 4600),
    f: p("Level", 0.85, 1.35, 1.08),
    tags: ["techno", "transient", "club", "envelope"],
    outG: 0,
  }
);

add(
  "Side Scream",
  "Club",
  "Mid stays punchy. Side is env-crushed diode so width bites without wrecking the kick center.",
  `param a = Drive [4.0, 16.0]
param b = Side [0.35, 1.4]
param c = Mid [0.7, 1.2]
param d = Air [3000, 9000]
param e = HPF [80, 280]
param f = Level [0.9, 1.4]
env1: type = peak; attack = 0.001; release = 0.09
filter1: type = highpass; cutoff = e; resonance = 0.32
ms1: mode = encode
stage1: channel = mid; y = tube(x * c, 1.15)
stage2: channel = side; y = diode(x * a * 0.2 * b * (0.2 + 0.8 * env1), 1.55)
eq1: type = highshelf; freq = d; q = 0.7; gain = 3.6; channel = side
ms2: mode = decode
stage3: y = hardclip(softclip(x * f, 1.12), 0.78)
filter2: type = lowpass; cutoff = 11500; resonance = 0.22`,
  {
    a: p("Drive", 4, 16, 11.6),
    b: p("Side", 0.35, 1.4, 0.95),
    c: p("Mid", 0.7, 1.2, 1.0),
    d: p("Air", 3000, 9000, 6400),
    e: p("HPF", 80, 280, 140),
    f: p("Level", 0.9, 1.4, 1.16),
    tags: ["techno", "hardcore", "club", "mid-side", "width"],
    outG: 0,
  }
);

// =============================================================================
// MIX DESK — MS + BUS (honest utilities, not a mastering suite)
// These cover jobs a desk actually does: side-only time, mono-below,
// parallel smash, vocal send, mid grit / side air. They do not replace a
// linear-phase MS EQ, convolution verb, or a dedicated imager.
// =============================================================================
add(
  "Side Delay",
  "Delay",
  "MS encode, delay only on the side, decode. Center stays dry — width without smearing a vocal or kick.",
  `param a = Time [90, 420]
param b = Feedback [0.12, 0.62]
param c = Mix [0.15, 0.7]
param d = Damp [900, 7000]
param e = Mid [0.7, 1.15]
param f = Level [0.75, 1.15]
ms1: mode = encode
stage1: channel = mid; y = x * e
delay1: time = a; feedback = b; mix = c; damp = d; channel = side
ms2: mode = decode
stage2: y = softclip(x * f, 1.06)`,
  {
    a: p("Time", 90, 420, 220),
    b: p("Feedback", 0.12, 0.62, 0.32),
    c: p("Mix", 0.15, 0.7, 0.38),
    d: p("Damp", 900, 7000, 3800),
    e: p("Mid", 0.7, 1.15, 1.0),
    f: p("Level", 0.75, 1.15, 1.0),
    outG: 0,
  }
);

add(
  "Side Hall",
  "Reverb",
  "Dry stays on main. A bus mutes mid, keeps side, then halls it. Center image stays put.",
  `param a = Size [0.28, 0.82]
param b = Decay [0.28, 0.8]
param c = Send [0.12, 0.62]
param d = Damp [0.25, 0.8]
param e = Tone [3500, 10000]
param f = Level [0.75, 1.15]
stage1: y = x * f
bus sides:
  send: in = 1
  ms1: mode = encode
  stage2: channel = mid; y = x * 0.0
  stage3: channel = side; y = x
  ms2: mode = decode
  reverb1: size = a; decay = b; damp = d; mix = 1; width = 1.0
  filter1: type = lowpass; cutoff = e; resonance = 0.25
out: main = 1-c; sides = c`,
  {
    a: p("Size", 0.28, 0.82, 0.52),
    b: p("Decay", 0.28, 0.8, 0.5),
    c: p("Send", 0.12, 0.62, 0.32),
    d: p("Damp", 0.25, 0.8, 0.48),
    e: p("Tone", 3500, 10000, 7200),
    f: p("Level", 0.75, 1.15, 1.0),
    outG: 0,
  }
);

add(
  "Vocal Send",
  "Vocals",
  "Insert or send: dry vocal on main, slap delay bus + small room bus. Two real time blocks, one Send each.",
  `param a = Slap [70, 170]
param b = Size [0.2, 0.62]
param c = Decay [0.16, 0.58]
param d = SlapMix [0.08, 0.42]
param e = RoomMix [0.1, 0.52]
param f = Tone [3000, 10000]
stage1: y = x
bus slap:
  send: in = 1
  delay1: time = a; feedback = 0.14; mix = 1; damp = 5500
  filter1: type = lowpass; cutoff = f; resonance = 0.25
bus room:
  send: in = 1
  reverb1: size = b; decay = c; damp = 0.5; mix = 1; width = 0.82
  filter2: type = lowpass; cutoff = f; resonance = 0.25
out: main = 1; slap = d; room = e`,
  {
    a: p("Slap", 70, 170, 112),
    b: p("Size", 0.2, 0.62, 0.38),
    c: p("Decay", 0.16, 0.58, 0.34),
    d: p("SlapMix", 0.08, 0.42, 0.18),
    e: p("RoomMix", 0.1, 0.52, 0.24),
    f: p("Tone", 3000, 10000, 6800),
    mix: 1,
    outG: 0,
  }
);

add(
  "NY Drum Bus",
  "Drums",
  "Classic parallel smash: dry kit on main, bus is fast comp + tube + tone LPF. Blend is the mix.",
  `param a = Thresh [-28.0, -8.0]
param b = Ratio [2.0, 8.0]
param c = Drive [1.1, 4.0]
param d = Blend [0.15, 0.75]
param e = Tone [4000, 12000]
param f = Level [0.55, 1.2]
stage1: y = x * f
bus smash:
  send: in = 1
  comp1: threshold = a; ratio = b; attack = 0.004; release = 0.09
  stage2: y = tube(x, c)
  filter1: type = lowpass; cutoff = e; resonance = 0.28
  stage3: y = softclip(y * f, 1.08)
out: main = 1-d; smash = d`,
  {
    a: p("Thresh", -28, -8, -16),
    b: p("Ratio", 2, 8, 4.2),
    c: p("Drive", 1.1, 4, 2.1),
    d: p("Blend", 0.15, 0.75, 0.42),
    e: p("Tone", 4000, 12000, 7800),
    f: p("Level", 0.55, 1.2, 0.95),
    outG: 0,
  }
);

add(
  "MS Mix Desk",
  "Mastering",
  "MS encode: mid gets optional tube glue, side gets HPF + gain, decode, air LPF. A mix-bus habit, not Ozone.",
  `param a = MidDrive [0.8, 2.4]
param b = SideHPF [90, 420]
param c = Side [0.7, 1.35]
param d = Glue [0.0, 0.7]
param e = Air [6000, 14000]
param f = Level [0.75, 1.2]
ms1: mode = encode
stage1: channel = mid; y = lerp(x, tube(x, a), d)
filter1: type = highpass; cutoff = b; resonance = 0.3; channel = side
stage2: channel = side; y = softclip(x * c, 1.05)
ms2: mode = decode
filter2: type = lowpass; cutoff = e; resonance = 0.22
stage3: y = softclip(x * f, 1.06)`,
  {
    a: p("MidDrive", 0.8, 2.4, 1.25),
    b: p("SideHPF", 90, 420, 180),
    c: p("Side", 0.7, 1.35, 1.05),
    d: p("Glue", 0, 0.7, 0.28),
    e: p("Air", 6000, 14000, 11000),
    f: p("Level", 0.75, 1.2, 1.0),
    outG: 0,
  }
);

add(
  "Mono Below",
  "Mastering",
  "MS encode, high-pass the side at the crossover, decode. Lows collapse to mono; tops keep width.",
  `param a = Crossover [70, 280]
param b = Side [0.5, 1.3]
param c = Mid [0.85, 1.15]
param d = Level [0.8, 1.15]
ms1: mode = encode
stage1: channel = mid; y = x * c
filter1: type = highpass; cutoff = a; resonance = 0.28; channel = side
stage2: channel = side; y = softclip(x * b, 1.05)
ms2: mode = decode
stage3: y = x * d`,
  {
    a: p("Crossover", 70, 280, 140),
    b: p("Side", 0.5, 1.3, 1.0),
    c: p("Mid", 0.85, 1.15, 1.0),
    d: p("Level", 0.8, 1.15, 1.0),
    outG: 0,
  }
);

add(
  "Slap Double",
  "Vocals",
  "Dry on main, 18-42 ms slap on a bus. Thickness without a chorus LFO.",
  `param a = Time [16, 48]
param b = Feedback [0.0, 0.28]
param c = Blend [0.12, 0.55]
param d = Tone [2500, 9000]
param e = Drive [0.8, 2.2]
param f = Level [0.75, 1.15]
stage1: y = x * f
bus slap:
  send: in = 1
  delay1: time = a; feedback = b; mix = 1; damp = 6500
  stage2: y = softclip(x, e)
  filter1: type = lowpass; cutoff = d; resonance = 0.25
out: main = 1-c; slap = c`,
  {
    a: p("Time", 16, 48, 26),
    b: p("Feedback", 0, 0.28, 0.08),
    c: p("Blend", 0.12, 0.55, 0.28),
    d: p("Tone", 2500, 9000, 6200),
    e: p("Drive", 0.8, 2.2, 1.15),
    f: p("Level", 0.75, 1.15, 1.0),
    outG: 0,
  }
);

add(
  "MS Guitar Spread",
  "Guitar",
  "MS: mid stays the riff (mild tube), side gets a short delay. Rhythm stays centered, edges bloom.",
  `param a = Drive [0.9, 3.2]
param b = Time [12, 55]
param c = Side [0.15, 0.65]
param d = Tone [2000, 8000]
param e = Mid [0.75, 1.2]
param f = Level [0.55, 1.15]
ms1: mode = encode
stage1: channel = mid; y = tube(x, a) * e
delay1: time = b; feedback = 0.12; mix = c; damp = d; channel = side
ms2: mode = decode
filter1: type = lowpass; cutoff = d; resonance = 0.28
stage2: y = softclip(x * f, 1.08)`,
  {
    a: p("Drive", 0.9, 3.2, 1.6),
    b: p("Time", 12, 55, 28),
    c: p("Side", 0.15, 0.65, 0.34),
    d: p("Tone", 2000, 8000, 4800),
    e: p("Mid", 0.75, 1.2, 1.0),
    f: p("Level", 0.55, 1.15, 0.9),
    outG: 0,
  }
);

add(
  "MS Imager",
  "Mastering",
  "True MS width + side HPF + mid level. Same idea as a simple imager; not linear-phase, not multiband.",
  `param a = Width [0.0, 1.6]
param b = SideHPF [80, 500]
param c = Mid [0.7, 1.25]
param d = Level [0.75, 1.2]
ms1: mode = encode
stage1: channel = mid; y = x * c
filter1: type = highpass; cutoff = b; resonance = 0.28; channel = side
stage2: channel = side; y = x * a
ms2: mode = decode
stage3: y = softclip(x * d, 1.05)`,
  {
    a: p("Width", 0, 1.6, 1.05),
    b: p("SideHPF", 80, 500, 160),
    c: p("Mid", 0.7, 1.25, 1.0),
    d: p("Level", 0.75, 1.2, 1.0),
    outG: 0,
  }
);

add(
  "Plate Send",
  "Reverb",
  "Dry on main. Bus is a short pre-delay into a plate-ish room. Use as insert with Send, or on an FX return.",
  `param a = Predelay [12, 90]
param b = Size [0.18, 0.55]
param c = Decay [0.18, 0.62]
param d = Send [0.12, 0.7]
param e = Damp [0.3, 0.85]
param f = Tone [2800, 9500]
stage1: y = x
bus plate:
  send: in = 1
  delay1: time = a; feedback = 0.04; mix = 1; damp = 7000
  reverb1: size = b; decay = c; damp = e; mix = 1; width = 0.9
  filter1: type = lowpass; cutoff = f; resonance = 0.25
out: main = 1-d; plate = d`,
  {
    a: p("Predelay", 12, 90, 32),
    b: p("Size", 0.18, 0.55, 0.34),
    c: p("Decay", 0.18, 0.62, 0.38),
    d: p("Send", 0.12, 0.7, 0.32),
    e: p("Damp", 0.3, 0.85, 0.55),
    f: p("Tone", 2800, 9500, 6200),
    outG: 0,
  }
);

add(
  "MS Kit Punch",
  "Drums",
  "MS encode: mid gets tube punch, side stays open with a light HPF. Kit stays in the middle.",
  `param a = Punch [1.0, 3.5]
param b = SideHPF [100, 450]
param c = Side [0.75, 1.3]
param d = Mid [0.75, 1.25]
param e = Tone [5000, 13000]
param f = Level [0.6, 1.2]
ms1: mode = encode
stage1: channel = mid; y = tube(x, a) * d
filter1: type = highpass; cutoff = b; resonance = 0.28; channel = side
stage2: channel = side; y = softclip(x * c, 1.05)
ms2: mode = decode
filter2: type = lowpass; cutoff = e; resonance = 0.24
stage3: y = softclip(x * f, 1.07)`,
  {
    a: p("Punch", 1, 3.5, 1.7),
    b: p("SideHPF", 100, 450, 200),
    c: p("Side", 0.75, 1.3, 1.05),
    d: p("Mid", 0.75, 1.25, 1.0),
    e: p("Tone", 5000, 13000, 9000),
    f: p("Level", 0.6, 1.2, 0.95),
    outG: 0,
  }
);

add(
  "Parallel Tape",
  "Mastering",
  "Dry main + tape-ish bus (tube + HF roll). Blend in the weight; it is not a Studer model.",
  `param a = Drive [0.9, 3.2]
param b = Blend [0.15, 0.75]
param c = Dull [4000, 12000]
param d = Level [0.7, 1.2]
stage1: y = x * d
bus tape:
  send: in = 1
  stage2: y = tube(x, a)
  filter1: type = lowpass; cutoff = c; resonance = 0.28
  stage3: y = softclip(y * d, 1.06)
out: main = 1-b; tape = b`,
  {
    a: p("Drive", 0.9, 3.2, 1.55),
    b: p("Blend", 0.15, 0.75, 0.4),
    c: p("Dull", 4000, 12000, 7200),
    d: p("Level", 0.7, 1.2, 1.0),
    outG: 0,
  }
);

add(
  "MS Presence",
  "Vocals",
  "MS: mid HPF + side HPF, then a parallel presence peak on mid only. Does not replace a de-esser.",
  `param a = Presence [1800, 5200]
param b = Amount [0.12, 0.55]
param c = SideHPF [140, 600]
param d = Side [0.7, 1.25]
param e = LowCut [70, 200]
param f = Level [0.75, 1.15]
ms1: mode = encode
filter1: type = highpass; cutoff = e; resonance = 0.3; channel = mid
stage1: channel = mid; y = x * f
filter2: type = highpass; cutoff = c; resonance = 0.3; channel = side
stage2: channel = side; y = softclip(x * d, 1.04)
ms2: mode = decode
bus air:
  send: in = 1
  ms3: mode = encode
  eq1: type = peak; freq = a; q = 1.15; gain = 5; channel = mid
  stage3: channel = mid; y = softclip(x, 1.12)
  stage4: channel = side; y = x * 0.0
  ms4: mode = decode
  filter3: type = lowpass; cutoff = 12000; resonance = 0.22
out: main = 1; air = b`,
  {
    a: p("Presence", 1800, 5200, 3200),
    b: p("Amount", 0.12, 0.55, 0.28),
    c: p("SideHPF", 140, 600, 260),
    d: p("Side", 0.7, 1.25, 1.0),
    e: p("LowCut", 70, 200, 110),
    f: p("Level", 0.75, 1.15, 1.0),
    outG: 0,
  }
);

add(
  "Width Delay",
  "Delay",
  "Dry on main, ping-pong delay on a bus. Stereo movement without washing the center.",
  `param a = Time [120, 480]
param b = Feedback [0.15, 0.6]
param c = Send [0.12, 0.62]
param d = Damp [800, 6500]
param e = Drive [0.8, 2.4]
param f = Level [0.75, 1.15]
stage1: y = x * f
bus ping:
  send: in = 1
  delay1: time = a; feedback = b; mix = 1; damp = d; pingpong = true
  stage2: y = softclip(x, e)
  filter1: type = lowpass; cutoff = 9000; resonance = 0.25
out: main = 1-c; ping = c`,
  {
    a: p("Time", 120, 480, 240),
    b: p("Feedback", 0.15, 0.6, 0.36),
    c: p("Send", 0.12, 0.62, 0.3),
    d: p("Damp", 800, 6500, 3600),
    e: p("Drive", 0.8, 2.4, 1.2),
    f: p("Level", 0.75, 1.15, 1.0),
    outG: 0,
  }
);

// =============================================================================
// PSYCHOACOUSTIC — mix tricks the ear, not a hearing-lab suite
// =============================================================================
add(
  "Haas Width",
  "Psychoacoustic",
  "Precedence: left stays put, right is a short delay (6-40 ms). Width without a chorus. Not a true stereoizer.",
  `param a = Time [6, 40]
param b = Mix [0.2, 0.75]
param c = Tone [3500, 12000]
param d = Level [0.75, 1.15]
stage1: channel = left; y = x * d
delay1: time = a; feedback = 0.04; mix = b; damp = c; channel = right
filter1: type = lowpass; cutoff = c; resonance = 0.22
stage2: y = softclip(x, 1.04)`,
  {
    a: p("Time", 6, 40, 18),
    b: p("Mix", 0.2, 0.75, 0.42),
    c: p("Tone", 3500, 12000, 8000),
    d: p("Level", 0.75, 1.15, 1.0),
    tags: ["haas", "width", "psychoacoustic"],
    outG: 0,
  }
);

add(
  "Mono to Stereo",
  "Psychoacoustic",
  "True stereoizer: mid stays the source, side is allpass + Haas above the bass. Mono sum stays the original. Works on a mono send or a collapsed DI.",
  `param a = Width [0.2, 1.15]
param b = Haas [8, 28]
param c = Bass [80, 240]
widen1: width = a; delay = b; bass = c`,
  {
    a: p("Width", 0.2, 1.15, 0.72),
    b: p("Haas", 8, 28, 14),
    c: p("Bass", 80, 240, 130),
    tags: ["widen", "stereo", "width", "mono", "psychoacoustic"],
    outG: 0,
  }
);

add(
  "Loudness Curve",
  "Psychoacoustic",
  "Rough equal-loudness habit: keep the body, add a 3 kHz presence peak. Not K-weighting, not a loudness meter.",
  `param a = Contour [0.12, 0.65]
param b = Presence [2200, 4500]
param c = Air [7000, 14000]
param d = Rumble [35, 100]
param e = Level [0.7, 1.2]
filter1: type = highpass; cutoff = d; resonance = 0.24
stage1: y = x * e
bus curve:
  send: in = 1
  eq1: type = peak; freq = b; q = 1.05; gain = 5.5
  stage2: y = softclip(x, 1.08)
  filter2: type = lowpass; cutoff = c; resonance = 0.22
out: main = 1-a; curve = a`,
  {
    a: p("Contour", 0.12, 0.65, 0.32),
    b: p("Presence", 2200, 4500, 3200),
    c: p("Air", 7000, 14000, 11000),
    d: p("Rumble", 35, 100, 55),
    e: p("Level", 0.7, 1.2, 1.0),
    tags: ["psychoacoustic", "presence", "loudness"],
    outG: 0,
  }
);

add(
  "Missing Bass",
  "Psychoacoustic",
  "High-pass the main path, add tube harmonics of the lows. Small speakers still imply the fundamental.",
  `param a = Cross [70, 180]
param b = Harm [1.2, 4.0]
param c = Blend [0.15, 0.6]
param d = Level [0.7, 1.2]
filter1: type = highpass; cutoff = a; resonance = 0.22
stage1: y = x * d
bus harm:
  send: in = 1
  filter2: type = lowpass; cutoff = a; resonance = 0.26
  stage2: y = tube(x, b)
  filter3: type = lowpass; cutoff = 3200; resonance = 0.28
out: main = 1-c; harm = c`,
  {
    a: p("Cross", 70, 180, 110),
    b: p("Harm", 1.2, 4, 2.2),
    c: p("Blend", 0.15, 0.6, 0.34),
    d: p("Level", 0.7, 1.2, 1.0),
    tags: ["psychoacoustic", "bass", "harmonics"],
    outG: 0,
  }
);

add(
  "Speech Band",
  "Psychoacoustic",
  "Parallel 2-4 kHz bump so dialogue and lead lines cut. Use on a bus, not as a full mix EQ.",
  `param a = Band [1800, 4200]
param b = Amount [0.12, 0.55]
param c = LowCut [70, 180]
param d = Air [7000, 13000]
param e = Level [0.75, 1.15]
filter1: type = highpass; cutoff = c; resonance = 0.28
stage1: y = x * e
bus talk:
  send: in = 1
  filter2: type = bandpass; center = a; width = 1400
  stage2: y = softclip(x, 1.12)
  filter3: type = lowpass; cutoff = d; resonance = 0.22
out: main = 1-b; talk = b`,
  {
    a: p("Band", 1800, 4200, 2800),
    b: p("Amount", 0.12, 0.55, 0.28),
    c: p("LowCut", 70, 180, 100),
    d: p("Air", 7000, 13000, 10000),
    e: p("Level", 0.75, 1.15, 1.0),
    tags: ["psychoacoustic", "dialogue", "presence"],
    outG: 0,
  }
);

// =============================================================================
// CINEMATIC — score / FX / dialogue processing, not a trailer sample pack
// =============================================================================
add(
  "Trailer Impact",
  "Cinematic",
  "Dry hit on main. Bus is tube smash + short pre-delay into a tight chamber. For impacts, not music.",
  `param a = Drive [1.2, 4.5]
param b = Size [0.16, 0.48]
param c = Decay [0.12, 0.42]
param d = Blend [0.18, 0.68]
param e = Predelay [8, 48]
param f = Tone [2500, 8000]
stage1: y = x
bus smash:
  send: in = 1
  stage2: y = tube(x, a)
  delay1: time = e; feedback = 0.04; mix = 1; damp = 5500
  reverb1: size = b; decay = c; damp = 0.58; mix = 1; width = 0.65
  filter1: type = lowpass; cutoff = f; resonance = 0.28
  stage3: y = softclip(y, 1.08)
out: main = 1-d; smash = d`,
  {
    a: p("Drive", 1.2, 4.5, 2.4),
    b: p("Size", 0.16, 0.48, 0.3),
    c: p("Decay", 0.12, 0.42, 0.24),
    d: p("Blend", 0.18, 0.68, 0.4),
    e: p("Predelay", 8, 48, 22),
    f: p("Tone", 2500, 8000, 4800),
    tags: ["cinematic", "trailer", "impact"],
    outG: 0,
  }
);

add(
  "Score Hall",
  "Cinematic",
  "Predelay into a large dark hall, then MS width. Orchestral / score send. Algorithmic, not convolution.",
  `param a = Predelay [24, 140]
param b = Size [0.48, 0.95]
param c = Decay [0.42, 0.88]
param d = Mix [0.18, 0.62]
param e = Damp [0.28, 0.82]
param f = Width [0.65, 1.5]
stage1: y = x
bus hall:
  send: in = 1
  delay1: time = a; feedback = 0; mix = 1; damp = 6500
  reverb1: size = b; decay = c; damp = e; mix = 1; width = 0.92
  ms1: mode = encode
  stage2: channel = mid; y = x
  stage3: channel = side; y = x * f
  ms2: mode = decode
  filter1: type = lowpass; cutoff = 9200; resonance = 0.24
  stage4: y = softclip(x, 1.05)
out: main = 1-d; hall = d`,
  {
    a: p("Predelay", 24, 140, 68),
    b: p("Size", 0.48, 0.95, 0.78),
    c: p("Decay", 0.42, 0.88, 0.7),
    d: p("Mix", 0.18, 0.62, 0.36),
    e: p("Damp", 0.28, 0.82, 0.52),
    f: p("Width", 0.65, 1.5, 1.05),
    tags: ["cinematic", "score", "hall"],
    outG: 0,
  }
);

add(
  "Dialogue Seat",
  "Cinematic",
  "Chair-level speech: low cut, light glue, parallel presence, tiny room. For ADR and production dialogue.",
  `param a = LowCut [80, 200]
param b = Presence [1800, 4000]
param c = Amount [0.12, 0.5]
param d = Room [0.06, 0.32]
param e = Thresh [-22.0, -8.0]
param f = Air [6000, 12000]
filter1: type = highpass; cutoff = a; resonance = 0.3
stage1: y = x
comp1: threshold = e; ratio = 2.6; attack = 0.006; release = 0.11
bus talk:
  send: in = 1
  filter2: type = bandpass; center = b; width = 1300
  stage2: y = softclip(x, 1.1)
  filter3: type = lowpass; cutoff = f; resonance = 0.22
bus room:
  send: in = 1
  reverb1: size = 0.22; decay = 0.22; damp = 0.62; mix = 1; width = 0.45
  filter4: type = lowpass; cutoff = f; resonance = 0.24
out: main = 1; talk = c; room = d`,
  {
    a: p("LowCut", 80, 200, 110),
    b: p("Presence", 1800, 4000, 2700),
    c: p("Amount", 0.12, 0.5, 0.26),
    d: p("Room", 0.06, 0.32, 0.14),
    e: p("Thresh", -22, -8, -14),
    f: p("Air", 6000, 12000, 9500),
    tags: ["cinematic", "dialogue"],
    outG: 0,
  }
);

add(
  "Far Plane",
  "Cinematic",
  "Push a layer back: high-pass, long predelay, dark hall. Distance cue, not a worldizer.",
  `param a = Near [140, 520]
param b = Predelay [45, 180]
param c = Size [0.42, 0.9]
param d = Mix [0.28, 0.75]
param e = Damp [0.42, 0.88]
param f = Level [0.6, 1.1]
filter1: type = highpass; cutoff = a; resonance = 0.26
stage1: y = x * f
bus hall:
  send: main = 1
  delay1: time = b; feedback = 0; mix = 1; damp = 5000
  reverb1: size = c; decay = 0.72; damp = e; mix = 1; width = 0.88
  filter2: type = lowpass; cutoff = 6800; resonance = 0.24
  stage2: y = softclip(x, 1.05)
out: main = 1-d; hall = d`,
  {
    a: p("Near", 140, 520, 260),
    b: p("Predelay", 45, 180, 95),
    c: p("Size", 0.42, 0.9, 0.68),
    d: p("Mix", 0.28, 0.75, 0.48),
    e: p("Damp", 0.42, 0.88, 0.62),
    f: p("Level", 0.6, 1.1, 0.92),
    tags: ["cinematic", "distance"],
    outG: 0,
  }
);

add(
  "Boom Tail",
  "Cinematic",
  "Sub-safe boom: rumble cut, tube weight, long dark tail. Park on an impact or drone send.",
  `param a = Drive [1.3, 4.0]
param b = Size [0.52, 0.95]
param c = Decay [0.48, 0.9]
param d = Mix [0.2, 0.68]
param e = Sub [40, 110]
param f = Tone [1800, 6500]
filter1: type = highpass; cutoff = e; resonance = 0.24
stage1: y = tube(x, a)
reverb1: size = b; decay = c; damp = 0.7; mix = d; width = 0.8
filter2: type = lowpass; cutoff = f; resonance = 0.26
stage2: y = softclip(x, 1.06)`,
  {
    a: p("Drive", 1.3, 4, 2.2),
    b: p("Size", 0.52, 0.95, 0.78),
    c: p("Decay", 0.48, 0.9, 0.72),
    d: p("Mix", 0.2, 0.68, 0.4),
    e: p("Sub", 40, 110, 62),
    f: p("Tone", 1800, 6500, 3800),
    tags: ["cinematic", "boom", "trailer"],
    outG: 0,
  }
);

add(
  "Wide Canvas",
  "Cinematic",
  "Picture-wide sides: dry stays on main, a bus halls the side only. Big screen width, center locked.",
  `param a = Size [0.35, 0.85]
param b = Decay [0.3, 0.75]
param c = Send [0.14, 0.58]
param d = Damp [0.28, 0.78]
param e = Mid [0.75, 1.2]
param f = Tone [3200, 9500]
stage1: y = x
ms1: mode = encode
stage2: channel = mid; y = x * e
ms2: mode = decode
bus sides:
  send: in = 1
  ms3: mode = encode
  stage3: channel = mid; y = x * 0.0
  stage4: channel = side; y = x
  ms4: mode = decode
  reverb1: size = a; decay = b; damp = d; mix = 1; width = 1.0
  filter1: type = lowpass; cutoff = f; resonance = 0.24
out: main = 1-c; sides = c`,
  {
    a: p("Size", 0.35, 0.85, 0.58),
    b: p("Decay", 0.3, 0.75, 0.5),
    c: p("Send", 0.14, 0.58, 0.32),
    d: p("Damp", 0.28, 0.78, 0.48),
    e: p("Mid", 0.75, 1.2, 1.0),
    f: p("Tone", 3200, 9500, 6800),
    tags: ["cinematic", "width", "mid-side"],
    outG: 0,
  }
);

add(
  "Tension Bed",
  "Cinematic",
  "Slow pulse on a dark hall over mild tube. Underscore bed, not a riser sample.",
  `param a = Drive [0.9, 2.8]
param b = Size [0.4, 0.88]
param c = Mix [0.22, 0.7]
param d = Rate [0.05, 0.8]
param e = Depth [0.12, 0.5]
param f = Tone [2500, 8000]
osc1: shape = sine; freq = d
stage1: y = tube(x, a)
reverb1: size = b; decay = 0.7; damp = 0.62; mix = 1; width = 0.85
stage2: y = y * (1.0 - e + e * (0.5 + 0.5 * osc1))
filter1: type = lowpass; cutoff = f; resonance = 0.25
stage3: y = lerp(x, softclip(y, 1.06), c)`,
  {
    a: p("Drive", 0.9, 2.8, 1.5),
    b: p("Size", 0.4, 0.88, 0.64),
    c: p("Mix", 0.22, 0.7, 0.42),
    d: p("Rate", 0.05, 0.8, 0.16),
    e: p("Depth", 0.12, 0.5, 0.28),
    f: p("Tone", 2500, 8000, 4800),
    tags: ["cinematic", "score", "tension"],
    outG: 0,
  }
);

add(
  "JCM Hot Lead",
  "Distortion",
  "800-style lead: aggressive HPF, stacked tube, mid scoop then presence, dark cab.",
  `param a = Gain [2.5, 10.0]
param b = Scoop [400, 1200]
param c = Presence [2500, 7000]
param d = Level [0.28, 0.95]
gate1: threshold = -46; hyst = 6; hold = 0.03; range = -80
filter1: type = highpass; cutoff = 110; resonance = 0.42
stage1: y = tube(x, a * 0.55)
stage2: y = tube(y, a * 0.7)
eq1: type = notch; freq = b; q = 1.1; gain = 0
eq2: type = highshelf; freq = c; q = 0.7; gain = 3.5
stage3: y = softclip(y * 1.25, 1.45)
filter2: type = lowpass; cutoff = 4800; resonance = 0.48
stage4: y = diode(y, 1.35) * d
ir1: mix = 0.45; gain = 0
limit1: ceiling = -0.3; release = 0.08`,
  {
    a: p("Gain", 2.5, 10, 5.8),
    b: p("Scoop", 400, 1200, 720),
    c: p("Presence", 2500, 7000, 4200),
    d: p("Level", 0.28, 0.95, 0.52),
    tags: ["amp", "guitar", "lead", "crunch", "gate", "ir"],
    outG: 0,
    irs: { ir1: "British IR 01.wav" },
  }
);

add(
  "SLO Crunch",
  "Distortion",
  "Tight American high-gain: steep HPF, triple tube, bright shelf, closed cab.",
  `param a = Gain [3.0, 11.0]
param b = Tight [55, 180]
param c = Presence [3000, 8000]
param d = Level [0.22, 0.85]
filter1: type = highpass; cutoff = b; resonance = 0.55
stage1: y = tube(x, a * 0.48)
stage2: y = tube(y, a * 0.62)
stage3: y = tube(y, a * 0.4)
eq1: type = highshelf; freq = c; q = 0.65; gain = 4
stage4: y = hardclip(softclip(y, 1.5), 0.72)
filter2: type = lowpass; cutoff = 4300; resonance = 0.52
stage5: y = diode(y, 1.45) * d`,
  {
    a: p("Gain", 3, 11, 6.8),
    b: p("Tight", 55, 180, 95),
    c: p("Presence", 3000, 8000, 5200),
    d: p("Level", 0.22, 0.85, 0.4),
    tags: ["amp", "guitar", "high-gain"],
    outG: 0,
  }
);

add(
  "Orange Crush",
  "Distortion",
  "British mid grind: modest HPF, diode+tube, strong mid peak, open cab.",
  `param a = Drive [1.6, 7.5]
param b = Mid [500, 1800]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 85; resonance = 0.32
stage1: y = diode(x, a * 0.55)
stage2: y = tube(y, a * 0.5)
eq1: type = peak; freq = b; q = 1.4; gain = 5
stage3: y = softclip(y * 1.15, 1.4) * c
filter2: type = lowpass; cutoff = 6200; resonance = 0.34`,
  {
    a: p("Drive", 1.6, 7.5, 3.4),
    b: p("Mid", 500, 1800, 920),
    c: p("Level", 0.4, 1.2, 0.78),
    tags: ["amp", "guitar", "crunch"],
    outG: 0,
  }
);

add(
  "SVT Grind",
  "Bass",
  "Flip-top grind: 40 Hz HPF, mid scoop, hot tube, 4.5 kHz cab.",
  `param a = Drive [1.4, 6.5]
param b = Scoop [280, 900]
param c = Cab [2800, 6500]
param d = Level [0.45, 1.2]
filter1: type = highpass; cutoff = 40; resonance = 0.28
eq1: type = notch; freq = b; q = 0.9; gain = 0
stage1: y = tube(x, a)
stage2: y = softclip(y, 1.2)
filter2: type = lowpass; cutoff = c; resonance = 0.36
stage3: y = diode(y, 1.15) * d`,
  {
    a: p("Drive", 1.4, 6.5, 2.8),
    b: p("Scoop", 280, 900, 520),
    c: p("Cab", 2800, 6500, 4500),
    d: p("Level", 0.45, 1.2, 0.88),
    tags: ["amp", "bass", "tube"],
    outG: 0,
  }
);

add(
  "Porta Bass",
  "Bass",
  "Round combo: gentle HPF, mild tube, warm low shelf, early cab roll-off.",
  `param a = Drive [0.8, 3.2]
param b = Warm [80, 220]
param c = Cab [2200, 5000]
param d = Level [0.55, 1.3]
filter1: type = highpass; cutoff = 32; resonance = 0.2
eq1: type = lowshelf; freq = b; q = 0.7; gain = 3
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = c; resonance = 0.28
stage2: y = softclip(y, 1.08) * d`,
  {
    a: p("Drive", 0.8, 3.2, 1.6),
    b: p("Warm", 80, 220, 140),
    c: p("Cab", 2200, 5000, 3400),
    d: p("Level", 0.55, 1.3, 0.98),
    tags: ["amp", "bass", "clean"],
    outG: 0,
  }
);

add(
  "Glass Blend",
  "Bass",
  "Clean/grit blend: dry path plus tight tube+LPF dirt. Mix is the blend.",
  `param a = Drive [1.8, 8.0]
param b = Blend [0.15, 0.85]
param c = Tone [1800, 5500]
param d = Level [0.5, 1.2]
filter1: type = highpass; cutoff = 35; resonance = 0.22
stage1: y = x * d
bus grit:
  send: in = 1
  filter2: type = highpass; cutoff = 50; resonance = 0.3
  stage2: y = tube(x, a)
  stage3: y = diode(y, 1.3)
  filter3: type = lowpass; cutoff = c; resonance = 0.4
  stage4: y = softclip(y, 1.1) * d
out: main = 1-b; grit = b`,
  {
    a: p("Drive", 1.8, 8, 3.6),
    b: p("Blend", 0.15, 0.85, 0.45),
    c: p("Tone", 1800, 5500, 3200),
    d: p("Level", 0.5, 1.2, 0.9),
    tags: ["amp", "bass", "blend", "parallel"],
    outG: 0,
  }
);

add(
  "Precision Octaver",
  "Pitch",
  "OC-style analog octaver: mid-clocked divider on −1, rectifier on +1. Track the dry string, then warm the blend.",
  `param a = Sub [0.0, 1.2]
param b = Up [0.0, 0.7]
param c = Tone [120, 800]
param d = Mix [0.18, 0.82]
param e = Track [0.02, 0.12]
param f = Level [0.7, 1.15]
filter1: type = highpass; cutoff = 42; resonance = 0.16
octaver1: sub = a; up = b; mix = d; tone = c; thresh = e
stage1: y = tube(x, 1.04) * f
filter2: type = lowpass; cutoff = 6200; resonance = 0.2
stage2: y = softclip(y, 1.04)`,
  {
    a: p("Sub", 0, 1.2, 0.84),
    b: p("Up", 0, 0.7, 0.1),
    c: p("Tone", 120, 800, 240),
    d: p("Mix", 0.18, 0.82, 0.52),
    e: p("Track", 0.02, 0.12, 0.05),
    f: p("Level", 0.7, 1.15, 1.0),
    tags: ["octaver", "pitch", "bass", "guitar"],
    outG: 0,
  }
);

add(
  "Bass Sub Octave",
  "Bass",
  "Sub-only analog divider for bass. Clean track, then a little tube on the blend.",
  `param a = Sub [0.25, 1.15]
param b = Tone [80, 280]
param c = Mix [0.22, 0.78]
param d = Level [0.7, 1.2]
filter1: type = highpass; cutoff = 30; resonance = 0.16
octaver1: sub = a; up = 0; mix = c; tone = b; thresh = 0.04
stage1: y = tube(x, 1.05) * d
filter2: type = lowpass; cutoff = 4200; resonance = 0.2
stage2: y = softclip(y, 1.03)`,
  {
    a: p("Sub", 0.25, 1.15, 0.8),
    b: p("Tone", 80, 280, 140),
    c: p("Mix", 0.22, 0.78, 0.48),
    d: p("Level", 0.7, 1.2, 1.0),
    tags: ["octaver", "bass", "sub"],
    outG: 0,
  }
);

add(
  "Vocoder Bank",
  "Vocals",
  "16-band analog vocoder. Insert on a pad/synth (carrier). Pin the voice on Sidechain or connect the voice-jack. Empty sidechain self-vocodes.",
  `param a = Mix [0.35, 1.0]
param b = Q [0.8, 4.0]
param c = Dry [0.0, 0.4]
param d = Formant [0.72, 1.4]
param e = CarrierHP [60, 280]
param f = Level [0.7, 1.35]
filter1: type = highpass; cutoff = e; resonance = 0.18
stage1: y = x * f
vocoder1: bands = 16; mix = a; q = b; formant = d; dry = c; attack = 0.003; release = 0.030
filter2: type = lowpass; cutoff = 8500; resonance = 0.2
stage2: y = softclip(y, 1.12)`,
  {
    a: p("Mix", 0.35, 1, 0.92),
    b: p("Q", 0.8, 4.0, 1.35),
    c: p("Dry", 0, 0.4, 0.12),
    d: p("Formant", 0.72, 1.4, 1.0),
    e: p("CarrierHP", 60, 280, 90),
    f: p("Level", 0.7, 1.35, 1.08),
    tags: ["vocoder", "sidechain", "vocal"],
    outG: 0,
  }
);

add(
  "Vocoder Lite",
  "Vocals",
  "8-band vocoder, lighter CPU. Carrier on this track, voice on the Sidechain pin or voice-jack.",
  `param a = Mix [0.3, 1.0]
param b = Q [1.1, 4.5]
param c = Dry [0.08, 0.6]
param d = Formant [0.8, 1.28]
param e = Level [0.55, 1.2]
stage1: y = x * e
vocoder1: bands = 8; mix = a; q = b; formant = d; dry = c; attack = 0.003; release = 0.030
filter1: type = lowpass; cutoff = 8500; resonance = 0.2
stage2: y = softclip(y, 1.08)`,
  {
    a: p("Mix", 0.3, 1, 0.86),
    b: p("Q", 1.1, 4.5, 2.0),
    c: p("Dry", 0.08, 0.6, 0.32),
    d: p("Formant", 0.8, 1.28, 1.0),
    e: p("Level", 0.55, 1.2, 1.12),
    tags: ["vocoder", "sidechain", "vocal"],
    outG: 0,
  }
);

add(
  "AC30 Chime",
  "Distortion",
  "Top-boost chime: modest HPF, dual tube, mid scoop, airy 7 kHz cab.",
  `param a = Drive [1.2, 6.5]
param b = Scoop [600, 1600]
param c = Air [4500, 9000]
param d = Level [0.4, 1.15]
filter1: type = highpass; cutoff = 75; resonance = 0.3
stage1: y = tube(x, a * 0.5)
stage2: y = tube(y, a * 0.42)
eq1: type = notch; freq = b; q = 1.15; gain = 0
eq2: type = highshelf; freq = c; q = 0.65; gain = 3.2
stage3: y = softclip(y * 1.12, 1.28) * d
filter2: type = lowpass; cutoff = 7200; resonance = 0.3
ir1: mix = 0.35; gain = 0`,
  {
    a: p("Drive", 1.2, 6.5, 2.8),
    b: p("Scoop", 600, 1600, 980),
    c: p("Air", 4500, 9000, 6800),
    d: p("Level", 0.4, 1.15, 0.82),
    tags: ["amp", "guitar", "clean", "chime", "ir"],
    outG: 0,
    irs: { ir1: "Vintage IR 01.wav" },
  }
);

add(
  "5150 Lead",
  "Distortion",
  "Modern high-gain: steep HPF, triple tube, tight low cut, dark 4.2 kHz cab.",
  `param a = Gain [4.0, 12.0]
param b = Tight [70, 220]
param c = Presence [2800, 7000]
param d = Level [0.2, 0.8]
gate1: threshold = -44; hyst = 8; hold = 0.025; range = -80
filter1: type = highpass; cutoff = b; resonance = 0.58
stage1: y = tube(x, a * 0.46)
stage2: y = tube(y, a * 0.58)
stage3: y = tube(y, a * 0.38)
eq1: type = highshelf; freq = c; q = 0.7; gain = 3.8
stage4: y = hardclip(softclip(y, 1.48), 0.7)
filter2: type = lowpass; cutoff = 4200; resonance = 0.5
stage5: y = diode(y, 1.5) * d
ir1: mix = 0.5; gain = 0
limit1: ceiling = -0.3; release = 0.08`,
  {
    a: p("Gain", 4, 12, 7.4),
    b: p("Tight", 70, 220, 110),
    c: p("Presence", 2800, 7000, 4800),
    d: p("Level", 0.2, 0.8, 0.38),
    tags: ["amp", "guitar", "high-gain", "lead", "gate", "ir"],
    outG: 0,
    irs: { ir1: "American IR 01.wav" },
  }
);

add(
  "Deluxe Sparkle",
  "Distortion",
  "American deluxe: single tube, bright shelf, open 8.5 kHz cab, gentle clip.",
  `param a = Drive [0.6, 3.4]
param b = Bright [2500, 8000]
param c = Level [0.55, 1.35]
filter1: type = highpass; cutoff = 50; resonance = 0.22
stage1: y = tube(x, a)
eq1: type = highshelf; freq = b; q = 0.6; gain = 2.8
stage2: y = softclip(y, 0.95) * c
filter2: type = lowpass; cutoff = 8500; resonance = 0.24`,
  {
    a: p("Drive", 0.6, 3.4, 1.35),
    b: p("Bright", 2500, 8000, 5200),
    c: p("Level", 0.55, 1.35, 1.05),
    tags: ["amp", "guitar", "clean"],
    outG: 0,
  }
);

add(
  "Bassman Grind",
  "Bass",
  "4x10 grind: 35 Hz HPF, warm tube, mid punch, 5 kHz cab.",
  `param a = Drive [1.1, 5.5]
param b = Punch [180, 700]
param c = Cab [2800, 6200]
param d = Level [0.5, 1.25]
filter1: type = highpass; cutoff = 35; resonance = 0.24
eq1: type = peak; freq = b; q = 1.1; gain = 3.5
stage1: y = tube(x, a)
stage2: y = softclip(y, 1.15)
filter2: type = lowpass; cutoff = c; resonance = 0.32
stage3: y = diode(y, 1.12) * d`,
  {
    a: p("Drive", 1.1, 5.5, 2.4),
    b: p("Punch", 180, 700, 380),
    c: p("Cab", 2800, 6200, 5000),
    d: p("Level", 0.5, 1.25, 0.92),
    tags: ["amp", "bass", "tube"],
    outG: 0,
  }
);

add(
  "B15 Flip",
  "Bass",
  "Portable flip-top: gentle HPF, mild tube, low shelf bloom, early 3.2 kHz cab.",
  `param a = Drive [0.7, 2.8]
param b = Bloom [70, 180]
param c = Cab [2200, 4200]
param d = Level [0.6, 1.3]
filter1: type = highpass; cutoff = 30; resonance = 0.18
eq1: type = lowshelf; freq = b; q = 0.7; gain = 3.2
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = c; resonance = 0.26
stage2: y = softclip(y, 1.05) * d`,
  {
    a: p("Drive", 0.7, 2.8, 1.4),
    b: p("Bloom", 70, 180, 110),
    c: p("Cab", 2200, 4200, 3200),
    d: p("Level", 0.6, 1.3, 1.02),
    tags: ["amp", "bass", "clean"],
    outG: 0,
  }
);

add(
  "Trace Filter",
  "Bass",
  "Graphic-preamp bass: HPF, mid scoop, presence peak, tight cab, mild tube.",
  `param a = Drive [0.8, 3.6]
param b = Scoop [350, 900]
param c = Presence [1800, 4500]
param d = Level [0.55, 1.25]
filter1: type = highpass; cutoff = 38; resonance = 0.28
eq1: type = notch; freq = b; q = 0.95; gain = 0
eq2: type = peak; freq = c; q = 1.3; gain = 3.8
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = 5200; resonance = 0.3
stage2: y = softclip(y, 1.08) * d`,
  {
    a: p("Drive", 0.8, 3.6, 1.7),
    b: p("Scoop", 350, 900, 560),
    c: p("Presence", 1800, 4500, 2800),
    d: p("Level", 0.55, 1.25, 0.98),
    tags: ["amp", "bass", "eq"],
    outG: 0,
  }
);

// =============================================================================
// DYNAMICS HARDWARE + MULTIBAND + ENVELOPE
// Times/ratios follow published front-panel ranges, then floor to the engine:
//   attack 0.001–0.3 s, release 0.01–1.0 s, ratio 1–20, thresh −60–0 dB.
// These are character sketches, not circuit clones.
// =============================================================================
add(
  "Multiband Glue",
  "Dynamics",
  "3-band VCA-style: 12 dB/oct splits at LowX / HighX. Low thresh sits 4 dB higher (less kick pump), highs 2 dB lower. Butterworth-ish Q 0.71.",
  `param a = LowX [80, 280]
param b = HighX [1800, 5500]
param c = Thresh [-26.0, -8.0]
param d = Ratio [1.8, 8.0]
param e = Attack [0.003, 0.04]
param f = Mix [0.35, 1.0]
bus low:
  send: in = 1
  filter1: type = lowpass; cutoff = a; resonance = 0.71
  comp1: threshold = c + 4; ratio = d; attack = e; release = 0.16
  stage1: y = softclip(x * 1.04, 1.06)
bus mid:
  send: in = 1
  filter2: type = highpass; cutoff = a; resonance = 0.71
  filter3: type = lowpass; cutoff = b; resonance = 0.71
  comp2: threshold = c; ratio = d; attack = e; release = 0.12
  stage2: y = softclip(x, 1.05)
bus high:
  send: in = 1
  filter4: type = highpass; cutoff = b; resonance = 0.71
  comp3: threshold = c - 2; ratio = d * 0.85; attack = e * 0.7; release = 0.09
  stage3: y = softclip(x, 1.04)
out: main = 1-f; low = f; mid = f; high = f`,
  {
    a: p("LowX", 80, 280, 140),
    b: p("HighX", 1800, 5500, 3200),
    c: p("Thresh", -26, -8, -16),
    d: p("Ratio", 1.8, 8, 3.2),
    e: p("Attack", 0.003, 0.04, 0.01),
    f: p("Mix", 0.35, 1, 0.72),
    tags: ["compressor", "multiband", "glue"],
    outG: 0,
  }
);

add(
  "Envelope Shaper",
  "Dynamics",
  "Transient designer: fast peak env minus slower RMS body. Attack + punches the difference, Sustain + lifts the body. Clamp keeps it finite.",
  `param a = Attack [-1.0, 1.0]
param b = Sustain [-0.85, 0.85]
param c = Fast [0.008, 0.05]
param d = Body [0.08, 0.4]
param e = Mix [0.35, 1.0]
param f = Level [0.6, 1.25]
env1: type = peak; attack = 0.001; release = c
env2: type = rms; attack = 0.014; release = d
stage1: y = lerp(x, x * clamp(1 + a * (env1 - env2) + b * env2, 0.14, 3.4), e) * f
filter1: type = lowpass; cutoff = 14000; resonance = 0.22`,
  {
    a: p("Attack", -1, 1, 0.35),
    b: p("Sustain", -0.85, 0.85, -0.12),
    c: p("Fast", 0.008, 0.05, 0.018),
    d: p("Body", 0.08, 0.4, 0.18),
    e: p("Mix", 0.35, 1, 0.85),
    f: p("Level", 0.6, 1.25, 1.18),
    tags: ["envelope", "transient", "shaper"],
    outG: 0,
  }
);

add(
  "1176 FET",
  "Dynamics",
  "UREI 1176LN-style FET: Input drives a fixed −16 dB threshold. Attack floor 1 ms (panel 20–800 µs). Release 50–1100 ms. Ratio 4/8/12/20. Class-A grit after GR.",
  `param a = Input [1.2, 6.5]
param b = Ratio [4.0, 20.0]
param c = Attack [0.001, 0.02]
param d = Release [0.05, 1.0]
param e = Output [0.45, 1.35]
filter1: type = highpass; cutoff = 28; resonance = 0.2
stage1: y = tube(x, a)
comp1: threshold = -16; ratio = b; attack = c; release = d; knee = 2; makeup = 3; hpf = 80
stage2: y = diode(y, 1.18) * e
filter2: type = lowpass; cutoff = 16000; resonance = 0.2`,
  {
    a: p("Input", 1.2, 6.5, 3.4),
    b: p("Ratio", 4, 20, 8),
    c: p("Attack", 0.001, 0.02, 0.001),
    d: p("Release", 0.05, 1, 0.22),
    e: p("Output", 0.45, 1.35, 0.92),
    tags: ["compressor", "fet", "1176"],
    outG: 0,
  }
);

add(
  "1176 All In",
  "Dynamics",
  "All-buttons-in: ratio pegged at 20, threshold −20 dB, attack 1 ms, release ~110 ms. Extra even harmonics. Drum smash, not a vocal default.",
  `param a = Input [1.6, 7.5]
param b = Release [0.05, 0.35]
param c = Output [0.35, 1.15]
filter1: type = highpass; cutoff = 35; resonance = 0.22
stage1: y = tube(x, a)
comp1: threshold = -20; ratio = 20; attack = 0.001; release = b; knee = 1; makeup = 4
stage2: y = hardclip(softclip(y * c, 1.25), 0.82)
filter2: type = lowpass; cutoff = 12000; resonance = 0.28`,
  {
    a: p("Input", 1.6, 7.5, 4.2),
    b: p("Release", 0.05, 0.35, 0.11),
    c: p("Output", 0.35, 1.15, 0.95),
    tags: ["compressor", "fet", "1176", "drums"],
    outG: 0,
  }
);

add(
  "LA-2A Opto",
  "Dynamics",
  "Teletronix LA-2A: optical, program-dependent. Attack ~10 ms, release 0.5 s (50% then hang — we use 0.48). Comp ratio ~3:1. Peak Reduction + Gain. T4-cell smoothness, tube makeup.",
  `param a = PeakRed [-28.0, -6.0]
param b = Gain [0.7, 2.2]
param c = Limit [3.0, 12.0]
comp1: threshold = a; ratio = c; attack = 0.01; release = 0.48; knee = 6; makeup = 2; hpf = 60
stage1: y = tube(x * b, 1.12)
filter1: type = lowpass; cutoff = 15000; resonance = 0.2`,
  {
    a: p("PeakRed", -28, -6, -14),
    b: p("Gain", 0.7, 2.2, 1.35),
    c: p("Limit", 3, 12, 3.2),
    tags: ["compressor", "opto", "la2a", "vocal"],
    outG: 0,
  }
);

add(
  "SSL Bus Comp",
  "Dynamics",
  "SSL G-series bus: VCA glue. Panel attack 0.1/0.3/1/3/10/30 ms (engine floor 1 ms). Release 0.1–1.0 s (Auto ≈ 0.3). Ratio 2/4/10. Default 4:1, 3 ms, 0.3 s.",
  `param a = Thresh [-22.0, -6.0]
param b = Ratio [2.0, 10.0]
param c = Attack [0.001, 0.03]
param d = Release [0.1, 1.0]
param e = Makeup [0.85, 1.8]
filter1: type = highpass; cutoff = 30; resonance = 0.2
comp1: threshold = a; ratio = b; attack = c; release = d; knee = 3; makeup = 2; hpf = 90
stage1: y = softclip(x * e, 1.06)`,
  {
    a: p("Thresh", -22, -6, -12),
    b: p("Ratio", 2, 10, 4),
    c: p("Attack", 0.001, 0.03, 0.003),
    d: p("Release", 0.1, 1, 0.3),
    e: p("Makeup", 0.85, 1.8, 1.15),
    tags: ["compressor", "vca", "ssl", "bus"],
    outG: 0,
  }
);

add(
  "Fairchild Mu",
  "Dynamics",
  "Fairchild 670-ish variable-mu: Time Constant 1–2 (0.2 ms / 0.3–0.8 s → 1 ms / 0.45 s). Ratio ~2.5, thickens with GR. Lateral/vertical via mild MS side HPF.",
  `param a = Input [-24.0, -8.0]
param b = Release [0.25, 1.0]
param c = SideHPF [40, 220]
param d = Makeup [0.85, 1.6]
ms1: mode = encode
stage1: channel = mid; y = x
filter1: type = highpass; cutoff = c; resonance = 0.22; channel = side
comp1: threshold = a; ratio = 2.5; attack = 0.001; release = b
ms2: mode = decode
stage2: y = tube(x * d, 1.08)
filter2: type = lowpass; cutoff = 14000; resonance = 0.22`,
  {
    a: p("Input", -24, -8, -15),
    b: p("Release", 0.25, 1, 0.45),
    c: p("SideHPF", 40, 220, 90),
    d: p("Makeup", 0.85, 1.6, 1.12),
    tags: ["compressor", "vari-mu", "fairchild", "bus"],
    outG: 0,
  }
);

add(
  "dbx 160 VCA",
  "Dynamics",
  "dbx 160: RMS / Over Easy. Attack 15 ms, release 120 ms. Clean VCA, almost no color. Ratio 4–inf (we stop at 20). Threshold + Output.",
  `param a = Thresh [-28.0, -6.0]
param b = Ratio [4.0, 20.0]
param c = Output [0.7, 1.6]
comp1: threshold = a; ratio = b; attack = 0.015; release = 0.12
stage1: y = softclip(x * c, 1.04)`,
  {
    a: p("Thresh", -28, -6, -16),
    b: p("Ratio", 4, 20, 6),
    c: p("Output", 0.7, 1.6, 1.1),
    tags: ["compressor", "vca", "dbx"],
    outG: 0,
  }
);

add(
  "CL-1B Vocal",
  "Dynamics",
  "Tube-Tech CL 1B: optical tube, vocal default. Attack 1 ms (fixed-ish), release 50 ms–1 s (panel to 10 s). Ratio 2–10. Interconnect-style smoothness.",
  `param a = Thresh [-26.0, -8.0]
param b = Ratio [2.0, 10.0]
param c = Release [0.05, 1.0]
param d = Makeup [0.8, 1.7]
filter1: type = highpass; cutoff = 70; resonance = 0.22
comp1: threshold = a; ratio = b; attack = 0.001; release = c
stage1: y = tube(x * d, 1.1)`,
  {
    a: p("Thresh", -26, -8, -13),
    b: p("Ratio", 2, 10, 3.2),
    c: p("Release", 0.05, 1, 0.32),
    d: p("Makeup", 0.8, 1.7, 1.2),
    tags: ["compressor", "opto", "vocal", "cl1b"],
    outG: 0,
  }
);

add(
  "Neve Diode Bus",
  "Dynamics",
  "Neve 33609-ish diode-bridge: attack 3–5 ms, recovery 100 ms–1 s (panel to 6 s). Ratio ~2–6. Thick, slow grab. 30 Hz HPF, mild tube.",
  `param a = Thresh [-22.0, -8.0]
param b = Ratio [2.0, 6.0]
param c = Recovery [0.1, 1.0]
param d = Makeup [0.85, 1.55]
filter1: type = highpass; cutoff = 30; resonance = 0.2
comp1: threshold = a; ratio = b; attack = 0.004; release = c
stage1: y = tube(x * d, 1.14)
filter2: type = lowpass; cutoff = 14000; resonance = 0.22`,
  {
    a: p("Thresh", -22, -8, -14),
    b: p("Ratio", 2, 6, 3.5),
    c: p("Recovery", 0.1, 1, 0.35),
    d: p("Makeup", 0.85, 1.55, 1.12),
    tags: ["compressor", "diode", "neve", "bus"],
    outG: 0,
  }
);

add(
  "Distressor Punch",
  "Dynamics",
  "EL8-ish: fast FET-like grab, detector HPF ~6 kHz habit as a 90 Hz HPF on the audio (compromise). Ratio 6, attack 1 ms, release 80–400 ms. British-mode grit.",
  `param a = Input [1.4, 6.0]
param b = Ratio [4.0, 20.0]
param c = Release [0.06, 0.45]
param d = Output [0.5, 1.25]
filter1: type = highpass; cutoff = 90; resonance = 0.28
stage1: y = tube(x, a)
comp1: threshold = -15; ratio = b; attack = 0.001; release = c
stage2: y = diode(y, 1.22) * d
filter2: type = lowpass; cutoff = 13000; resonance = 0.24`,
  {
    a: p("Input", 1.4, 6, 3.1),
    b: p("Ratio", 4, 20, 6),
    c: p("Release", 0.06, 0.45, 0.14),
    d: p("Output", 0.5, 1.25, 0.9),
    tags: ["compressor", "distressor", "punch"],
    outG: 0,
  }
);

// =============================================================================
// DELAY / REVERB HARDWARE
// =============================================================================
add(
  "Space Echo RE-201",
  "Delay",
  "Roland RE-201: tape preamp, heads ~1.0x and 0.52x in parallel (mode 7-ish), Repeat Rate 140–620 ms, Intensity to the edge of runaway, wow ±8 ms @ 0.65 Hz, spring send.",
  `param a = Rate [140, 620]
param b = Intensity [0.18, 0.78]
param c = Mix [0.18, 0.7]
param d = Tone [1400, 4800]
param e = Wow [0.0, 1.0]
param f = Spring [0.0, 0.32]
osc1: shape = sine; freq = 0.65; depth = 1.0
stage1: y = tube(x, 1.58)
bus h1:
  send: in = 1
  delay1: time = a + osc1 * e * 8; feedback = b; mix = 1; damp = d
  stage2: y = softclip(x, 1.08)
bus h2:
  send: in = 1
  delay2: time = a * 0.52 + osc1 * e * 5; feedback = b * 0.55; mix = 1; damp = d
bus tank:
  send: in = 1
  reverb1: size = 0.26; decay = 0.32; damp = 0.52; mix = 1; width = 0.55
out: main = 1-c; h1 = c * 0.58; h2 = c * 0.42; tank = f`,
  {
    a: p("Rate", 140, 620, 310),
    b: p("Intensity", 0.18, 0.78, 0.46),
    c: p("Mix", 0.18, 0.7, 0.4),
    d: p("Tone", 1400, 4800, 2600),
    e: p("Wow", 0, 1, 0.35),
    f: p("Spring", 0, 0.32, 0.12),
    tags: ["delay", "tape", "space echo", "spring"],
    outG: 0,
  }
);

add(
  "Memory Man BBD",
  "Delay",
  "EHX Deluxe Memory Man: bucket-brigade 80–550 ms, dark 1.4–3.2 kHz damp, chorus 0.6 Hz on the delay time (±6 ms), Blend + Feedback. Analog preamp.",
  `param a = Time [80, 550]
param b = Feedback [0.12, 0.68]
param c = Blend [0.18, 0.62]
param d = Age [1200, 3200]
param e = Chorus [0.0, 1.0]
osc1: shape = sine; freq = 0.6; depth = 1.0
stage1: y = tube(x, 1.45)
delay1: time = a + osc1 * e * 6; feedback = b; mix = c; damp = d
stage2: y = softclip(y, 1.08)
filter1: type = lowpass; cutoff = 9000; resonance = 0.24`,
  {
    a: p("Time", 80, 550, 320),
    b: p("Feedback", 0.12, 0.68, 0.4),
    c: p("Blend", 0.18, 0.62, 0.38),
    d: p("Age", 1200, 3200, 2100),
    e: p("Chorus", 0, 1, 0.4),
    tags: ["delay", "analog", "bbd", "chorus"],
    outG: 0,
  }
);

add(
  "Echoplex EP-3",
  "Delay",
  "Maestro EP-3: tube record amp, 70–400 ms tape path, Sustain = feedback, Echo = mix, Record = drive. Darker than Space Echo, less wow.",
  `param a = Time [70, 400]
param b = Sustain [0.12, 0.7]
param c = Echo [0.16, 0.58]
param d = Record [1.1, 3.4]
param e = Age [1600, 4200]
stage1: y = tube(x, d)
delay1: time = a; feedback = b; mix = c; damp = e
stage2: y = softclip(y, 1.1)
filter1: type = lowpass; cutoff = 10000; resonance = 0.24`,
  {
    a: p("Time", 70, 400, 190),
    b: p("Sustain", 0.12, 0.7, 0.38),
    c: p("Echo", 0.16, 0.58, 0.34),
    d: p("Record", 1.1, 3.4, 1.85),
    e: p("Age", 1600, 4200, 2800),
    tags: ["delay", "tape", "echoplex"],
    outG: 0,
  }
);

add(
  "TC 2290 Grid",
  "Delay",
  "TC 2290-style digital: note-grid time 1/4…1/16, clean 12 kHz damp, 0.35 Hz mod ±3 ms, no tape grit. Dynamic-ish by leaving dry on main.",
  `param a = Time [1/4, 1/16]
param b = Feedback [0.12, 0.62]
param c = Mix [0.16, 0.55]
param d = Mod [0.0, 1.0]
param e = Damp [7000, 14000]
osc1: shape = sine; freq = 0.35; depth = 1.0
delay1: time = a + osc1 * d * 3; feedback = b; mix = c; damp = e
stage1: y = softclip(x, 1.03)`,
  {
    a: p("Time", 0, 1, 0.45),
    b: p("Feedback", 0.12, 0.62, 0.34),
    c: p("Mix", 0.16, 0.55, 0.32),
    d: p("Mod", 0, 1, 0.28),
    e: p("Damp", 7000, 14000, 11000),
    tags: ["delay", "digital", "2290", "grid"],
    outG: 0,
  }
);

add(
  "EMT 140 Plate",
  "Reverb",
  "EMT 140: foil plate, not a hall. Size 0.42–0.58, decay 0.42–0.72 (~1.5–4 s habit), damp 0.18–0.34 (bright), predelay 0–18 ms, width 0.92.",
  `param a = Decay [0.42, 0.72]
param b = Predelay [0, 18]
param c = Damp [0.18, 0.34]
param d = Mix [0.12, 0.55]
param e = Size [0.42, 0.58]
delay1: time = b; feedback = 0.02; mix = 0.28; damp = 9000
reverb1: size = e; decay = a; damp = c; mix = d; width = 0.92
filter1: type = highpass; cutoff = 90; resonance = 0.22
eq1: type = highshelf; freq = 6500; q = 0.7; gain = 1.8
stage1: y = softclip(x, 1.04)`,
  {
    a: p("Decay", 0.42, 0.72, 0.56),
    b: p("Predelay", 0, 18, 6),
    c: p("Damp", 0.18, 0.34, 0.24),
    d: p("Mix", 0.12, 0.55, 0.32),
    e: p("Size", 0.42, 0.58, 0.5),
    tags: ["reverb", "plate", "emt"],
    outG: 0,
  }
);

add(
  "Lexicon 480 Hall",
  "Reverb",
  "Lexicon 480L hall habit: size 0.76–0.94, decay 0.62–0.88, damp 0.38–0.56, predelay 20–80 ms, full width. Even, long, not a plate.",
  `param a = Size [0.76, 0.94]
param b = Decay [0.62, 0.88]
param c = Predelay [20, 80]
param d = Damp [0.38, 0.56]
param e = Mix [0.12, 0.5]
delay1: time = c; feedback = 0.04; mix = 0.3; damp = 8500
reverb1: size = a; decay = b; damp = d; mix = e; width = 1.0
filter1: type = highpass; cutoff = 55; resonance = 0.2
stage1: y = softclip(x, 1.04)`,
  {
    a: p("Size", 0.76, 0.94, 0.84),
    b: p("Decay", 0.62, 0.88, 0.74),
    c: p("Predelay", 20, 80, 36),
    d: p("Damp", 0.38, 0.56, 0.46),
    e: p("Mix", 0.12, 0.5, 0.3),
    tags: ["reverb", "hall", "lexicon"],
    outG: 0,
  }
);

add(
  "AMS RMX Nonlin",
  "Reverb",
  "AMS RMX16 NonLin2 habit: 150–350 ms dense burst. Fixed room (no live size jumps), darker damp, short slap, recovery LPF. 80s snare room, not a hall.",
  `param a = Burst [0.18, 0.34]
param b = Predelay [10, 26]
param c = Damp [0.32, 0.55]
param d = Mix [0.18, 0.62]
delay1: time = b; feedback = 0.02; mix = 0.12; damp = 5500
reverb1: size = 0.36; decay = a; damp = c; mix = d; width = 0.68
eq1: type = highshelf; freq = 3800; q = 0.65; gain = 1.1
filter1: type = highpass; cutoff = 160; resonance = 0.22
filter2: type = lowpass; cutoff = 9500; resonance = 0.22
stage1: y = softclip(x, 1.04)`,
  {
    a: p("Burst", 0.18, 0.34, 0.24),
    b: p("Predelay", 10, 26, 16),
    c: p("Damp", 0.32, 0.55, 0.42),
    d: p("Mix", 0.18, 0.62, 0.4),
    tags: ["reverb", "ams", "nonlinear", "drums"],
    outG: 0,
  }
);

add(
  "Spring Tank",
  "Reverb",
  "Guitar-amp / AKG-ish spring: small tank, splashy top, 22–40 ms drip delay, decay 0.26–0.48, damp 0.16–0.3. Not a plate and not a hall.",
  `param a = Decay [0.26, 0.48]
param b = Drip [22, 40]
param c = Splash [0.16, 0.3]
param d = Mix [0.12, 0.55]
param e = Drive [0.9, 2.2]
stage1: y = tube(x, e)
delay1: time = b; feedback = 0.08; mix = 0.22; damp = 7000
reverb1: size = 0.24; decay = a; damp = c; mix = d; width = 0.45
eq1: type = peak; freq = 2100; q = 1.4; gain = 2.6
filter1: type = highpass; cutoff = 120; resonance = 0.28
stage2: y = softclip(x, 1.08)`,
  {
    a: p("Decay", 0.26, 0.48, 0.36),
    b: p("Drip", 22, 40, 30),
    c: p("Splash", 0.16, 0.3, 0.22),
    d: p("Mix", 0.12, 0.55, 0.3),
    e: p("Drive", 0.9, 2.2, 1.25),
    tags: ["reverb", "spring", "guitar"],
    outG: 0,
  }
);

registerWave2(add, p);

const topologySig = (script, category) => {
  const bits = [category];
  for (const raw of String(script).split(/\r?\n/)) {
    const l = raw.replace(/#.*$/, "").replace(/\/\/.*$/, "").trim();
    if (!l || /^param\s/i.test(l)) continue;
    const type = ((l.match(/^([a-zA-Z_]+)/) || [, ""])[1] || "").replace(/\d+$/, "");
    const clip = (l.match(/tube|diode|softclip|hardclip|fold|bitcrush/g) || []).join("+");
    const notes = (l.match(/\b1\/\d+[t.]?\b/g) || []).join(",");
    const bus = (l.match(/^bus\s+(\w+)/) || [])[1] || "";
    const ir = /ir\d/.test(l) ? "ir" : "";
    const ch = (l.match(/channel\s*=\s*\w+|mode\s*=\s*\w+|type\s*=\s*\w+|pingpong|source\s*=\s*\w+/) || [""])[0];
    const lit = (l.match(/=\s*(?!([a-f])\b)([^;]+)/g) || []).join("");
    bits.push([type, clip, notes, bus, ir, ch, lit].filter(Boolean).join(":"));
  }
  return bits.join("|");
};

// Validate
const names = new Set();
const sigs = new Map();
let idx = 0;
for (const e of list) {
  if (names.has(e.name)) throw new Error("duplicate: " + e.name);
  names.add(e.name);
  const sig = topologySig(e.script, e.category);
  if (sigs.has(sig) && idx >= 191)
    throw new Error("topology clash: " + sigs.get(sig) + " vs " + e.name);
  if (!sigs.has(sig))
    sigs.set(sig, e.name);
  idx++;
  if (!e.script.includes(":")) throw new Error("bad script " + e.name);
  if (/\bparam\s+[gh]\b/.test(e.script))
    throw new Error("dead knob g/h in " + e.name + " (host only binds a–f)");
  for (const letter of ["C", "D", "E", "F"]) {
    const key = "param" + letter;
    if (e[key] && e[key].name === letter && !e.script.includes("param " + letter.toLowerCase()))
      throw new Error("dummy metadata " + key + " on " + e.name);
  }
}

const cats = {};
list.forEach((e) => (cats[e.category] = (cats[e.category] || 0) + 1));
console.log("presets", list.length);
console.log(cats);
if (fs.existsSync("resources/factory_presets.json")) {
  const prev = JSON.parse(fs.readFileSync("resources/factory_presets.json", "utf8"));
  const keep = new Set(list.map((e) => e.name));
  const dropped = (Array.isArray(prev) ? prev : [])
    .filter((e) => e && e.name && ! keep.has(e.name))
    .map((e) => `${e.name} [${e.category}]`);
  if (dropped.length) {
    throw new Error("refusing to drop curated factory presets:\n" + dropped.join("\n"));
  }
}
fs.writeFileSync("resources/factory_presets.json", JSON.stringify(list, null, 2) + "\n");
console.log("wrote resources/factory_presets.json");
