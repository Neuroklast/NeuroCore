/**
 * Second factory wave: distinct studio jobs, not knob clones.
 * registerWave2(add, p) appends >= 339 presets to the existing 191.
 */
import { registerRest } from "./wave2_rest.mjs";
const irA = { ir1: "American IR 01.wav" };
const irB = { ir1: "British IR 01.wav" };
const irM = { ir1: "Medium IR 01.wav" };
const irV = { ir1: "Vintage IR 01.wav" };

const opt = (p, spec, extra = {}) => {
  const o = { outG: 0, ...extra };
  for (const k of ["a", "b", "c", "d", "e", "f"])
    if (spec[k]) o[k] = spec[k];
  return o;
};

export function registerWave2 (add, p)
{
  // =========================================================================
  // VOCALS (+28)
  // =========================================================================
  add("FET Peak Catch", "Vocals",
    "Insert on a lead vocal. Fast FET catches peaks. Mix 100.",
    `param a = Drive [2, 10]
param b = Attack [0.0002, 0.008]
param c = Release [0.04, 0.28]
filter1: type = highpass; cutoff = 80; resonance = 0.25
comp1: threshold = -a; ratio = 8; attack = b; release = c; knee = 2; makeup = 3
eq1: type = peak; freq = 3200; q = 1.1; gain = 1.8`,
    opt(p, { a: p("Drive", 2, 10, 5.5), b: p("Attack", 0.0002, 0.008, 0.001), c: p("Release", 0.04, 0.28, 0.09) }));

  add("Opto Leveler", "Vocals",
    "Insert after a peak catcher. Slow opto rides the level. Mix 100.",
    `param a = Pull [4, 14]
param b = Makeup [0, 8]
param c = Air [0, 5]
stage1: y = tube(x, 0.85)
comp1: threshold = -a; ratio = 3; attack = 0.012; release = 0.22; knee = 8; makeup = b
eq1: type = highshelf; freq = 9000; q = 0.6; gain = c`,
    opt(p, { a: p("Pull", 4, 14, 8), b: p("Makeup", 0, 8, 2.5), c: p("Air", 0, 5, 2.2) }));

  add("Serial Vocal Stack", "Vocals",
    "Classic FET into opto on one insert. Lead vocal, Mix 100.",
    `param a = Peak [3, 12]
param b = Ride [4, 12]
param c = Air [0, 4]
filter1: type = highpass; cutoff = 90; resonance = 0.22
comp1: threshold = -a; ratio = 8; attack = 0.0008; release = 0.07; knee = 1; makeup = 2
comp2: threshold = -b; ratio = 3; attack = 0.015; release = 0.25; knee = 10; makeup = 2
eq1: type = highshelf; freq = 10000; q = 0.55; gain = c`,
    opt(p, { a: p("Peak", 3, 12, 6), b: p("Ride", 4, 12, 7), c: p("Air", 0, 4, 1.6) }));

  add("Rap Punch Chain", "Vocals",
    "Rap vocal insert: 80 Hz cut, fast FET, de-ess on the air band. Mix 100.",
    `param a = Punch [3, 12]
param b = Ess [2, 10]
param c = Presence [0, 5]
filter1: type = highpass; cutoff = 80; resonance = 0.3
comp1: threshold = -a; ratio = 12; attack = 0.0004; release = 0.05; knee = 1; makeup = 3
eq1: type = peak; freq = 2800; q = 1.2; gain = c
xover1: f1 = 4200; f2 = 9000
bus high:
  comp2: threshold = -b; ratio = 6; attack = 0.001; release = 0.04; makeup = 0
out: low = 1; mid = 1; high = 1`,
    opt(p, { a: p("Punch", 3, 12, 7), b: p("Ess", 2, 10, 5), c: p("Presence", 0, 5, 2) }));

  add("De-Ess Shelf", "Vocals",
    "Insert de-ess: only the top band is compressed. Mix 100.",
    `param a = Ess [3, 14]
param b = BandHz [3500, 8000]
param c = Air [0, 3]
xover1: f1 = b; f2 = 11000
bus high:
  comp1: threshold = -a; ratio = 8; attack = 0.0006; release = 0.035; makeup = 0
  eq1: type = highshelf; freq = 12000; q = 0.5; gain = c
out: low = 1; mid = 1; high = 1`,
    opt(p, { a: p("Ess", 3, 14, 7), b: p("BandHz", 3500, 8000, 5200), c: p("Air", 0, 3, 0.8) }));

  add("Slap And Booth", "Vocals",
    "Vocal send: 95 ms slap plus a small booth. Blend is wet. Mix 25–40 on a send.",
    `param a = Slap [70, 140]
param b = Room [0.12, 0.42]
param c = Blend [0.15, 0.7]
stage1: y = x
bus wet:
  send: in = 1
  delay1: time = a; feedback = 0.08; mix = 1; damp = 4500
  reverb1: size = b; decay = 0.22; damp = 0.55; mix = 1; width = 0.45
out: main = 1-c; wet = c`,
    opt(p, { a: p("Slap", 70, 140, 95), b: p("Room", 0.12, 0.42, 0.22), c: p("Blend", 0.15, 0.7, 0.32), mix: 0.35 }));

  add("Dotted Eighth Throw", "Vocals",
    "Pop vocal throw. Dotted 1/8, dark. Use as a send, Mix 20–35.",
    `param a = Feedback [0.12, 0.48]
param b = Tone [1800, 6500]
param c = Blend [0.12, 0.55]
stage1: y = x
bus throw:
  send: in = 1
  filter1: type = highpass; cutoff = 180; resonance = 0.25
  delay1: time = 1/8.; feedback = a; mix = 1; damp = b
out: main = 1-c; throw = c`,
    opt(p, { a: p("Feedback", 0.12, 0.48, 0.26), b: p("Tone", 1800, 6500, 3800), c: p("Blend", 0.12, 0.55, 0.28), mix: 0.28 }));

  add("Quarter Note Halo", "Vocals",
    "Ballad vocal send. Straight 1/4, wider and wetter than the dotted throw.",
    `param a = Feedback [0.18, 0.52]
param b = Size [0.2, 0.55]
param c = Blend [0.15, 0.6]
stage1: y = x
bus halo:
  send: in = 1
  delay1: time = 1/4; feedback = a; mix = 1; damp = 4200
  reverb1: size = b; decay = 0.45; damp = 0.5; mix = 0.35; width = 0.7
out: main = 1-c; halo = c`,
    opt(p, { a: p("Feedback", 0.18, 0.52, 0.3), b: p("Size", 0.2, 0.55, 0.34), c: p("Blend", 0.15, 0.6, 0.3), mix: 0.3 }));

  add("Dual Throw Lattice", "Vocals",
    "Two throws: dotted 1/8 left-ish and 1/4. Pop chorus send.",
    `param a = Short [0.1, 0.4]
param b = Long [0.12, 0.45]
param c = Blend [0.15, 0.55]
stage1: y = x
bus short:
  send: in = 1
  delay1: time = 1/8.; feedback = a; mix = 1; damp = 5000
bus long:
  send: in = 1
  delay2: time = 1/4; feedback = b; mix = 1; damp = 3600
out: main = 1-c; short = c*0.55; long = c*0.45`,
    opt(p, { a: p("Short", 0.1, 0.4, 0.2), b: p("Long", 0.12, 0.45, 0.24), c: p("Blend", 0.15, 0.55, 0.3), mix: 0.3 }));

  add("Radio Band Vocal", "Vocals",
    "AM radio vocal. Tight band, light crush, then LPF. Insert or send.",
    `param a = Bits [5, 11]
param b = Center [800, 2200]
param c = Width [400, 1400]
filter1: type = bandpass; center = b; width = c
stage1: y = bitcrush(softclip(x, 1.4), a)
filter2: type = lowpass; cutoff = 4500; resonance = 0.3`,
    opt(p, { a: p("Bits", 5, 11, 8), b: p("Center", 800, 2200, 1400), c: p("Width", 400, 1400, 800) }));

  add("Telephone Cup", "Vocals",
    "Narrower than Radio Band. 400–3 kHz cup, diode grit.",
    `param a = Grit [0.8, 3.5]
param b = Low [280, 700]
param c = High [2200, 4200]
filter1: type = highpass; cutoff = b; resonance = 0.45
filter2: type = lowpass; cutoff = c; resonance = 0.4
stage1: y = diode(x, a)`,
    opt(p, { a: p("Grit", 0.8, 3.5, 1.6), b: p("Low", 280, 700, 400), c: p("High", 2200, 4200, 3000) }));

  add("Haas Vocal Double", "Vocals",
    "Mono vocal to a tight double. Mid stays the dry word. Insert, Mix 100.",
    `param a = Offset [8, 28]
param b = Width [0.3, 1]
param c = High [4000, 10000]
widen1: width = b; delay = a; bass = 140
eq1: type = highshelf; freq = c; q = 0.55; gain = 1.4`,
    opt(p, { a: p("Offset", 8, 28, 16), b: p("Width", 0.3, 1, 0.62), c: p("High", 4000, 10000, 7500) }));

  add("Air Only Vocal", "Vocals",
    "Add air without more compression. High shelf + tiny tube. Insert.",
    `param a = Air [1, 8]
param b = Warm [0.4, 1.6]
param c = LowCut [40, 140]
filter1: type = highpass; cutoff = c; resonance = 0.2
stage1: y = tube(x, b)
eq1: type = highshelf; freq = 11000; q = 0.5; gain = a`,
    opt(p, { a: p("Air", 1, 8, 3.4), b: p("Warm", 0.4, 1.6, 0.75), c: p("LowCut", 40, 140, 70) }));

  add("Presence Pocket", "Vocals",
    "3 kHz pocket so the lyric cuts a dense mix. Light glue after.",
    `param a = Cut [0, 6]
param b = Glue [2, 8]
param c = Dip [200, 500]
eq1: type = peak; freq = 3100; q = 1.35; gain = a
eq2: type = peak; freq = c; q = 1.1; gain = -2.2
comp1: threshold = -b; ratio = 3; attack = 0.006; release = 0.12; makeup = 1.5`,
    opt(p, { a: p("Cut", 0, 6, 2.8), b: p("Glue", 2, 8, 4.5), c: p("Dip", 200, 500, 320) }));

  add("Breath Gate", "Vocals",
    "Close-mic vocal. Gate the breaths, keep the line. Mix 100.",
    `param a = Open [-56, -28]
param b = Hold [0.02, 0.16]
param c = Range [-70, -20]
gate1: threshold = a; hyst = 5; hold = b; range = c
filter1: type = highpass; cutoff = 70; resonance = 0.22
eq1: type = highshelf; freq = 8000; q = 0.55; gain = 1.2`,
    opt(p, { a: p("Open", -56, -28, -42), b: p("Hold", 0.02, 0.16, 0.06), c: p("Range", -70, -20, -48) }));

  add("Choir Widen Hall", "Vocals",
    "Stack or choir. Width then a long hall on a send blend.",
    `param a = Width [0.25, 1]
param b = Hall [0.35, 0.75]
param c = Blend [0.12, 0.5]
widen1: delay = 14; mix = a
bus hall:
  send: in = 1
  reverb1: size = b; decay = 1.6; damp = 0.42; mix = 1; width = 0.85
out: main = 1-c; hall = c`,
    opt(p, { a: p("Width", 0.25, 1, 0.55), b: p("Hall", 0.35, 0.75, 0.55), c: p("Blend", 0.12, 0.5, 0.24), mix: 0.35 }));

  add("Shout Clip Vocal", "Vocals",
    "Screamed / shouted vocal. HPF, soft-pre hard ceiling, dark recovery.",
    `param a = Drive [1.4, 6]
param b = Ceiling [0.35, 0.85]
param c = Tone [2500, 7000]
filter1: type = highpass; cutoff = 140; resonance = 0.35
stage1: y = hardclip(softclip(x, a), b)
filter2: type = lowpass; cutoff = c; resonance = 0.32`,
    opt(p, { a: p("Drive", 1.4, 6, 2.8), b: p("Ceiling", 0.35, 0.85, 0.58), c: p("Tone", 2500, 7000, 4800) }));

  add("Narration Seat", "Vocals",
    "Spoken word / VO. Gentle EQ, slow leveler, peak limit. Mix 100.",
    `param a = Body [0, 4]
param b = Ride [3, 10]
param c = Ceiling [-3, -0.2]
eq1: type = peak; freq = 180; q = 0.8; gain = a
eq2: type = peak; freq = 4500; q = 0.9; gain = 1.2
comp1: threshold = -b; ratio = 2.5; attack = 0.02; release = 0.3; knee = 10; makeup = 1
limit1: ceiling = c; release = 0.12`,
    opt(p, { a: p("Body", 0, 4, 1.6), b: p("Ride", 3, 10, 6), c: p("Ceiling", -3, -0.2, -0.8) }));

  add("Mid Word Side Bloom", "Vocals",
    "MS vocal: mid stays dry words, sides get a short bloom.",
    `param a = Bloom [0.15, 0.5]
param b = Size [0.18, 0.45]
param c = Width [0.4, 1]
ms1: mode = encode
stage1: channel = mid; y = x
stage2: channel = side; y = x
reverb1: channel = side; size = b; decay = a; damp = 0.5; mix = 0.55; width = c
ms2: mode = decode`,
    opt(p, { a: p("Bloom", 0.15, 0.5, 0.28), b: p("Size", 0.18, 0.45, 0.28), c: p("Width", 0.4, 1, 0.7) }));

  add("Vocal Plate Throw", "Vocals",
    "Short bright plate on a send. Not a hall.",
    `param a = Size [0.12, 0.38]
param b = Decay [0.18, 0.55]
param c = Blend [0.12, 0.5]
stage1: y = x
bus plate:
  send: in = 1
  filter1: type = highpass; cutoff = 220; resonance = 0.25
  reverb1: size = a; decay = b; damp = 0.35; mix = 1; width = 0.72
out: main = 1-c; plate = c`,
    opt(p, { a: p("Size", 0.12, 0.38, 0.22), b: p("Decay", 0.18, 0.55, 0.32), c: p("Blend", 0.12, 0.5, 0.26), mix: 0.28 }));

  add("Side Air Only", "Vocals",
    "MS: lift air on the sides only. Center stays put.",
    `param a = Air [1, 7]
param b = AirHz [5000, 12000]
param c = SideGain [0.7, 1.3]
ms1: mode = encode
stage1: channel = mid; y = x
eq1: channel = side; type = highshelf; freq = b; q = 0.5; gain = a
stage2: channel = side; y = x * c
ms2: mode = decode`,
    opt(p, { a: p("Air", 1, 7, 3.2), b: p("AirHz", 5000, 12000, 8500), c: p("SideGain", 0.7, 1.3, 1.0) }));

  add("Parallel Vocal Smash", "Vocals",
    "NY vocal: dry word plus a smashed bus. Blend is the smash.",
    `param a = Smash [4, 14]
param b = Tone [2000, 7000]
param c = Blend [0.12, 0.55]
stage1: y = x
bus smash:
  send: in = 1
  filter1: type = highpass; cutoff = 160; resonance = 0.3
  stage2: y = tube(x, 2.2)
  comp1: threshold = -a; ratio = 12; attack = 0.0005; release = 0.08; makeup = 6
  filter2: type = lowpass; cutoff = b; resonance = 0.28
out: main = 1-c; smash = c`,
    opt(p, { a: p("Smash", 4, 14, 8), b: p("Tone", 2000, 7000, 4500), c: p("Blend", 0.12, 0.55, 0.28) }));

  add("Track Then Slap", "Vocals",
    "Insert EQ/comp plus a dedicated slap send in one preset.",
    `param a = Glue [3, 10]
param b = Slap [70, 130]
param c = Send [0.08, 0.4]
filter1: type = highpass; cutoff = 85; resonance = 0.22
eq1: type = peak; freq = 3500; q = 1.05; gain = 1.6
comp1: threshold = -a; ratio = 4; attack = 0.003; release = 0.1; makeup = 2
bus slap:
  send: in = 1
  delay1: time = b; feedback = 0.06; mix = 1; damp = 5000
out: main = 1; slap = c`,
    opt(p, { a: p("Glue", 3, 10, 5.5), b: p("Slap", 70, 130, 98), c: p("Send", 0.08, 0.4, 0.18) }));

  add("Talk Formant", "Vocals",
    "Envelope moves a bandpass so the mouth shape talks. Creative insert.",
    `param a = Depth [200, 1600]
param b = Center [700, 1800]
param c = Mix [0.2, 1]
env1: attack = 0.008; release = 0.09
stage1: y = x
bus mouth:
  send: in = 1
  filter1: type = bandpass; center = b + env1 * a; width = 650; resonance = 1.4
out: main = 1-c; mouth = c`,
    opt(p, { a: p("Depth", 200, 1600, 700), b: p("Center", 700, 1800, 1100), c: p("Mix", 0.2, 1, 0.55) }));

  add("Widen Then Slap", "Vocals",
    "Double first, then a short slap on the wide image.",
    `param a = Width [0.25, 0.9]
param b = Slap [60, 120]
param c = Mix [0.15, 0.55]
widen1: delay = 12; mix = a
delay1: time = b; feedback = 0.1; mix = c; damp = 4800`,
    opt(p, { a: p("Width", 0.25, 0.9, 0.5), b: p("Slap", 60, 120, 88), c: p("Mix", 0.15, 0.55, 0.22) }));

  add("Air Band Polish", "Vocals",
    "Crossover: compress the air band and add a silk shelf there only.",
    `param a = Silk [0, 5]
param b = Pull [2, 10]
param c = AirHz [6000, 11000]
xover1: f1 = 250; f2 = c
bus mid:
  eq2: type = peak; freq = 1800; q = 0.8; gain = 0.6
bus high:
  comp1: threshold = -b; ratio = 3; attack = 0.004; release = 0.08; makeup = 1
  eq1: type = highshelf; freq = 12000; q = 0.45; gain = a
out: low = 1; mid = 1; high = 1`,
    opt(p, { a: p("Silk", 0, 5, 2.2), b: p("Pull", 2, 10, 5), c: p("AirHz", 6000, 11000, 8000) }));

  add("Pop Lead Desk", "Vocals",
    "Full pop insert: HPF, FET, de-ess, slap send. Mix 100, send is built in.",
    `param a = Peak [3, 11]
param b = Ess [3, 12]
param c = Throw [0.08, 0.35]
filter1: type = highpass; cutoff = 90; resonance = 0.24
comp1: threshold = -a; ratio = 6; attack = 0.001; release = 0.08; makeup = 2.5
xover1: f1 = 4800; f2 = 9500
bus high:
  comp2: threshold = -b; ratio = 6; attack = 0.0008; release = 0.04
bus throw:
  send: in = 1
  delay1: time = 1/8.; feedback = 0.22; mix = 1; damp = 4200
out: low = 1; mid = 1; high = 1; throw = c`,
    opt(p, { a: p("Peak", 3, 11, 6), b: p("Ess", 3, 12, 6.5), c: p("Throw", 0.08, 0.35, 0.16) }));

  add("Intimate Whisper", "Vocals",
    "Close whisper: high HPF, gentle gate, warm tube, no slap.",
    `param a = Warm [0.5, 2.2]
param b = Open [-60, -32]
param c = Air [0, 4]
gate1: threshold = b; hyst = 4; hold = 0.04; range = -36
filter1: type = highpass; cutoff = 160; resonance = 0.2
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 7500; q = 0.55; gain = c`,
    opt(p, { a: p("Warm", 0.5, 2.2, 1.1), b: p("Open", -60, -32, -46), c: p("Air", 0, 4, 1.8) }));

  registerGuitar (add, p);
  registerRest (add, p, opt, irA, irB, irM, irV);
}

