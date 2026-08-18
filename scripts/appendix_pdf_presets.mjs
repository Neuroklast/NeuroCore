/**
 * Append the 12 chains from feedback/NeuroCore DSL Audio Preset Generierung.pdf
 * into resources/factory_presets.json. Never rewrite the catalog from the generator.
 */
import fs from "fs";

const path = "resources/factory_presets.json";
const list = JSON.parse(fs.readFileSync(path, "utf8"));
const have = new Set(list.map((p) => p.name));

const p = (name, min, max, def) => ({ name, min, max, default: def });

function inferTags(script, name, description, category, extra = []) {
  const blob = `${script}\n${name}\n${description}\n${category}`.toLowerCase();
  const tags = new Set((extra || []).map((t) => String(t).toLowerCase().trim()).filter(Boolean));
  if (category) tags.add(String(category).toLowerCase());
  const addIf = (re, ...ts) => {
    if (re.test(blob)) ts.forEach((t) => tags.add(t));
  };
  addIf(/delay/, "delay");
  addIf(/reverb|verb\d+\s*:/, "reverb", "space");
  addIf(/ms\d+\s*:|mode\s*=\s*encode|channel\s*=\s*(mid|side)/, "mid-side", "ms");
  addIf(/\bbus\b|send:|out:/, "bus", "parallel");
  addIf(/tube/, "tube");
  addIf(/softclip|hardclip/, "clip");
  addIf(/hardclip/, "hardclip");
  addIf(/bitcrush/, "bitcrush", "lo-fi");
  addIf(/fold/, "fold");
  addIf(/diode/, "diode");
  addIf(/comp\d*\s*:/, "compressor");
  addIf(/ott\d*\s*:/, "ott", "compressor", "multiband");
  addIf(/widen\d*\s*:/, "widen", "stereo", "width");
  addIf(/gate\d*\s*:/, "gate");
  addIf(/limit\d*\s*:/, "limiter");
  addIf(/ir\d*\s*:/, "ir", "cabinet");
  addIf(/filter/, "filter");
  addIf(/eq\d*\s*:/, "eq", "equalizer");
  addIf(/lowpass/, "lowpass");
  addIf(/highpass/, "highpass");
  addIf(/octaver\d*\s*:/, "octaver", "pitch");
  addIf(/osc\d*\s*:/, "modulation", "lfo");
  addIf(/env\d*\s*:/, "envelope");
  addIf(/xover/, "split", "multiband");
  for (const w of ["kick", "techno", "hardcore", "rumble", "guitar", "amp", "master", "cinematic", "shimmer", "transient"]) {
    if (new RegExp(`(^|[^a-z0-9])${w}([^a-z0-9]|$)`, "i").test(blob)) tags.add(w);
  }
  return [...tags].sort();
}

function annotate(name, description, script, knobs) {
  const header = [`# ${name}`, `# ${description}`];
  const letters = "abcdef";
  letters.split("").forEach((L, i) => {
    const k = knobs[L];
    if (k) header.push(`# ${L} ${k.name}: ${k.min} to ${k.max}, default ${k.default}`);
  });
  return `${header.join("\n")}\n\n${script.trim()}\n`;
}

function preset(name, category, description, script, knobs, extraTags = []) {
  const rec = {
    name,
    category,
    description,
    inputGain: 0,
    outputGain: 0,
    mix: 1,
    script: annotate(name, description, script, knobs),
    tags: inferTags(script, name, description, category, extraTags),
  };
  const map = { a: "paramA", b: "paramB", c: "paramC", d: "paramD", e: "paramE", f: "paramF" };
  for (const [L, key] of Object.entries(map)) {
    if (knobs[L]) rec[key] = knobs[L];
  }
  return rec;
}

