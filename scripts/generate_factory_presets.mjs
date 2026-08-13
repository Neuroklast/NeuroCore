/**
 * Professional factory preset library for NeuroCore.
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
  addIf(/ms\d+\s*:|mode\s*=\s*encode|channel\s*=\s*(mid|side)|ms_encode|ms_decode/,
        "mid-side", "midside", "ms", "mid", "side");
  addIf(/\bbus\b|send:|out:/, "bus", "parallel");
  addIf(/tube/, "tube");
  addIf(/softclip|hardclip/, "clip");
  addIf(/hardclip/, "hardclip");
  addIf(/bitcrush/, "bitcrush", "lo-fi");
  addIf(/fold/, "fold");
  addIf(/diode/, "diode");
  addIf(/comp\d*\s*:/, "compressor");
  addIf(/filter/, "filter");
  addIf(/lowpass/, "lowpass");
  addIf(/highpass/, "highpass");
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
  const header = [`# ${name}: ${description}`];
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
  "Blackface clean: soft single tube, bright sparkle path, open cab 9 kHz — airy, not mid-crush.",
  `param a = Drive [0.5, 2.8]
param b = Bright [0.0, 0.85]
param c = Level [0.55, 1.45]
filter1: type = highpass; cutoff = 45; resonance = 0.22
stage1: y = tube(x, a * 0.5)
stage2: y = softclip(y, 0.75) * c
filter2: type = lowpass; cutoff = 9500; resonance = 0.25
bus bright:
  send: in = 1
  filter3: type = highpass; cutoff = 45; resonance = 0.22
  stage3: y = softclip(x * (1.0 + b * 0.7), 0.9) * c
  filter4: type = lowpass; cutoff = 9500; resonance = 0.25
out: main = 0.6; bright = 0.4`,
  { a: p("Drive", 0.5, 2.8, 1.1), b: p("Bright", 0, 0.85, 0.42), c: p("Level", 0.55, 1.45, 1.1), outG: 0 }
);

add(
  "Marshall Crunch",
  "Distortion",
  "Plexi bark: dual tube, strong mid hump (bandpass), hotter presence — mid-forward crunch.",
  `param a = Drive [1.4, 7.5]
param b = Presence [2000, 7000]
param c = Level [0.38, 1.15]
filter1: type = highpass; cutoff = 100; resonance = 0.38
stage1: y = tube(x, a * 0.6)
stage2: y = tube(y, a * 0.7)
filter2: type = bandpass; center = 750; width = 1200
stage3: y = softclip(y * 1.45, 1.55) * c
filter3: type = lowpass; cutoff = b; resonance = 0.42`,
  { a: p("Drive", 1.4, 7.5, 3.6), b: p("Presence", 2000, 7000, 3800), c: p("Level", 0.38, 1.15, 0.72), outG: 0 }
);

add(
  "Mesa High Gain",
  "Distortion",
  "Rectifier wall: tight HPF, triple tube, hard soft-knee ceiling, dark cab — densest amp.",
  `param a = Gain [3.5, 12.0]
param b = Tight [45, 200]
param c = Level [0.25, 0.9]
filter1: type = highpass; cutoff = b; resonance = 0.6
stage1: y = tube(x, a * 0.5)
stage2: y = tube(y, a * 0.6)
stage3: y = tube(y, a * 0.45)
stage4: y = hardclip(softclip(y, 1.55), 0.68)
filter2: type = lowpass; cutoff = 4500; resonance = 0.58
stage5: y = diode(y, 1.6) * c`,
  { a: p("Gain", 3.5, 12, 7.8), b: p("Tight", 45, 200, 85), c: p("Level", 0.25, 0.9, 0.42), outG: 0 }
);

add(
  "Vox Top Boost",
  "Distortion",
  "AC chime: aggressive bass cut, upper-mid band, glassy softclip — brightest of the amps.",
  `param a = Drive [1.2, 6.5]
param b = Cut [280, 1400]
param c = Level [0.45, 1.25]
filter1: type = highpass; cutoff = b; resonance = 0.5
stage1: y = tube(x, a * 0.85)
filter2: type = bandpass; center = 1800; width = 2200
stage2: y = softclip(y * 1.2, 1.6) * c
filter3: type = lowpass; cutoff = 8200; resonance = 0.32`,
  { a: p("Drive", 1.2, 6.5, 2.9), b: p("Cut", 280, 1400, 520), c: p("Level", 0.45, 1.25, 0.92), outG: 0 }
);

add(
  "Tube Screamer",
  "Distortion",
  "TS808: high HPF into softclip, narrow mid hump, diode polish — classic mid-focused pedal.",
  `param a = Drive [1.2, 11.0]
param b = Tone [350, 4500]
param c = Level [0.28, 1.15]
filter1: type = highpass; cutoff = 780; resonance = 0.28
stage1: y = softclip(x, a)
filter2: type = bandpass; center = 720; width = 420
stage2: y = diode(y * 1.25, 1.35) * c
filter3: type = lowpass; cutoff = b; resonance = 0.48`,
  { a: p("Drive", 1.2, 11, 5.2), b: p("Tone", 350, 4500, 1800), c: p("Level", 0.28, 1.15, 0.7), outG: 0 }
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
filter1: type = highpass; cutoff = 45; resonance = 0.3
stage1: y = hardclip(softclip(x + 0.06, a * 0.4), 0.38)
filter2: type = lowpass; cutoff = b; resonance = 0.7
stage2: y = tube(y, 1.4) * c`,
  { a: p("Fuzz", 3, 16, 9), b: p("Tone", 250, 4500, 1400), c: p("Level", 0.15, 0.85, 0.42), inG: 1.5, outG: 0 }
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
  "Wave Folder",
  "Distortion",
  "West-coast fold â†’ diode recovery â†’ LPF (fold is harsh; always band-limit after).",
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
  "Opto-style sine tremolo. Use Div for host-tempo note rates (1/1..1/32).",
  `param a = Div [0.0, 1.0]
param b = Depth [0.2, 0.85]
param c = Floor [0.2, 0.7]
osc1: shape = sine; sync = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Div", 0, 1, 0.4), b: p("Depth", 0.2, 0.85, 0.5), c: p("Floor", 0.2, 0.7, 0.4), outG: 0 }
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
  "Soft-square trem chopper. Div knob = host tempo note rate (1/1..1/32). Dynamics preserved (floor).",
  `param a = Div [0.0, 1.0]
param b = Depth [0.25, 0.85]
param c = Floor [0.15, 0.55]
osc1: shape = softsquare; sync = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Div", 0, 1, 0.45), b: p("Depth", 0.25, 0.85, 0.55), c: p("Floor", 0.15, 0.55, 0.28), outG: 0 }
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
  "Envelope follower â†’ resonant bandpass + softclip polish + AA LPF (Res on LPF kept moderate).",
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
  "Stable phaser-ish: dual LFO-mod filters (low Q), dry/wet blend — no self-osc hang.",
  `param a = Rate [0.08, 3.0]
param b = Center [300, 1600]
param c = Depth [200, 2200]
param d = Mix [0.25, 0.9]
osc1: shape = sine; freq = a; depth = 1.0
bus wet:
  send: in = 1
  filter1: type = highpass; cutoff = b; + = osc1; * = c; resonance = 0.55
  filter2: type = lowpass; cutoff = b; + = osc1; * = c * 0.7; resonance = 0.45
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
  "Musical LFO lowpass: Min + osc*Depth, restrained Q.",
  `param a = Rate [0.05, 6.0]
param b = Min [100, 700]
param c = Depth [400, 7000]
param d = Res [0.4, 3.0]
osc1: shape = sine; freq = a; depth = 1.0
filter1: type = lowpass; cutoff = b; + = osc1; * = c; resonance = d
stage1: y = diode(x, 1.05)`,
  { a: p("Rate", 0.05, 6, 0.45), b: p("Min", 100, 700, 220), c: p("Depth", 400, 7000, 3200), d: p("Res", 0.4, 3, 1.4), outG: 0 }
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
  "Resonant peak filter â€” Q limited for stability.",
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

// =============================================================================
// GUITAR
// =============================================================================
add(
  "Amp Crunch",
  "Guitar",
  "Edge-of-breakup: mild tube, mid shelf via bandpass, open cab — between clean and Marshall.",
  `param a = Drive [0.9, 5.5]
param b = Tone [900, 7000]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 70; resonance = 0.3
stage1: y = tube(x, a * 0.7)
filter2: type = bandpass; center = 1100; width = 2800
stage2: y = softclip(y, 1.05) * c
filter3: type = lowpass; cutoff = b; resonance = 0.38`,
  { a: p("Drive", 0.9, 5.5, 2.2), b: p("Tone", 900, 7000, 3600), c: p("Level", 0.4, 1.2, 0.88), outG: 0 }
);

add(
  "Metal Gate",
  "Guitar",
  "Tight metal: HPF â†’ tube cascade â†’ soft-pre hard knee â†’ cab LPF (filter after clip).",
  `param a = Drive [4.0, 14.0]
param b = LowCut [70, 350]
param c = Level [0.2, 0.85]
filter1: type = highpass; cutoff = b; resonance = 0.5
stage1: y = tube(x, a * 0.45)
stage2: y = hardclip(softclip(tube(y, a * 0.5), 1.1), 0.5)
filter2: type = lowpass; cutoff = 4800; resonance = 0.6
stage3: y = y * c`,
  { a: p("Drive", 4, 14, 8.5), b: p("LowCut", 70, 350, 140), c: p("Level", 0.2, 0.85, 0.45), outG: 0 }
);

add(
  "Blues Breakup",
  "Guitar",
  "Touch-sensitive blues OD with bias.",
  `param a = Drive [1.0, 5.5]
param b = Bias [0.0, 0.22]
param c = Level [0.5, 1.25]
stage1: y = tube(x + b * 0.5, a) * c
filter1: type = lowpass; cutoff = 5500; resonance = 0.4`,
  { a: p("Drive", 1, 5.5, 2.5), b: p("Bias", 0, 0.22, 0.07), c: p("Level", 0.5, 1.25, 0.95), outG: 0 }
);

add(
  "Lead Boost",
  "Guitar",
  "Lead boost into saturated power stage + presence.",
  `param a = Boost [1.5, 7.0]
param b = Presence [1200, 7000]
param c = Level [0.4, 1.15]
filter1: type = highpass; cutoff = 100; resonance = 0.35
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.7
stage2: y = diode(y, 1.35) * c`,
  { a: p("Boost", 1.5, 7, 3.8), b: p("Presence", 1200, 7000, 3400), c: p("Level", 0.4, 1.15, 0.8), outG: 0 }
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
  "Compress then tube â€” thick DI bass.",
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
delay1: time = a + osc1 * 4; feedback = 0.08; mix = 1.0; damp = 7000
stage2: y = lerp(x, y, b)
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
  "Drum bus: tube â†’ soft-pre hard ceiling â†’ glue comp â†’ diode + mild LPF.",
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
  "Snare crack: HPF â†’ soft-pre hard ceiling â†’ body LPF (filter after clip).",
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
  "Cymbal sizzle: steep HPF + softclip (drive capped; soft only â€” no hardclip on air).",
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
  "Lead: tube â†’ restrained LPF â†’ softclip ceiling (clip after filter = less alias).",
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
  "HF excite: HPF â†’ softclip (low drive) â†’ blend â€” soft only, no hardclip on air.",
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
  "Peak clipper best-practice: softclip â†’ soft-knee hard ceiling â†’ gentle AA LPF.",
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
  "Sine ducking envelope (sidechain pump).",
  `param a = Rate [0.5, 8.0]
param b = Depth [0.4, 1.0]
param c = Floor [0.0, 0.35]
osc1: shape = sine; freq = a; depth = 1.0
stage1: y = x * (c + (1.0 - c) * (1.0 - b + b * (0.5 + 0.5 * osc1)))`,
  { a: p("Rate", 0.5, 8, 2), b: p("Depth", 0.4, 1, 0.85), c: p("Floor", 0, 0.35, 0.08), outG: 0 }
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
  "Club loudness: softclip â†’ hard ceiling â†’ AA LPF (same recipe as Loudness Clip, lower ceiling).",
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
reverb1: size = b; decay = 0.78; damp = f; mix = 1.0; width = 0.95
stage2: y = y * (1.0 - e + e * (0.5 + 0.5 * osc1))
filter1: type = highpass; cutoff = 1800; resonance = 0.25
filter2: type = lowpass; cutoff = 12000; resonance = 0.25
stage3: y = lerp(x, y, c)`,
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
  "Air crystal: HPF â†’ softclip blend (soft only) + mild AA LPF.",
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
stage1: channel = left; y = tube(x, c) * (1.0 - b + b * (0.5 + 0.5 * osc1))
stage2: channel = right; y = tube(x, c) * (1.0 - b + b * (0.5 - 0.5 * osc1))`,
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
  "Controlled feedback (fb capped) â†’ LPF â†’ softclip ceiling (filter before/around clip).",
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
  "Deep fold â†’ softclip recovery â†’ lower LPF (fold is HF-heavy; band-limit hard).",
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
ms1: mode = encode
stage1: channel = mid; y = x * b
stage2: channel = side; y = x * a * b
ms2: mode = decode`,
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
ms1: mode = encode
stage1: channel = mid; y = x
comp1: threshold = a; ratio = b; attack = 0.012; release = 0.16
stage2: channel = mid; y = softclip(x * d, 1.08)
stage3: channel = side; y = softclip(x * c, 1.05)
ms2: mode = decode`,
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
  "Safety peak clip: softclip â†’ hard ceiling â†’ high AA LPF (never bare clamp).",
  `param a = Ceiling [0.5, 1.0]
param b = Drive [1.0, 1.8]
stage1: y = hardclip(softclip(x, b), a)
filter1: type = lowpass; cutoff = 16000; resonance = 0.2`,
  { a: p("Ceiling", 0.5, 1, 0.94), b: p("Drive", 1, 1.8, 1.08), outG: 0 }
);

add(
  "Soft Clip Tone",
  "Utility",
  "Canonical softclip recipe: HPF â†’ softclip(drive) â†’ tone LPF â†’ level.",
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
  "Transparent peaks: lerp(dry, softclip) â€” best when you need control without dirt.",
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
  "Diode soft-knee (asinh) + optional soft ceiling + LPF â€” smooth analog-ish clip.",
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
  "Full strip: HPF → soft drive → mid EQ → air LPF → glue comp → soft ceiling.",
  `param a = Drive [0.8, 3.5]
param b = LowCut [30, 180]
param c = Mid [400, 3500]
param d = MidGain [0.45, 2.0]
param e = High [4000, 14000]
param f = Thresh [-28.0, -6.0]
filter1: type = highpass; cutoff = b; resonance = 0.32
stage1: y = softclip(x, a)
filter2: type = bandpass; center = c; width = c * 0.5
stage2: y = softclip(x * d, 1.12)
filter3: type = lowpass; cutoff = e; resonance = 0.28
comp1: threshold = f; ratio = 3.0; attack = 0.008; release = 0.15
stage3: y = softclip(x * 1.0, 1.08)`,
  {
    a: p("Drive", 0.8, 3.5, 1.35),
    b: p("LowCut", 30, 180, 55),
    c: p("Mid", 400, 3500, 1200),
    d: p("MidGain", 0.45, 2.0, 1.1),
    e: p("High", 4000, 14000, 9000),
    f: p("Thresh", -28, -6, -16),
    outG: 0,
  }
);

add(
  "Dual Amp Stack",
  "Distortion",
  "Parallel clean tube + hot tube, mid scoop, cab LPF, presence, level, blend — six-knob dual path.",
  `param a = Clean [0.6, 2.5]
param b = Hot [2.0, 9.0]
param c = Blend [0.2, 0.9]
param d = Scoop [0.3, 1.4]
param e = Cab [2500, 8000]
param f = Level [0.35, 1.15]
bus clean:
  send: in = 1
  filter1: type = highpass; cutoff = 70; resonance = 0.35
  stage1: y = tube(x, a)
  filter2: type = bandpass; center = 900; width = 2200
  stage2: y = softclip(x * d, 1.2)
  filter3: type = lowpass; cutoff = e; resonance = 0.4
  stage3: y = softclip(y, 1.1) * f
bus hot:
  send: in = 1
  filter4: type = highpass; cutoff = 70; resonance = 0.35
  stage4: y = tube(x, b)
  filter5: type = bandpass; center = 900; width = 2200
  stage5: y = softclip(x * d, 1.2)
  filter6: type = lowpass; cutoff = e; resonance = 0.4
  stage6: y = softclip(y, 1.1) * f
out: clean = 1-c; hot = c`,
  {
    a: p("Clean", 0.6, 2.5, 1.2),
    b: p("Hot", 2, 9, 4.5),
    c: p("Blend", 0.2, 0.9, 0.55),
    d: p("Scoop", 0.3, 1.4, 0.85),
    e: p("Cab", 2500, 8000, 4800),
    f: p("Level", 0.35, 1.15, 0.78),
    outG: 0,
  }
);

add(
  "Rhythmic Gate Delay",
  "Delay",
  "Tempo-synced delay + envelope duck + feedback damp + drive on repeats. Complex send FX.",
  `param a = Time [80, 480]
param b = Feedback [0.15, 0.88]
param c = Mix [0.2, 0.75]
param d = Damp [800, 9000]
param e = Duck [0.0, 0.9]
param f = Drive [0.8, 4.0]
env1: type = peak; attack = 0.008; release = 0.12
delay1: time = a; feedback = b; mix = 1.0; damp = d
stage1: y = softclip(x * (1.0 + f * 0.35), 1.15)
stage2: y = lerp(x, y, c * (1.0 - env1 * e))`,
  {
    a: p("Time", 80, 480, 220),
    b: p("Feedback", 0.15, 0.88, 0.48),
    c: p("Mix", 0.2, 0.75, 0.42),
    d: p("Damp", 800, 9000, 4200),
    e: p("Duck", 0, 0.9, 0.45),
    f: p("Drive", 0.8, 4, 1.6),
    outG: 0,
  }
);

add(
  "Cinematic Space",
  "Reverb",
  "Pre-delay → reverb → MS width + air LPF + soft tail ceiling.",
  `param a = Predelay [20, 180]
param b = Size [0.25, 0.95]
param c = Decay [0.35, 0.92]
param d = Damp [0.15, 0.85]
param e = Mix [0.2, 0.85]
param f = Width [0.4, 1.6]
delay1: time = a; feedback = 0.05; mix = 0.35; damp = 8000
reverb1: size = b; decay = c; damp = d; mix = e; width = 0.95
filter1: type = lowpass; cutoff = 8000; resonance = 0.25
ms1: mode = encode
stage1: channel = mid; y = x
stage2: channel = side; y = x * f
ms2: mode = decode
stage3: y = softclip(x, 1.08)`,
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
  "LFO-modulated multi-notch cascade with feedback, depth, rate, center, and wet. Advanced swirl.",
  `param a = Rate [0.05, 6.0]
param b = Depth [200, 2500]
param c = Center [400, 4000]
param d = Feedback [0.05, 0.55]
param e = Mix [0.25, 0.95]
param f = Resonance [0.4, 2.2]
osc1: type = sine; freq = a
stage1: y = x
bus swirl:
  send: main = 1
  filter1: type = bandpass; center = c + osc1 * b; width = 600; resonance = f
  stage2: y = softclip(x + y_prev * d, 1.2)
  filter2: type = lowpass; cutoff = 12000; resonance = 0.28
out: main = 1-e; swirl = e`,
  {
    a: p("Rate", 0.05, 6, 0.35),
    b: p("Depth", 200, 2500, 900),
    c: p("Center", 400, 4000, 1200),
    d: p("Feedback", 0.05, 0.55, 0.28),
    e: p("Mix", 0.25, 0.95, 0.7),
    f: p("Resonance", 0.4, 2.2, 1.1),
    outG: 0,
  }
);

add(
  "Vocal Chain Pro",
  "Vocals",
  "Broadcast vocal: de-mud HPF, mild tube, presence BPF, comp, air, de-ess LPF, level.",
  `param a = LowCut [60, 220]
param b = Drive [0.9, 3.5]
param c = Presence [1800, 5500]
param d = Amount [0.2, 0.85]
param e = Thresh [-24.0, -8.0]
param f = Air [5000, 14000]
filter1: type = highpass; cutoff = a; resonance = 0.35
stage1: y = tube(x, b)
filter2: type = bandpass; center = c; width = 1400
stage2: y = lerp(x, softclip(x, 1.25), d)
comp1: threshold = e; ratio = 3.5; attack = 0.006; release = 0.12
filter3: type = lowpass; cutoff = f; resonance = 0.28
filter4: type = lowpass; cutoff = 7000; resonance = 0.45
stage3: y = softclip(x * 0.95, 1.08)`,
  {
    a: p("LowCut", 60, 220, 110),
    b: p("Drive", 0.9, 3.5, 1.6),
    c: p("Presence", 1800, 5500, 3200),
    d: p("Amount", 0.2, 0.85, 0.48),
    e: p("Thresh", -24, -8, -14),
    f: p("Air", 5000, 14000, 10000),
    outG: 0,
  }
);

add(
  "Bass Architect",
  "Bass",
  "Sub-safe: HPF, parallel grit, mid focus, comp, low LPF, level. Six knobs for DI design.",
  `param a = HPF [25, 90]
param b = Drive [1.2, 6.0]
param c = Blend [0.2, 0.8]
param d = Mid [200, 1200]
param e = Thresh [-22.0, -8.0]
param f = Level [0.5, 1.3]
filter1: type = highpass; cutoff = a; resonance = 0.25
stage1: y = lerp(x, tube(x, b), c)
filter2: type = bandpass; center = d; width = d * 0.8
stage2: y = softclip(x * 1.15, 1.1)
comp1: threshold = e; ratio = 4.0; attack = 0.015; release = 0.22
filter3: type = lowpass; cutoff = 6500; resonance = 0.3
stage3: y = softclip(x * f, 1.08)`,
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
  "Bitcrush + fold + delay ping-pong + LFO LPF + mix. Band-limited; keep OS at 2x.",
  `param a = Bits [4.0, 12.0]
param b = Fold [0.2, 0.7]
param c = Time [40, 320]
param d = Feedback [0.1, 0.65]
param e = Rate [0.1, 4.0]
param f = Cutoff [600, 8000]
osc1: shape = sine; freq = e
stage1: y = bitcrush(softclip(x, 1.35), a)
stage2: y = fold(y, -b, b)
delay1: time = c; feedback = d; mix = 0.45; damp = 5000; pingpong = true
filter1: type = lowpass; cutoff = f; + = osc1; * = 0.22; resonance = 0.4
stage3: y = lerp(x, softclip(y, 1.08), 0.85)`,
  {
    a: p("Bits", 4, 12, 8),
    b: p("Fold", 0.2, 0.7, 0.38),
    c: p("Time", 40, 320, 140),
    d: p("Feedback", 0.1, 0.65, 0.35),
    e: p("Rate", 0.1, 4, 0.8),
    f: p("Cutoff", 600, 8000, 3200),
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
  "MS: mid HPF + side HPF, then a parallel presence bus (bandpass mid only) blended back. Does not replace a de-esser.",
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
  filter3: type = bandpass; center = a; width = 1400; channel = mid
  stage3: channel = mid; y = softclip(x, 1.18)
  stage4: channel = side; y = x * 0.0
  ms4: mode = decode
  filter4: type = lowpass; cutoff = 12000; resonance = 0.22
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
  "Loudness Curve",
  "Psychoacoustic",
  "Rough equal-loudness habit: keep the body, add a 3 kHz presence bus. Not K-weighting, not a loudness meter.",
  `param a = Contour [0.12, 0.65]
param b = Presence [2200, 4500]
param c = Air [7000, 14000]
param d = Rumble [35, 100]
param e = Level [0.7, 1.2]
filter1: type = highpass; cutoff = d; resonance = 0.24
stage1: y = x * e
bus curve:
  send: in = 1
  filter2: type = bandpass; center = b; width = 1600
  stage2: y = softclip(x, 1.1)
  filter3: type = lowpass; cutoff = c; resonance = 0.22
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
delay1: time = a; feedback = 0.04; mix = 0.28; damp = 6500
reverb1: size = b; decay = c; damp = e; mix = d; width = 0.92
ms1: mode = encode
stage1: channel = mid; y = x
stage2: channel = side; y = x * f
ms2: mode = decode
filter1: type = lowpass; cutoff = 9200; resonance = 0.24
stage3: y = softclip(x, 1.05)`,
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
delay1: time = b; feedback = 0.05; mix = 0.38; damp = 5000
reverb1: size = c; decay = 0.72; damp = e; mix = d; width = 0.88
filter2: type = lowpass; cutoff = 6800; resonance = 0.24
stage1: y = softclip(x * f, 1.05)`,
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

// Validate
const names = new Set();
for (const e of list) {
  if (names.has(e.name)) throw new Error("duplicate: " + e.name);
  names.add(e.name);
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
fs.writeFileSync("resources/factory_presets.json", JSON.stringify(list, null, 2) + "\n");
console.log("wrote resources/factory_presets.json");