function registerGuitar (add, p)
{
  add("Green Mid Into Amp", "Guitar",
    "TS mid-hump into a soft tube stage and vintage cab. Rhythm DI.",
    `param a = Drive [1.5, 10]
param b = Amp [0.8, 4.5]
param c = Tone [1200, 5500]
filter1: type = highpass; cutoff = 720; resonance = 0.28
stage1: y = softclip(x, a)
eq1: type = peak; freq = 720; q = 1.6; gain = 5.5
stage2: y = tube(y, b)
filter2: type = lowpass; cutoff = c; resonance = 0.4
ir1: mix = 0.5; gain = 2`,
    opt(p, { a: p("Drive", 1.5, 10, 5), b: p("Amp", 0.8, 4.5, 2.1), c: p("Tone", 1200, 5500, 2800), tags: ["amp", "ir"] }, { irs: irV }));

  add("Filter Dist Pedal", "Guitar",
    "RAT-style: clip first, steep tone after, British cab. Not the stock RAT.",
    `param a = Dist [2.5, 12]
param b = Filter [400, 4200]
param c = Cab [0.2, 0.7]
filter1: type = highpass; cutoff = 85; resonance = 0.32
stage1: y = hardclip(softclip(x, a * 0.5), 0.5)
filter2: type = lowpass; cutoff = b; resonance = 0.62
ir1: mix = c; gain = 3`,
    opt(p, { a: p("Dist", 2.5, 12, 7), b: p("Filter", 400, 4200, 1600), c: p("Cab", 0.2, 0.7, 0.42), tags: ["ir"] }, { irs: irB }));

  add("Tall Stack Fuzz", "Guitar",
    "Muff-like stacked filters around a fold, then a dark cab.",
    `param a = Fuzz [2, 10]
param b = Scoop [400, 1400]
param c = Top [1800, 5000]
filter1: type = highpass; cutoff = 70; resonance = 0.35
filter2: type = lowpass; cutoff = c; resonance = 0.45
stage1: y = fold(x, -0.35, 0.35)
stage2: y = softclip(y, a)
eq1: type = peak; freq = b; q = 0.9; gain = -5
ir1: mix = 0.4; gain = 4`,
    opt(p, { a: p("Fuzz", 2, 10, 5.5), b: p("Scoop", 400, 1400, 800), c: p("Top", 1800, 5000, 3200), tags: ["ir"] }, { irs: irV }));

  add("Clean Spring Send", "Guitar",
    "Clean DI: light tube, then a spring send. Surf / country.",
    `param a = Clean [0.4, 2]
param b = Spring [0.18, 0.5]
param c = Send [0.12, 0.55]
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 4500; q = 0.6; gain = 2
bus spring:
  send: in = 1
  reverb1: size = b; decay = 0.35; damp = 0.28; mix = 1; width = 0.4
out: main = 1-c; spring = c`,
    opt(p, { a: p("Clean", 0.4, 2, 0.9), b: p("Spring", 0.18, 0.5, 0.3), c: p("Send", 0.12, 0.55, 0.28) }));

  add("Chime Top End", "Guitar",
    "AC-style chime without the stock AC30 graph: 2.2 kHz bell + open cab.",
    `param a = Drive [0.8, 4]
param b = Bell [1, 7]
param c = Air [5000, 10000]
filter1: type = highpass; cutoff = 480; resonance = 0.42
stage1: y = tube(x, a)
eq1: type = peak; freq = 2200; q = 1.3; gain = b
filter2: type = lowpass; cutoff = c; resonance = 0.28
ir1: mix = 0.35; gain = 1`,
    opt(p, { a: p("Drive", 0.8, 4, 1.8), b: p("Bell", 1, 7, 3.6), c: p("Air", 5000, 10000, 8200), tags: ["ir"] }, { irs: irA }));

  add("Scooped Rhythm Wall", "Guitar",
    "High-gain rhythm: tight HPF, 400 Hz scoop, gate, American cab.",
    `param a = Gain [4, 12]
param b = Tight [55, 160]
param c = Scoop [-8, -2]
gate1: threshold = -44; hyst = 6; hold = 0.025; range = -78
filter1: type = highpass; cutoff = b; resonance = 0.55
stage1: y = tube(x, a * 0.55)
stage2: y = tube(y, a * 0.4)
eq1: type = peak; freq = 400; q = 1.05; gain = c
filter2: type = lowpass; cutoff = 4800; resonance = 0.5
ir1: mix = 0.48; gain = 0`,
    opt(p, { a: p("Gain", 4, 12, 8), b: p("Tight", 55, 160, 90), c: p("Scoop", -8, -2, -4.5), tags: ["gate", "ir"] }, { irs: irA }));

  add("Tight Gate Lead", "Guitar",
    "Single-note lead: faster gate, 3.5 kHz presence, British cab.",
    `param a = Gain [3, 11]
param b = Presence [2, 7]
param c = Gate [-50, -28]
gate1: threshold = c; hyst = 4; hold = 0.015; range = -75
filter1: type = highpass; cutoff = 130; resonance = 0.4
stage1: y = tube(x, a * 0.6)
stage2: y = softclip(y, 1.6)
eq1: type = peak; freq = 3500; q = 1.2; gain = b
ir1: mix = 0.46; gain = 1`,
    opt(p, { a: p("Gain", 3, 11, 6.5), b: p("Presence", 2, 7, 3.8), c: p("Gate", -50, -28, -38), tags: ["gate", "ir"] }, { irs: irB }));

  add("Edge Of Breakup", "Guitar",
    "One soft tube, no cab IR. Dynamic clean-to-crunch for pickers.",
    `param a = Push [0.6, 3.2]
param b = Low [50, 140]
param c = Top [4500, 9000]
filter1: type = highpass; cutoff = b; resonance = 0.25
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 3800; q = 0.6; gain = 1.8
filter2: type = lowpass; cutoff = c; resonance = 0.26`,
    opt(p, { a: p("Push", 0.6, 3.2, 1.4), b: p("Low", 50, 140, 75), c: p("Top", 4500, 9000, 6800) }));

  add("Pedalboard Crunch", "Guitar",
    "Klon-style clean blend into a crunch amp, medium cab.",
    `param a = Pedal [1, 7]
param b = Amp [1, 5]
param c = Blend [0.25, 0.85]
stage1: y = x
bus dirt:
  send: in = 1
  stage2: y = diode(x, a)
  stage3: y = tube(y, b)
  filter1: type = lowpass; cutoff = 6200; resonance = 0.35
  ir1: mix = 0.4; gain = 2
out: main = 1-c; dirt = c`,
    opt(p, { a: p("Pedal", 1, 7, 3.2), b: p("Amp", 1, 5, 2.2), c: p("Blend", 0.25, 0.85, 0.58), tags: ["ir"] }, { irs: irM }));

  add("Wet Dry Split Guitar", "Guitar",
    "Left stays drier amp, right is delayed wet. Stereo DI / dual cab feel.",
    `param a = Drive [1, 6]
param b = Wet [0.15, 0.6]
param c = Time [12, 40]
split1: type = leftright {
  left {
    stage1: y = tube(x, a * 0.7)
    ir1: mix = 0.4; gain = 0
  }
  right {
    delay1: time = c; feedback = 0.05; mix = b; damp = 5000
    stage2: y = tube(x, a)
    ir2: mix = 0.45; gain = 1
  }
}`,
    opt(p, { a: p("Drive", 1, 6, 2.8), b: p("Wet", 0.15, 0.6, 0.32), c: p("Time", 12, 40, 22), tags: ["ir", "stereo"] }, { irs: { ir1: "American IR 01.wav", ir2: "British IR 01.wav" } }));

  add("Script Phaser Guitar", "Guitar",
    "Four-stage-ish phaser on a guitar insert. Slow swirl.",
    `param a = Rate [0.08, 1.4]
param b = Depth [400, 2200]
param c = Mix [0.2, 0.85]
osc1: shape = sine; freq = a; depth = 1
stage1: y = x
bus swirl:
  send: in = 1
  filter1: type = bandpass; center = 700 + osc1 * b; width = 500; resonance = 0.7
  filter2: type = bandpass; center = 1400 + osc1 * b * 0.7; width = 600; resonance = 0.65
out: main = 1-c; swirl = c`,
    opt(p, { a: p("Rate", 0.08, 1.4, 0.28), b: p("Depth", 400, 2200, 1100), c: p("Mix", 0.2, 0.85, 0.5) }));

  add("Analog Chorus Guitar", "Guitar",
    "CE-style: short modulated delay, not a phaser.",
    `param a = Rate [0.15, 2.2]
param b = Depth [4, 18]
param c = Mix [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 12 + osc1 * b; feedback = 0.08; mix = c; damp = 7000`,
    opt(p, { a: p("Rate", 0.15, 2.2, 0.55), b: p("Depth", 4, 18, 9), c: p("Mix", 0.2, 0.7, 0.38) }));

  add("Vibe Rotary Guitar", "Guitar",
    "Uni-vibe cousin: AM plus a sweeping bandpass. Slow rotary.",
    `param a = Rate [0.2, 3]
param b = Chorus [300, 1800]
param c = AM [0.15, 0.7]
osc1: shape = sine; freq = a; depth = 1
filter1: type = bandpass; center = 900 + osc1 * b; width = 700; resonance = 0.8
stage1: y = x * (1 - c + c * (0.5 + 0.5 * osc1))`,
    opt(p, { a: p("Rate", 0.2, 3, 0.7), b: p("Chorus", 300, 1800, 800), c: p("AM", 0.15, 0.7, 0.35) }));

  add("Country Slap Guitar", "Guitar",
    "Tele / country: clean tube, 95 ms slap, light spring.",
    `param a = Clean [0.4, 2]
param b = Slap [70, 130]
param c = Room [0.08, 0.35]
stage1: y = tube(x, a)
delay1: time = b; feedback = 0.06; mix = 0.22; damp = 5200
reverb1: size = 0.2; decay = c; damp = 0.4; mix = 0.18; width = 0.4
ir1: mix = 0.3; gain = 2`,
    opt(p, { a: p("Clean", 0.4, 2, 0.85), b: p("Slap", 70, 130, 95), c: p("Room", 0.08, 0.35, 0.16), tags: ["ir"] }, { irs: irA }));

  add("Surf Spring Crash", "Guitar",
    "Splashier spring than Clean Spring Send. More drip, more HPF.",
    `param a = Drip [0.25, 0.7]
param b = Drive [0.5, 2.4]
param c = Splash [0.2, 0.65]
filter1: type = highpass; cutoff = 160; resonance = 0.35
stage1: y = tube(x, b)
reverb1: size = a; decay = 0.55; damp = 0.22; mix = c; width = 0.35`,
    opt(p, { a: p("Drip", 0.25, 0.7, 0.42), b: p("Drive", 0.5, 2.4, 1.1), c: p("Splash", 0.2, 0.65, 0.38) }));

  add("Jazz Box Warm", "Guitar",
    "Hollow-body jazz: low cut gentle, dark LPF, tiny tube, no IR.",
    `param a = Warm [0.4, 1.8]
param b = Dark [1800, 4500]
param c = Body [0, 4]
filter1: type = highpass; cutoff = 70; resonance = 0.2
eq1: type = peak; freq = 220; q = 0.8; gain = c
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.3`,
    opt(p, { a: p("Warm", 0.4, 1.8, 0.9), b: p("Dark", 1800, 4500, 2800), c: p("Body", 0, 4, 1.8) }));

  add("Funk Wah Env", "Guitar",
    "Auto-wah for 16th funk. Envelope opens a bandpass.",
    `param a = Range [400, 2400]
param b = Q [0.8, 2.4]
param c = Mix [0.35, 1]
env1: attack = 0.004; release = 0.08
stage1: y = x
bus wah:
  send: in = 1
  filter1: type = bandpass; center = 350 + env1 * a; width = 400; resonance = b
out: main = 1-c; wah = c`,
    opt(p, { a: p("Range", 400, 2400, 1400), b: p("Q", 0.8, 2.4, 1.5), c: p("Mix", 0.35, 1, 0.75) }));

  add("Metal Scoop Cab", "Guitar",
    "Modern metal: 90 Hz tight, 400 scoop, 1.5 kHz bark, dark cab.",
    `param a = Gain [5, 13]
param b = Bark [1, 6]
param c = Cab [0.3, 0.7]
gate1: threshold = -42; hyst = 5; hold = 0.02; range = -80
filter1: type = highpass; cutoff = 90; resonance = 0.58
stage1: y = tube(x, a * 0.5)
stage2: y = hardclip(softclip(y, 1.4), 0.62)
eq1: type = peak; freq = 400; q = 1.1; gain = -6
eq2: type = peak; freq = 1500; q = 1.15; gain = b
filter2: type = lowpass; cutoff = 4200; resonance = 0.48
ir1: mix = c; gain = 0`,
    opt(p, { a: p("Gain", 5, 13, 8.5), b: p("Bark", 1, 6, 3.2), c: p("Cab", 0.3, 0.7, 0.5), tags: ["gate", "ir"] }, { irs: irA }));

  add("British Crunch Stack", "Guitar",
    "Two British-voiced tubes, 800 Hz bark, no American scoop.",
    `param a = Drive [1.6, 8]
param b = Bark [2, 7]
param c = Cab [0.25, 0.65]
filter1: type = highpass; cutoff = 110; resonance = 0.38
stage1: y = tube(x, a * 0.65)
stage2: y = tube(y, a * 0.5)
eq1: type = peak; freq = 800; q = 1.05; gain = b
filter2: type = lowpass; cutoff = 5600; resonance = 0.4
ir1: mix = c; gain = 1`,
    opt(p, { a: p("Drive", 1.6, 8, 3.8), b: p("Bark", 2, 7, 4), c: p("Cab", 0.25, 0.65, 0.42), tags: ["ir"] }, { irs: irB }));

  add("American Clean Cab", "Guitar",
    "Open-cab cousin with IR. Brighter and more open than Edge Of Breakup.",
    `param a = Drive [0.4, 2.2]
param b = Bright [0, 6]
param c = Cab [0.2, 0.6]
filter1: type = highpass; cutoff = 50; resonance = 0.2
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 5200; q = 0.55; gain = b
filter2: type = lowpass; cutoff = 9800; resonance = 0.22
ir1: mix = c; gain = 2`,
    opt(p, { a: p("Drive", 0.4, 2.2, 0.95), b: p("Bright", 0, 6, 3), c: p("Cab", 0.2, 0.6, 0.38), tags: ["ir"] }, { irs: irA }));

  add("Lead Presence Stack", "Guitar",
    "Lead: 4 kHz shelf, slower gate, medium cab. Sings above a rhythm wall.",
    `param a = Gain [3, 10]
param b = Air [1, 6]
param c = Sustain [0.02, 0.12]
gate1: threshold = -40; hyst = 4; hold = c; range = -70
filter1: type = highpass; cutoff = 140; resonance = 0.35
stage1: y = tube(x, a * 0.7)
eq1: type = highshelf; freq = 4000; q = 0.6; gain = b
ir1: mix = 0.44; gain = 1`,
    opt(p, { a: p("Gain", 3, 10, 6), b: p("Air", 1, 6, 3), c: p("Sustain", 0.02, 0.12, 0.05), tags: ["gate", "ir"] }, { irs: irM }));

  add("Rhythm Mid Scoop", "Guitar",
    "Palm-mute rhythm. Deeper 280 Hz scoop than Scooped Rhythm Wall, less gain.",
    `param a = Gain [3, 9]
param b = Scoop [-9, -2]
param c = Click [1, 5]
gate1: threshold = -46; hyst = 6; hold = 0.03; range = -78
filter1: type = highpass; cutoff = 100; resonance = 0.5
stage1: y = tube(x, a * 0.55)
eq1: type = peak; freq = 280; q = 1.2; gain = b
eq2: type = peak; freq = 1800; q = 1.1; gain = c
ir1: mix = 0.45; gain = 0`,
    opt(p, { a: p("Gain", 3, 9, 5.5), b: p("Scoop", -9, -2, -5), c: p("Click", 1, 5, 2.6), tags: ["gate", "ir"] }, { irs: irA }));

  add("Single Coil Bright", "Guitar",
    "Strat / tele: 5 kHz silk, light breakup, vintage cab.",
    `param a = Push [0.5, 2.6]
param b = Silk [1, 6]
param c = Grit [0.4, 1.8]
filter1: type = highpass; cutoff = 90; resonance = 0.28
stage1: y = tube(x, a)
stage2: y = diode(y, c)
eq1: type = highshelf; freq = 5200; q = 0.55; gain = b
ir1: mix = 0.32; gain = 2`,
    opt(p, { a: p("Push", 0.5, 2.6, 1.2), b: p("Silk", 1, 6, 3.2), c: p("Grit", 0.4, 1.8, 0.8), tags: ["ir"] }, { irs: irV }));

  add("Humbucker Thick", "Guitar",
    "LP / humbucker: 200 Hz body, darker LPF, British cab.",
    `param a = Drive [1, 5]
param b = Body [1, 6]
param c = Dark [3200, 6500]
filter1: type = highpass; cutoff = 70; resonance = 0.28
eq1: type = peak; freq = 200; q = 0.85; gain = b
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = c; resonance = 0.35
ir1: mix = 0.42; gain = 1`,
    opt(p, { a: p("Drive", 1, 5, 2.4), b: p("Body", 1, 6, 3), c: p("Dark", 3200, 6500, 4800), tags: ["ir"] }, { irs: irB }));

  add("Acoustic Sim Box", "Guitar",
    "Electric to faux-acoustic: notch body, high bandpass, tiny room.",
    `param a = Body [200, 500]
param b = Notch [-10, -3]
param c = Room [0.08, 0.3]
filter1: type = highpass; cutoff = 90; resonance = 0.3
eq1: type = peak; freq = a; q = 2.2; gain = b
filter2: type = bandpass; center = 1800; width = 1400; resonance = 0.45
reverb1: size = 0.18; decay = c; damp = 0.45; mix = 0.16; width = 0.5`,
    opt(p, { a: p("Body", 200, 500, 320), b: p("Notch", -10, -3, -6), c: p("Room", 0.08, 0.3, 0.16) }));

  add("Twelve String Chorus", "Guitar",
    "Short stereo chorus + sparkle shelf. 12-string illusion.",
    `param a = Rate [0.4, 2.5]
param b = Depth [5, 16]
param c = Sparkle [1, 5]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 9 + osc1 * b; feedback = 0.05; mix = 0.4; damp = 8000
eq1: type = highshelf; freq = 6500; q = 0.5; gain = c`,
    opt(p, { a: p("Rate", 0.4, 2.5, 1.1), b: p("Depth", 5, 16, 9), c: p("Sparkle", 1, 5, 2.4) }));

  add("Slide Dirt Bed", "Guitar",
    "Slide guitar: long sustain tube, dark, small room, no gate.",
    `param a = Sustain [1.2, 5]
param b = Dark [2200, 5200]
param c = Room [0.1, 0.4]
filter1: type = highpass; cutoff = 100; resonance = 0.25
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.32
reverb1: size = 0.28; decay = c; damp = 0.5; mix = 0.22; width = 0.45`,
    opt(p, { a: p("Sustain", 1.2, 5, 2.6), b: p("Dark", 2200, 5200, 3400), c: p("Room", 0.1, 0.4, 0.2) }));

  add("Ambient Swell Guitar", "Guitar",
    "Volume-swell pad: slow env opens a hall. Post-rock.",
    `param a = Rise [0.05, 0.4]
param b = Hall [0.4, 0.85]
param c = Blend [0.25, 0.75]
env1: attack = a; release = 0.35
stage1: y = x * (0.15 + 0.85 * env1)
reverb1: size = b; decay = 2.2; damp = 0.4; mix = c; width = 0.8`,
    opt(p, { a: p("Rise", 0.05, 0.4, 0.16), b: p("Hall", 0.4, 0.85, 0.62), c: p("Blend", 0.25, 0.75, 0.48) }));

  add("Volume Bloom Lead", "Guitar",
    "Env swells a tube lead into a short plate. Cinematic lead.",
    `param a = Drive [1.2, 6]
param b = Bloom [0.04, 0.22]
param c = Plate [0.15, 0.5]
env1: attack = b; release = 0.18
stage1: y = tube(x * (0.2 + 0.8 * env1), a)
reverb1: size = 0.3; decay = c; damp = 0.38; mix = 0.28; width = 0.6
ir1: mix = 0.3; gain = 1`,
    opt(p, { a: p("Drive", 1.2, 6, 3), b: p("Bloom", 0.04, 0.22, 0.09), c: p("Plate", 0.15, 0.5, 0.28), tags: ["ir"] }, { irs: irM }));
}