const added = [
  preset(
    "Offbeat Gallop",
    "Club",
    "Hard techno rumble: punchy dry kick, dotted-16th dark delay plus hall on a low bus, tube density, env-ducked so the click stays clean. Insert on the kick.",
    `param a = Punch [0.4, 2.2]
param b = Rumble [0.25, 1.1]
param c = Cut [70, 280]
param d = Drive [2.0, 12.0]
param e = Duck [0.35, 1.0]
param f = Level [0.85, 1.35]
env1: type = peak; attack = 0.001; release = 0.075
filter1: type = highpass; cutoff = 28; resonance = 0.18
eq1: type = lowshelf; freq = 60; q = 0.7; gain = 2.4
eq2: type = peak; freq = 3200; q = 1.15; gain = 3.8
eq3: type = peak; freq = 320; q = 1.1; gain = -3.2
stage1: y = hardclip(softclip(x * f * (0.82 + 0.22 * a * env1), 1.12), 0.72)
filter2: type = lowpass; cutoff = 11000; resonance = 0.2
bus rumble:
  send: in = 1
  filter3: type = lowpass; cutoff = c; resonance = 0.12
  delay1: sync = 1/16.; feedback = 0.42; mix = 0.7; damp = 1800
  reverb1: size = 0.72; decay = 0.55; damp = 0.62; mix = 0.55; width = 0.35
  stage2: y = tube(x, d * 0.22)
  filter4: type = lowpass; cutoff = 130; resonance = 0.22
  stage3: y = x * (1 - e * env1)
  filter5: type = lowpass; cutoff = 160; resonance = 0.18
out: main = 0.95; rumble = b`,
    {
      a: p("Punch", 0.4, 2.2, 1.15),
      b: p("Rumble", 0.25, 1.1, 0.68),
      c: p("Cut", 70, 280, 140),
      d: p("Drive", 2, 12, 7.2),
      e: p("Duck", 0.35, 1, 0.78),
      f: p("Level", 0.85, 1.35, 1.08),
    },
    ["techno", "kick", "rumble", "club"],
  ),
  preset(
    "Schranz Multiband",
    "Club",
    "Hardcore kick split: tape-ish sub, 8-bit mid bark into diode, folded click on top, then a brick. Insert on the kick.",
    `param a = Drive [4.0, 16.0]
param b = Bits [5.0, 11.0]
param c = Fold [0.16, 0.55]
param d = Split [90, 220]
param e = Air [1600, 4200]
param f = Level [0.9, 1.4]
env1: type = peak; attack = 0.0006; release = 0.06
eq1: type = lowshelf; freq = 58; q = 0.7; gain = 3.2
eq2: type = peak; freq = 5000; q = 1.35; gain = 4.6
xover1: f1 = d; f2 = e
bus low:
  stage1: y = tube(x, a * 0.16)
  ms1: mode = encode
  stage2: channel = side; y = x * 0
  ms2: mode = decode
  filter1: type = lowpass; cutoff = 180; resonance = 0.2
bus mid:
  stage3: y = bitcrush(softclip(x, a * 0.18), b)
  stage4: y = diode(y, 1.55)
  filter2: type = bandpass; center = 820; width = 520
  filter3: type = lowpass; cutoff = 6500; resonance = 0.28
bus high:
  stage5: y = fold(x * (0.7 + 0.4 * env1), -c, c)
  filter4: type = highpass; cutoff = e; resonance = 0.28
  filter5: type = lowpass; cutoff = 12000; resonance = 0.2
stage6: y = hardclip(softclip(x * f, 1.18), 0.28)
filter6: type = lowpass; cutoff = 12500; resonance = 0.2
out: low = 0.92; mid = 0.88; high = 0.7`,
    {
      a: p("Drive", 4, 16, 11.4),
      b: p("Bits", 5, 11, 8),
      c: p("Fold", 0.16, 0.55, 0.32),
      d: p("Split", 90, 220, 150),
      e: p("Air", 1600, 4200, 2500),
      f: p("Level", 0.9, 1.4, 1.16),
    },
    ["hardcore", "kick", "club", "bitcrush"],
  ),
  preset(
    "Koren Stack Cab",
    "Guitar",
    "Triode pre into a British mid stack, pentode-ish second tube, cab IR, then a dark shelf. Insert on a DI guitar.",
    `param a = Gain [2.5, 12.0]
param b = Bass [2.0, 8.0]
param c = Mid [1.0, 8.0]
param d = Treble [2.0, 9.0]
param e = Presence [2000, 7000]
param f = Level [0.28, 0.95]
gate1: threshold = -48; hyst = 7; hold = 0.04; range = -78
filter1: type = highpass; cutoff = 78; resonance = 0.32
stage1: y = tube(x, a * 0.42)
eq1: type = lowshelf; freq = 110; q = 0.7; gain = b
eq2: type = peak; freq = 780; q = 1.05; gain = c
eq3: type = peak; freq = e; q = 0.85; gain = d * 0.45
stage2: y = tube(y, a * 0.38)
stage3: y = hardclip(softclip(y, 1.35), 0.62)
filter2: type = lowpass; cutoff = 5200; resonance = 0.42
ir1: mix = 0.48; gain = 0
eq4: type = highshelf; freq = 12000; q = 0.7; gain = -2.4
stage4: y = diode(y, 1.15) * f
limit1: ceiling = -0.4; release = 0.08`,
    {
      a: p("Gain", 2.5, 12, 7.2),
      b: p("Bass", 2, 8, 5.2),
      c: p("Mid", 1, 8, 4.4),
      d: p("Treble", 2, 9, 6.5),
      e: p("Presence", 2000, 7000, 4200),
      f: p("Level", 0.28, 0.95, 0.52),
    },
    ["guitar", "amp", "tube", "ir"],
  ),
  preset(
    "Streaming Ceiling",
    "Mastering",
    "Mixbus density: 25 Hz cut, 60 Hz boost with a 220 Hz scoop, VCA glue, 3 kHz presence, then a soft-knee ceiling. Insert on the mix.",
    `param a = Boost [1.0, 6.0]
param b = Glue [-22.0, -6.0]
param c = Air [1.0, 5.0]
param d = Drive [0.8, 2.4]
param e = Ceiling [0.55, 0.92]
param f = Level [0.7, 1.15]
filter1: type = highpass; cutoff = 26; resonance = 0.18
eq1: type = lowshelf; freq = 60; q = 0.7; gain = a
eq2: type = peak; freq = 210; q = 0.95; gain = -a * 0.55
eq3: type = highshelf; freq = 14000; q = 0.65; gain = c * 0.55
comp1: threshold = b; ratio = 4; attack = 0.03; release = 0.22; knee = 6; makeup = 2; hpf = 130
eq4: type = peak; freq = 3200; q = 1.05; gain = 1.6
stage1: y = tube(x, d * 0.55)
stage2: y = hardclip(softclip(x * f, 1.08), e)
filter2: type = lowpass; cutoff = 18000; resonance = 0.18
limit1: ceiling = -1.0; release = 0.05`,
    {
      a: p("Boost", 1, 6, 3.4),
      b: p("Glue", -22, -6, -12),
      c: p("Air", 1, 5, 3.2),
      d: p("Drive", 0.8, 2.4, 1.35),
      e: p("Ceiling", 0.55, 0.92, 0.78),
      f: p("Level", 0.7, 1.15, 0.95),
    },
    ["master", "glue", "limiter"],
  ),
  preset(
    "FET All In",
    "Dynamics",
    "FET smash: 20:1-ish, hair-trigger attack, short release, tube grit. Drums, vocals, whole buses.",
    `param a = Drive [6.0, 24.0]
param b = Attack [0.001, 0.008]
param c = Release [0.04, 0.22]
param d = Output [-8.0, 4.0]
param e = Color [0.8, 2.2]
param f = Mix [0.45, 1.0]
filter1: type = highpass; cutoff = 40; resonance = 0.16
comp1: threshold = -a; ratio = 18; attack = b; release = c; knee = 0; makeup = 6
stage1: y = tube(x, e)
stage2: y = lerp(x, y, f)
eq1: type = peak; freq = 80; q = 0.7; gain = d * 0.15
filter2: type = lowpass; cutoff = 16000; resonance = 0.16
stage3: y = hardclip(softclip(y, 1.1), 0.88)`,
    {
      a: p("Drive", 6, 24, 16),
      b: p("Attack", 0.001, 0.008, 0.0015),
      c: p("Release", 0.04, 0.22, 0.055),
      d: p("Output", -8, 4, -2),
      e: p("Color", 0.8, 2.2, 1.35),
      f: p("Mix", 0.45, 1, 0.92),
    },
    ["compressor", "drum", "vocal"],
  ),
  preset(
    "Precision Multiband",
    "Dynamics",
    "Three-band control: pinned low, lifted mid, tamed air. Insert on a mix or bus.",
    `param a = Low [-22.0, -8.0]
param b = Mid [-32.0, -14.0]
param c = Air [-20.0, -8.0]
param d = Split [80, 200]
param e = Top [3500, 8000]
param f = Level [0.75, 1.2]
xover1: f1 = d; f2 = e
bus low:
  comp1: threshold = a; ratio = 5.5; attack = 0.012; release = 0.16; makeup = 2; hpf = 0
  filter1: type = lowpass; cutoff = 180; resonance = 0.16
bus mid:
  ott1: depth = 0.28; time = 0.02; in = 1
  comp2: threshold = b; ratio = 2.4; attack = 0.03; release = 0.09; makeup = 1.5
bus high:
  comp3: threshold = c; ratio = 4; attack = 0.004; release = 0.035; makeup = 0.5
  eq1: type = highshelf; freq = 12000; q = 0.55; gain = 1.2
stage1: y = softclip(x * f, 1.06)
filter2: type = lowpass; cutoff = 17500; resonance = 0.16
out: low = 1; mid = 1; high = 1`,
    {
      a: p("Low", -22, -8, -16),
      b: p("Mid", -32, -14, -24),
      c: p("Air", -20, -8, -14),
      d: p("Split", 80, 200, 120),
      e: p("Top", 3500, 8000, 6000),
      f: p("Level", 0.75, 1.2, 0.98),
    },
    ["multiband", "ott", "master"],
  ),
  preset(
    "British Desk EQ",
    "Mastering",
    "Channel desk: 50 Hz cut, 110 Hz shelf, proportional mid bell, fixed 12 kHz air, light transformer tube.",
    `param a = Pre [0.0, 8.0]
param b = Low [ -4.0, 6.0]
param c = MidHz [360, 7200]
param d = Mid [ -6.0, 6.0]
param e = Air [ -2.0, 6.0]
param f = Level [0.7, 1.2]
filter1: type = highpass; cutoff = 50; resonance = 0.22
stage1: y = tube(x, 1.05 + a * 0.08)
eq1: type = lowshelf; freq = 110; q = 0.7; gain = b
eq2: type = peak; freq = c; q = 0.7 + abs(d) * 0.12; gain = d
eq3: type = highshelf; freq = 12000; q = 0.7; gain = e
stage2: y = softclip(x * f, 1.06)
filter2: type = lowpass; cutoff = 18000; resonance = 0.16`,
    {
      a: p("Pre", 0, 8, 3.2),
      b: p("Low", -4, 6, 2.4),
      c: p("MidHz", 360, 7200, 1600),
      d: p("Mid", -6, 6, -1.6),
      e: p("Air", -2, 6, 3.2),
      f: p("Level", 0.7, 1.2, 0.98),
    },
    ["eq", "desk", "master"],
  ),
  preset(
    "Passive Low Trick",
    "Mastering",
    "Passive low trick: boost and cut at 30-60 Hz so the fundamental rises and 200 Hz scoops. Tube makeup, broad air.",
    `param a = Boost [2.0, 8.0]
param b = Cut [1.0, 6.0]
param c = Freq [28, 80]
param d = Air [0.0, 7.0]
param e = Tube [0.8, 2.0]
param f = Level [0.75, 1.2]
eq1: type = lowshelf; freq = c; q = 0.65; gain = a
eq2: type = peak; freq = c * 3.4; q = 0.85; gain = -b
eq3: type = highshelf; freq = 10000; q = 0.55; gain = d
stage1: y = tube(x, e)
stage2: y = softclip(x * f, 1.05)
filter1: type = lowpass; cutoff = 17500; resonance = 0.16`,
    {
      a: p("Boost", 2, 8, 4.6),
      b: p("Cut", 1, 6, 2.8),
      c: p("Freq", 28, 80, 42),
      d: p("Air", 0, 7, 4.2),
      e: p("Tube", 0.8, 2, 1.2),
      f: p("Level", 0.75, 1.2, 0.98),
    },
    ["eq", "kick", "master"],
  ),
  preset(
    "Ladder Sweep",
    "Filter",
    "Near-oscillation ladder: LFO walks a 4-pole-ish LPF from a floor up, diode polish. For pads, leads, risers.",
    `param a = Rate [0.04, 2.5]
param b = Floor [80, 600]
param c = Span [400, 6000]
param d = Res [0.6, 3.1]
param e = Drive [0.9, 2.6]
param f = Level [0.7, 1.2]
osc1: shape = sine; freq = a; depth = 1
filter1: type = lowpass; cutoff = b + (0.5 + 0.5 * osc1) * c; resonance = d
stage1: y = diode(x, e)
filter2: type = lowpass; cutoff = 14000; resonance = 0.2
stage2: y = softclip(y * f, 1.08)`,
    {
      a: p("Rate", 0.04, 2.5, 0.18),
      b: p("Floor", 80, 600, 160),
      c: p("Span", 400, 6000, 2400),
      d: p("Res", 0.6, 3.1, 2.4),
      e: p("Drive", 0.9, 2.6, 1.45),
      f: p("Level", 0.7, 1.2, 0.96),
    },
    ["filter", "modulation", "synth"],
  ),
  preset(
    "Octave Cloud",
    "Delay",
    "Shimmer: 500 ms delay, octave-up in the wet, damped hall. Dry stays put.",
    `param a = Time [280, 720]
param b = Feedback [0.25, 0.72]
param c = Octave [0.25, 0.9]
param d = Size [0.45, 0.95]
param e = Mix [0.18, 0.7]
param f = Damp [2500, 9000]
stage1: y = x
bus cloud:
  send: in = 1
  delay1: time = a; feedback = b; mix = 1; damp = f
  octaver1: mix = c; sub = 0; up = 1; tone = 2400; thresh = 0.04
  filter1: type = lowpass; cutoff = 8000; resonance = 0.22
  reverb1: size = d; decay = 0.72; damp = 0.42; mix = 0.7; width = 0.95
  filter2: type = highpass; cutoff = 180; resonance = 0.16
out: main = 1; cloud = e`,
    {
      a: p("Time", 280, 720, 500),
      b: p("Feedback", 0.25, 0.72, 0.48),
      c: p("Octave", 0.25, 0.9, 0.62),
      d: p("Size", 0.45, 0.95, 0.78),
      e: p("Mix", 0.18, 0.7, 0.38),
      f: p("Damp", 2500, 9000, 6200),
    },
    ["shimmer", "delay", "reverb", "octaver"],
  ),
  preset(
    "Blackwall Space",
    "Cinematic",
    "Cyberpunk wreck: fold, slow bandpass sweep, huge hall, opto-ish pump. For vocals, synths, field recordings.",
    `param a = Fold [0.18, 0.62]
param b = Rate [0.04, 0.4]
param c = Size [0.55, 0.98]
param d = Pump [-36.0, -14.0]
param e = Mix [0.35, 0.92]
param f = Level [0.55, 1.15]
osc1: shape = sine; freq = b; depth = 1
filter1: type = highpass; cutoff = 70; resonance = 0.22
stage1: y = fold(softclip(x, 1.4), -a, a)
filter2: type = bandpass; center = 400 + (0.5 + 0.5 * osc1) * 3200; width = 900
reverb1: size = c; decay = 0.88; damp = 0.38; mix = 0.82; width = 1
comp1: threshold = d; ratio = 8; attack = 0.05; release = 0.45; knee = 8; makeup = 3
stage2: y = lerp(x, y, e) * f
filter3: type = lowpass; cutoff = 12000; resonance = 0.2`,
    {
      a: p("Fold", 0.18, 0.62, 0.36),
      b: p("Rate", 0.04, 0.4, 0.1),
      c: p("Size", 0.55, 0.98, 0.86),
      d: p("Pump", -36, -14, -26),
      e: p("Mix", 0.35, 0.92, 0.7),
      f: p("Level", 0.55, 1.15, 0.88),
    },
    ["cinematic", "cyberpunk", "fold"],
  ),
  preset(
    "Click Sustain",
    "Dynamics",
    "Differential transient: fast peak vs slow RMS. Attack clicks up, body ducks. Kicks, hats, claps.",
    `param a = Attack [0.5, 3.2]
param b = Body [0.2, 1.6]
param c = Rel [0.03, 0.28]
param d = Harm [0.8, 2.4]
param e = Tone [1800, 8000]
param f = Level [0.8, 1.3]
env1: type = peak; attack = 0.0005; release = 0.035
env2: type = rms; attack = 0.012; release = c
filter1: type = highpass; cutoff = 40; resonance = 0.16
stage1: y = x * (1 + env1 * a) / (1 + env2 * b * 0.55)
stage2: y = tube(y, d)
eq1: type = highshelf; freq = e; q = 0.7; gain = 2.2
filter2: type = lowpass; cutoff = 13000; resonance = 0.18
stage3: y = hardclip(softclip(y * f, 1.1), 0.86)`,
    {
      a: p("Attack", 0.5, 3.2, 1.7),
      b: p("Body", 0.2, 1.6, 0.85),
      c: p("Rel", 0.03, 0.28, 0.1),
      d: p("Harm", 0.8, 2.4, 1.35),
      e: p("Tone", 1800, 8000, 4800),
      f: p("Level", 0.8, 1.3, 1.05),
    },
    ["transient", "envelope", "kick"],
  ),
];

let n = 0;
for (const rec of added) {
  if (have.has(rec.name)) {
    console.log("skip existing", rec.name);
    continue;
  }
  list.push(rec);
  have.add(rec.name);
  n += 1;
}
fs.writeFileSync(path, JSON.stringify(list, null, 2) + "\n");
const vocals = list.filter((p) => p.category === "Vocals").length;
console.log("appended", n, "total", list.length, "vocals", vocals);
