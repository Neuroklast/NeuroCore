/** Remaining wave2 categories. Called from wave2.mjs. */

export function registerRest (add, p, opt, irA, irB, irM, irV)
{
  registerBass (add, p, opt, irA, irB, irM, irV);
  registerPitch (add, p, opt);
  registerDelay (add, p, opt);
  registerReverb (add, p, opt);
  registerDynamics (add, p, opt);
  registerDrums (add, p, opt);
  registerModulation (add, p, opt);
  registerFilter (add, p, opt);
  registerMastering (add, p, opt);
  registerUtility (add, p, opt);
  registerPsycho (add, p, opt);
  registerDistortion (add, p, opt, irA, irB, irM, irV);
  registerClub (add, p, opt);
  registerEdm (add, p, opt);
  registerLofi (add, p, opt);
  registerAmbient (add, p, opt);
  registerCinematic (add, p, opt);
  registerCreative (add, p, opt);
  registerSoundDesign (add, p, opt);
  registerSynth (add, p, opt);
}

function registerBass (add, p, opt, irA, irB, irM, irV)
{
  add("DI Amp Blend Bass", "Bass",
    "Clean DI plus a grind bus. Blend is the amp. Mix 100.",
    `param a = Grind [1, 6]
param b = Blend [0.15, 0.7]
param c = Low [40, 90]
filter1: type = highpass; cutoff = c; resonance = 0.25
stage1: y = x
bus amp:
  send: in = 1
  stage2: y = tube(x, a)
  filter2: type = lowpass; cutoff = 4200; resonance = 0.35
  ir1: mix = 0.35; gain = 2
out: main = 1-b; amp = b`,
    opt(p, { a: p("Grind", 1, 6, 2.8), b: p("Blend", 0.15, 0.7, 0.38), c: p("Low", 40, 90, 55), tags: ["ir"] }, { irs: irV }));

  add("Pick Attack Bass", "Bass",
    "Env boosts 2 kHz click on picked bass. Insert.",
    `param a = Click [0, 7]
param b = Fast [0.003, 0.03]
param c = Body [40, 100]
env1: type = peak; attack = 0.001; release = b
filter1: type = highpass; cutoff = c; resonance = 0.22
eq1: type = peak; freq = 2100; q = 1.3; gain = a * env1`,
    opt(p, { a: p("Click", 0, 7, 3.2), b: p("Fast", 0.003, 0.03, 0.01), c: p("Low", 40, 100, 50) }));

  add("Fretless Warm Bass", "Bass",
    "Dark LPF, slow opto, no click. Fretless / synth bass.",
    `param a = Dark [1200, 4000]
param b = Ride [3, 10]
param c = Tube [0.4, 1.8]
filter1: type = highpass; cutoff = 35; resonance = 0.2
stage1: y = tube(x, c)
comp1: threshold = -b; ratio = 3; attack = 0.02; release = 0.28; knee = 8; makeup = 2
filter2: type = lowpass; cutoff = a; resonance = 0.28`,
    opt(p, { a: p("Dark", 1200, 4000, 2400), b: p("Ride", 3, 10, 6), c: p("Tube", 0.4, 1.8, 0.9) }));

  add("Synth Sub Round", "Bass",
    "808 / sine sub: HPF 25, soft clip, tiny even harmonics.",
    `param a = Round [0.4, 2.2]
param b = Floor [22, 45]
param c = Ceiling [0.4, 0.95]
filter1: type = highpass; cutoff = b; resonance = 0.18
stage1: y = tube(x, a)
stage2: y = hardclip(softclip(y, 1.1), c)
filter2: type = lowpass; cutoff = 180; resonance = 0.25`,
    opt(p, { a: p("Round", 0.4, 2.2, 1.0), b: p("Floor", 22, 45, 28), c: p("Ceiling", 0.4, 0.95, 0.72) }));

  add("Slap Bass Pop", "Bass",
    "Slap bass: 3 kHz pop, fast FET, high HPF.",
    `param a = Pop [1, 7]
param b = Peak [3, 11]
param c = Cut [60, 140]
filter1: type = highpass; cutoff = c; resonance = 0.3
eq1: type = peak; freq = 3000; q = 1.4; gain = a
comp1: threshold = -b; ratio = 8; attack = 0.0006; release = 0.06; makeup = 2`,
    opt(p, { a: p("Pop", 1, 7, 3.4), b: p("Peak", 3, 11, 6), c: p("Cut", 60, 140, 90) }));

  add("Bass DI Chorus", "Bass",
    "Chorus only above 200 Hz so the sub stays mono.",
    `param a = Rate [0.2, 1.8]
param b = Depth [4, 14]
param c = Mix [0.15, 0.55]
xover1: f1 = 200
osc1: shape = sine; freq = a; depth = 1
bus high:
  delay1: time = 10 + osc1 * b; feedback = 0.06; mix = c; damp = 6000
out: low = 1; high = 1`,
    opt(p, { a: p("Rate", 0.2, 1.8, 0.55), b: p("Depth", 4, 14, 8), c: p("Mix", 0.15, 0.55, 0.3) }));

  add("Grit Mid Bass", "Bass",
    "Saturate 400–800 Hz only. Sub stays clean.",
    `param a = Grit [1, 5]
param b = Low [180, 400]
param c = High [700, 1400]
xover1: f1 = b; f2 = c
bus mid:
  stage1: y = diode(x, a)
out: low = 1; mid = 1; high = 1`,
    opt(p, { a: p("Grit", 1, 5, 2.4), b: p("Low", 180, 400, 280), c: p("High", 700, 1400, 900) }));

  add("Bass Amp Cab Only", "Bass",
    "No extra clip: just HPF, British cab, gentle LPF.",
    `param a = Cut [30, 80]
param b = Cab [0.25, 0.75]
param c = Top [2500, 6000]
filter1: type = highpass; cutoff = a; resonance = 0.22
filter2: type = lowpass; cutoff = c; resonance = 0.28
ir1: mix = b; gain = 3`,
    opt(p, { a: p("Cut", 30, 80, 45), b: p("Cab", 0.25, 0.75, 0.48), c: p("Top", 2500, 6000, 4000), tags: ["ir"] }, { irs: irB }));

  add("Sidechain Duck Bass", "Bass",
    "Bass ducks from the host sidechain (kick). Mix 100.",
    `param a = Depth [0.2, 0.9]
param b = Release [0.04, 0.28]
param c = Floor [25, 50]
sidechain1: mix = a
filter1: type = highpass; cutoff = c; resonance = 0.2
comp1: threshold = -18; ratio = 6; attack = 0.001; release = b; source = sidechain; makeup = 0`,
    opt(p, { a: p("Depth", 0.2, 0.9, 0.55), b: p("Release", 0.04, 0.28, 0.12), c: p("Floor", 25, 50, 32) }));

  add("Octave Dirt Bass", "Bass",
    "Sub octave then a tight tube. Not Precision Octaver alone.",
    `param a = Sub [0.2, 0.9]
param b = Dirt [0.8, 3.5]
param c = Low [30, 70]
filter1: type = highpass; cutoff = c; resonance = 0.2
octaver1: mix = a; sub = 1; up = 0; tone = 160; thresh = 0.05
stage1: y = tube(x, b)
filter2: type = lowpass; cutoff = 3500; resonance = 0.3`,
    opt(p, { a: p("Sub", 0.2, 0.9, 0.45), b: p("Dirt", 0.8, 3.5, 1.6), c: p("Low", 30, 70, 42) }));

  add("Mute Click Bass", "Bass",
    "Palm-mute bass: gate + 1.2 kHz click, no cab.",
    `param a = Open [-50, -28]
param b = Click [0, 6]
param c = Tight [50, 120]
gate1: threshold = a; hyst = 5; hold = 0.02; range = -70
filter1: type = highpass; cutoff = c; resonance = 0.4
eq1: type = peak; freq = 1200; q = 1.4; gain = b`,
    opt(p, { a: p("Open", -50, -28, -38), b: p("Click", 0, 6, 2.8), c: p("Tight", 50, 120, 75) }));

  add("Motown Bass Round", "Bass",
    "Round fingerstyle: 100 Hz bump, slow comp, vintage cab.",
    `param a = Boom [1, 6]
param b = Glue [3, 9]
param c = Cab [0.15, 0.5]
eq1: type = peak; freq = 100; q = 0.8; gain = a
comp1: threshold = -b; ratio = 3; attack = 0.025; release = 0.22; makeup = 2
ir1: mix = c; gain = 2`,
    opt(p, { a: p("Boom", 1, 6, 3), b: p("Glue", 3, 9, 5.5), c: p("Cab", 0.15, 0.5, 0.28), tags: ["ir"] }, { irs: irV }));

  add("Fuzz Bass Stack", "Bass",
    "Fuzz on a bus, dry sub stays. Blend is fuzz.",
    `param a = Fuzz [2, 9]
param b = Blend [0.12, 0.55]
param c = Tone [800, 3500]
stage1: y = x
bus fuzz:
  send: in = 1
  filter1: type = highpass; cutoff = 80; resonance = 0.3
  stage2: y = fold(x, -0.3, 0.3)
  stage3: y = softclip(y, a)
  filter2: type = lowpass; cutoff = c; resonance = 0.4
out: main = 1-b; fuzz = b`,
    opt(p, { a: p("Fuzz", 2, 9, 4.5), b: p("Blend", 0.12, 0.55, 0.28), c: p("Tone", 800, 3500, 1800) }));

  add("Bass Limit Polish", "Bass",
    "Transparent peak control after a DI. No tone change.",
    `param a = Ceiling [-4, -0.2]
param b = Release [0.04, 0.2]
param c = Low [25, 50]
filter1: type = highpass; cutoff = c; resonance = 0.18
limit1: ceiling = a; release = b`,
    opt(p, { a: p("Ceiling", -4, -0.2, -1), b: p("Release", 0.04, 0.2, 0.09), c: p("Low", 25, 50, 32) }));

  add("Direct Amp SVT Dark", "Bass",
    "Darker SVT cousin: more 200 Hz, less top, medium cab.",
    `param a = Drive [1.2, 6]
param b = Wool [1, 6]
param c = Cab [0.3, 0.7]
filter1: type = highpass; cutoff = 40; resonance = 0.25
eq1: type = peak; freq = 200; q = 0.85; gain = b
stage1: y = tube(x, a)
filter2: type = lowpass; cutoff = 3200; resonance = 0.32
ir1: mix = c; gain = 1`,
    opt(p, { a: p("Drive", 1.2, 6, 2.8), b: p("Wool", 1, 6, 3.2), c: p("Cab", 0.3, 0.7, 0.48), tags: ["ir"] }, { irs: irM }));
}

function registerPitch (add, p, opt)
{
  add("Sub Only Octave", "Pitch",
    "Just the analog sub, no dry. Bass / guitar. Mix to taste in the host.",
    `param a = Tone [80, 400]
param b = Level [0.3, 1.2]
param c = Low [30, 80]
filter1: type = highpass; cutoff = c; resonance = 0.2
octaver1: mix = 1; sub = 1; up = 0; tone = a; thresh = 0.05
stage1: y = x * b
filter2: type = lowpass; cutoff = 400; resonance = 0.25`,
    opt(p, { a: p("Tone", 80, 400, 180), b: p("Level", 0.3, 1.2, 0.7), c: p("Low", 30, 80, 45) }));

  add("Up Octave Chime", "Pitch",
    "Plus-one only, bright. 12-string / lead sparkle.",
    `param a = Mix [0.15, 0.7]
param b = Tone [200, 800]
param c = Air [1, 5]
octaver1: mix = a; sub = 0; up = 1; tone = b; thresh = 0.05
eq1: type = highshelf; freq = 6000; q = 0.5; gain = c`,
    opt(p, { a: p("Mix", 0.15, 0.7, 0.35), b: p("Tone", 200, 800, 420), c: p("Air", 1, 5, 2.2) }));

  add("Both Octaves Stack", "Pitch",
    "Sub + up together, mid scoop so it does not pile.",
    `param a = Mix [0.2, 0.8]
param b = Scoop [-6, 0]
param c = Tone [0.25, 0.75]
octaver1: mix = a; sub = 1; up = 1; tone = c; thresh = 0.05
eq1: type = peak; freq = 500; q = 0.9; gain = b`,
    opt(p, { a: p("Mix", 0.2, 0.8, 0.45), b: p("Scoop", -6, 0, -2.5), c: p("Tone", 120, 600, 280) }));

  add("Octave Into Dirt", "Pitch",
    "Octave then diode. Synth-bass / riff.",
    `param a = Mix [0.25, 0.85]
param b = Dirt [1, 5]
param c = Tone [900, 4000]
octaver1: mix = a; sub = 1; up = 0; tone = 180; thresh = 0.05
stage1: y = diode(x, b)
filter1: type = lowpass; cutoff = c; resonance = 0.35`,
    opt(p, { a: p("Mix", 0.25, 0.85, 0.5), b: p("Dirt", 1, 5, 2.2), c: p("Tone", 900, 4000, 2200) }));

  add("Octave Then Gate", "Pitch",
    "Octave plus a tight gate so muted notes die.",
    `param a = Mix [0.2, 0.75]
param b = Open [-48, -26]
param c = Hold [0.01, 0.08]
octaver1: mix = a; sub = 1; up = 0; tone = 170; thresh = 0.05
gate1: threshold = b; hyst = 5; hold = c; range = -72`,
    opt(p, { a: p("Mix", 0.2, 0.75, 0.42), b: p("Open", -48, -26, -36), c: p("Hold", 0.01, 0.08, 0.03) }));

  add("Harmony Bed Octave", "Pitch",
    "Quiet up-octave into a small hall. Pad / harmony.",
    `param a = Mix [0.1, 0.45]
param b = Hall [0.2, 0.55]
param c = Blend [0.1, 0.4]
octaver1: mix = a; sub = 0; up = 1; tone = 360; thresh = 0.05
reverb1: size = b; decay = 0.8; damp = 0.4; mix = c; width = 0.7`,
    opt(p, { a: p("Mix", 0.1, 0.45, 0.22), b: p("Hall", 0.2, 0.55, 0.35), c: p("Blend", 0.1, 0.4, 0.2) }));

  add("Tracking Octave Tight", "Pitch",
    "Higher track threshold, less flutter on busy parts.",
    `param a = Mix [0.2, 0.8]
param b = Track [0.02, 0.14]
param c = Tone [80, 400]
octaver1: mix = a; sub = 1; up = 0; tone = c; thresh = b
filter1: type = highpass; cutoff = 40; resonance = 0.2`,
    opt(p, { a: p("Mix", 0.2, 0.8, 0.48), b: p("Track", 0.02, 0.14, 0.07), c: p("Tone", 80, 400, 180) }));

  add("Vocoder Formant Shift", "Pitch",
    "Vocoder as a formant color, self if no sidechain. Voice / synth.",
    `param a = Formant [0.6, 1.6]
param b = Mix [0.2, 1]
param c = Q [0.8, 3.5]
vocoder1: bands = 16; formant = a; mix = b; q = c`,
    opt(p, { a: p("Formant", 0.6, 1.6, 1.05), b: p("Mix", 0.2, 1, 0.55), c: p("Q", 0.8, 3.5, 1.6) }));

  add("Vocoder Talk Box", "Pitch",
    "Narrower, more mouth. Sidechain voice recommended.",
    `param a = Formant [0.7, 1.8]
param b = Mix [0.3, 1]
param c = Tone [2000, 7000]
vocoder1: bands = 12; formant = a; mix = b
filter1: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Formant", 0.7, 1.8, 1.2), b: p("Mix", 0.3, 1, 0.7), c: p("Tone", 2000, 7000, 4500) }));

  add("Octave Shimmer Send", "Pitch",
    "Up-octave into a long verb on a send. Honest shimmer, not pitch-shift feedback.",
    `param a = Mix [0.15, 0.6]
param b = Hall [0.4, 0.85]
param c = Blend [0.15, 0.55]
stage1: y = x
bus shine:
  send: in = 1
  octaver1: mix = a; sub = 0; up = 1; tone = 400; thresh = 0.05
  reverb1: size = b; decay = 2.4; damp = 0.35; mix = 1; width = 0.85
out: main = 1-c; shine = c`,
    opt(p, { a: p("Mix", 0.15, 0.6, 0.32), b: p("Hall", 0.4, 0.85, 0.62), c: p("Blend", 0.15, 0.55, 0.28), mix: 0.3 }));

  add("Bass Synth Stack", "Pitch",
    "Sub octave + fold + LPF. Acid / reese cousin, not Acid Hash.",
    `param a = Sub [0.3, 0.9]
param b = Fold [0.15, 0.5]
param c = Cut [400, 2200]
octaver1: mix = a; sub = 1; up = 0; tone = 140; thresh = 0.05
stage1: y = fold(x, -b, b)
filter1: type = lowpass; cutoff = c; resonance = 0.55`,
    opt(p, { a: p("Sub", 0.3, 0.9, 0.55), b: p("Fold", 0.15, 0.5, 0.28), c: p("Cut", 400, 2200, 1100) }));
}

function registerDelay (add, p, opt)
{
  const delays = [
    ["Straight Eighth Pop", "1/8", 0.22, 4800, "Pop insert delay. Straight 1/8, darker than 2290."],
    ["Triplet Delay", "1/8t", 0.28, 4200, "Shuffle / swing throw. 1/8 triplet."],
    ["Sixteenth Sprinkle", "1/16", 0.12, 5500, "Tight 1/16 sprinkle. Rhythm guitar / hat."],
    ["Half Note Wash", "1/2", 0.35, 3800, "Slow wash. Ballad guitar / vocal."],
    ["Dotted Quarter Dub", "1/4.", 0.42, 3200, "Dub delay. Dotted 1/4, more feedback."],
  ];
  for (const [name, note, fb, damp, desc] of delays)
  {
    add(name, "Delay", desc,
      `param a = Feedback [0.08, 0.55]
param b = Tone [1800, 7000]
param c = Mix [0.12, 0.55]
delay1: time = ${note}; feedback = a; mix = c; damp = b`,
      opt(p, { a: p("Feedback", 0.08, 0.55, fb), b: p("Tone", 1800, 7000, damp), c: p("Mix", 0.12, 0.55, 0.28), mix: 0.32 }));
  }

  add("Dub Feedback Cave", "Delay",
    "High feedback, dark, then a tiny room. Dub send.",
    `param a = Feedback [0.35, 0.72]
param b = Cave [0.2, 0.55]
param c = Mix [0.15, 0.5]
filter1: type = highpass; cutoff = 180; resonance = 0.25
delay1: time = 1/4; feedback = a; mix = 1; damp = 2800
reverb1: size = b; decay = 0.6; damp = 0.55; mix = 0.25; width = 0.5
stage1: y = x * c`,
    opt(p, { a: p("Feedback", 0.35, 0.72, 0.52), b: p("Cave", 0.2, 0.55, 0.34), c: p("Mix", 0.15, 0.5, 0.3), mix: 0.3 }));

  add("Dark Bucket Brigade", "Delay",
    "BBD cousin: HPF, dark damp, slight diode. Not Memory Man.",
    `param a = Time [180, 520]
param b = Feedback [0.15, 0.5]
param c = Mix [0.15, 0.5]
filter1: type = highpass; cutoff = 220; resonance = 0.3
delay1: time = a; feedback = b; mix = c; damp = 2400
stage1: y = diode(x, 1.15)`,
    opt(p, { a: p("Time", 180, 520, 320), b: p("Feedback", 0.15, 0.5, 0.3), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Bright Digital Grid", "Delay",
    "Clean 1/8 grid, bright damp, no analog dirt.",
    `param a = Feedback [0.08, 0.4]
param b = Mix [0.12, 0.45]
param c = Air [0, 4]
delay1: time = 1/8; feedback = a; mix = b; damp = 9000
eq1: type = highshelf; freq = 7000; q = 0.5; gain = c`,
    opt(p, { a: p("Feedback", 0.08, 0.4, 0.18), b: p("Mix", 0.12, 0.45, 0.24), c: p("Air", 0, 4, 1.5), mix: 0.26 }));

  add("Multi Tap Stair", "Delay",
    "Two taps: 1/8 then 1/4. Stair echo.",
    `param a = Short [0.08, 0.35]
param b = Long [0.1, 0.4]
param c = Mix [0.15, 0.5]
delay1: time = 1/8; feedback = a; mix = c * 0.6; damp = 5000
delay2: time = 1/4; feedback = b; mix = c * 0.5; damp = 4000`,
    opt(p, { a: p("Short", 0.08, 0.35, 0.16), b: p("Long", 0.1, 0.4, 0.2), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Mid Only Echo", "Delay",
    "Delay on the mid only. Sides stay dry.",
    `param a = Time [1/8, 1/4]
param b = Feedback [0.12, 0.45]
param c = Mix [0.15, 0.5]
ms1: mode = encode
delay1: channel = mid; time = 1/8; feedback = b; mix = c; damp = 4500
ms2: mode = decode`,
    opt(p, { a: p("Time", 0, 1, 0.5), b: p("Feedback", 0.12, 0.45, 0.24), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Side Only Echo", "Delay",
    "Delay on the sides only. Center stays the dry word.",
    `param a = Feedback [0.12, 0.5]
param b = Mix [0.15, 0.55]
param c = Tone [2500, 7000]
ms1: mode = encode
delay1: channel = side; time = 1/8.; feedback = a; mix = b; damp = c
ms2: mode = decode`,
    opt(p, { a: p("Feedback", 0.12, 0.5, 0.26), b: p("Mix", 0.15, 0.55, 0.3), c: p("Tone", 2500, 7000, 4200), mix: 0.3 }));

  add("Filtered Throw", "Delay",
    "Band-limited throw. 300–3 kHz into the delay.",
    `param a = Feedback [0.12, 0.48]
param b = Mix [0.12, 0.5]
param c = Top [2000, 5000]
filter1: type = highpass; cutoff = 300; resonance = 0.25
filter2: type = lowpass; cutoff = c; resonance = 0.28
delay1: time = 1/8.; feedback = a; mix = b; damp = 4000`,
    opt(p, { a: p("Feedback", 0.12, 0.48, 0.24), b: p("Mix", 0.12, 0.5, 0.26), c: p("Top", 2000, 5000, 3200), mix: 0.28 }));

  add("Long Wash Delay", "Delay",
    "Long ms delay, low feedback, for pads.",
    `param a = Time [380, 900]
param b = Feedback [0.08, 0.32]
param c = Mix [0.15, 0.55]
delay1: time = a; feedback = b; mix = c; damp = 3600
reverb1: size = 0.3; decay = 0.5; damp = 0.45; mix = 0.12; width = 0.6`,
    opt(p, { a: p("Time", 380, 900, 560), b: p("Feedback", 0.08, 0.32, 0.16), c: p("Mix", 0.15, 0.55, 0.3), mix: 0.32 }));

  add("Slap Room Combo", "Delay",
    "90 ms slap then a booth. Guitar / vocal send.",
    `param a = Slap [70, 130]
param b = Room [0.12, 0.4]
param c = Mix [0.15, 0.5]
delay1: time = a; feedback = 0.07; mix = c; damp = 4800
reverb1: size = b; decay = 0.25; damp = 0.5; mix = 0.18; width = 0.45`,
    opt(p, { a: p("Slap", 70, 130, 92), b: p("Room", 0.12, 0.4, 0.22), c: p("Mix", 0.15, 0.5, 0.26), mix: 0.3 }));

  add("Swell Delay Rise", "Delay",
    "Env opens delay mix. Reverse-feel swell, not true reverse.",
    `param a = Rise [0.05, 0.3]
param b = Time [1/8, 1/4]
param c = Feedback [0.15, 0.45]
env1: attack = a; release = 0.2
delay1: time = 1/4; feedback = c; mix = 0.15 + env1 * 0.45; damp = 4000`,
    opt(p, { a: p("Rise", 0.05, 0.3, 0.12), b: p("Time", 0, 1, 0.5), c: p("Feedback", 0.15, 0.45, 0.26), mix: 0.4 }));

  add("Ping Tight", "Delay",
    "Short ping-pong. 1/16 L / 1/8 R feel via two times.",
    `param a = Feedback [0.1, 0.4]
param b = Mix [0.15, 0.5]
param c = Damp [3000, 8000]
delay1: time = 1/16; feedback = a; mix = b; damp = c; pingpong = true`,
    opt(p, { a: p("Feedback", 0.1, 0.4, 0.2), b: p("Mix", 0.15, 0.5, 0.28), c: p("Damp", 3000, 8000, 5200), mix: 0.3 }));

  add("Ping Wide Slow", "Delay",
    "Wide ping-pong on 1/4. Pads / vocals.",
    `param a = Feedback [0.15, 0.48]
param b = Mix [0.15, 0.5]
param c = Width [0.4, 1]
delay1: time = 1/4; feedback = a; mix = b; damp = 4500; pingpong = true
widen1: width = c; delay = 10; bass = 160`,
    opt(p, { a: p("Feedback", 0.15, 0.48, 0.28), b: p("Mix", 0.15, 0.5, 0.3), c: p("Width", 0.4, 1, 0.7), mix: 0.32 }));

  add("Saturation Echo", "Delay",
    "Tube into delay so repeats get dirtier.",
    `param a = Drive [0.8, 3.2]
param b = Feedback [0.15, 0.48]
param c = Mix [0.15, 0.5]
stage1: y = tube(x, a)
delay1: time = 1/8; feedback = b; mix = c; damp = 3800`,
    opt(p, { a: p("Drive", 0.8, 3.2, 1.5), b: p("Feedback", 0.15, 0.48, 0.28), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Gate Tail Delay", "Delay",
    "Delay then a gate so tails chop. Industrial / EBM.",
    `param a = Feedback [0.2, 0.55]
param b = Open [-42, -20]
param c = Mix [0.15, 0.5]
delay1: time = 1/8; feedback = a; mix = c; damp = 4000
gate1: threshold = b; hyst = 4; hold = 0.02; range = -60`,
    opt(p, { a: p("Feedback", 0.2, 0.55, 0.32), b: p("Open", -42, -20, -30), c: p("Mix", 0.15, 0.5, 0.3), mix: 0.32 }));

  add("Chorus Into Delay", "Delay",
    "Short chorus then 1/8 delay.",
    `param a = Rate [0.2, 1.8]
param b = Feedback [0.12, 0.42]
param c = Mix [0.15, 0.5]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 11 + osc1 * 6; feedback = 0.06; mix = 0.28; damp = 7000
delay2: time = 1/8; feedback = b; mix = c; damp = 5000`,
    opt(p, { a: p("Rate", 0.2, 1.8, 0.55), b: p("Feedback", 0.12, 0.42, 0.22), c: p("Mix", 0.15, 0.5, 0.26), mix: 0.3 }));

  add("Delay Into Plate", "Delay",
    "1/8 delay feeding a short plate.",
    `param a = Feedback [0.12, 0.4]
param b = Plate [0.15, 0.45]
param c = Mix [0.15, 0.5]
delay1: time = 1/8; feedback = a; mix = 0.35; damp = 4500
reverb1: size = b; decay = 0.4; damp = 0.38; mix = c; width = 0.65`,
    opt(p, { a: p("Feedback", 0.12, 0.4, 0.22), b: p("Plate", 0.15, 0.45, 0.26), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.32 }));

  add("Looper Pad Delay", "Delay",
    "Very long time, low feedback. Texture bed, not a rhythm delay.",
    `param a = Time [700, 1400]
param b = Feedback [0.05, 0.22]
param c = Mix [0.2, 0.6]
delay1: time = a; feedback = b; mix = c; damp = 3000
filter1: type = lowpass; cutoff = 5000; resonance = 0.25`,
    opt(p, { a: p("Time", 700, 1400, 980), b: p("Feedback", 0.05, 0.22, 0.12), c: p("Mix", 0.2, 0.6, 0.35), mix: 0.38 }));

  add("Haas Pre Delay", "Delay",
    "12–28 ms only. Width, not an echo.",
    `param a = Offset [10, 28]
param b = Mix [0.2, 0.7]
param c = Bass [80, 200]
widen1: width = b; delay = a; bass = c`,
    opt(p, { a: p("Offset", 10, 28, 16), b: p("Mix", 0.2, 0.7, 0.45), c: p("Bass", 80, 200, 130) }));
}

function registerReverb (add, p, opt)
{
  add("Small Booth", "Reverb",
    "Tight vocal booth. Insert or short send.",
    `param a = Size [0.08, 0.28]
param b = Decay [0.08, 0.28]
param c = Mix [0.08, 0.4]
reverb1: size = a; decay = b; damp = 0.55; mix = c; width = 0.35`,
    opt(p, { a: p("Size", 0.08, 0.28, 0.14), b: p("Decay", 0.08, 0.28, 0.14), c: p("Mix", 0.08, 0.4, 0.18), mix: 0.22 }));

  add("Drum Room Close", "Reverb",
    "Close kit room. Faster than Drum Chamber.",
    `param a = Size [0.16, 0.4]
param b = Decay [0.12, 0.4]
param c = Mix [0.1, 0.4]
filter1: type = highpass; cutoff = 140; resonance = 0.25
reverb1: size = a; decay = b; damp = 0.48; mix = c; width = 0.55`,
    opt(p, { a: p("Size", 0.16, 0.4, 0.24), b: p("Decay", 0.12, 0.4, 0.22), c: p("Mix", 0.1, 0.4, 0.2), mix: 0.24 }));

  add("Vocal Plate Short", "Reverb",
    "Bright short plate. Lead vocal send.",
    `param a = Size [0.12, 0.35]
param b = Decay [0.15, 0.45]
param c = Mix [0.1, 0.45]
filter1: type = highpass; cutoff = 200; resonance = 0.22
reverb1: size = a; decay = b; damp = 0.32; mix = c; width = 0.7`,
    opt(p, { a: p("Size", 0.12, 0.35, 0.2), b: p("Decay", 0.15, 0.45, 0.26), c: p("Mix", 0.1, 0.45, 0.22), mix: 0.26 }));

  add("Gated Eighties", "Reverb",
    "Big room then a hard gate. Snare / 80s tom.",
    `param a = Size [0.3, 0.65]
param b = Open [-28, -8]
param c = Mix [0.15, 0.55]
reverb1: size = a; decay = 0.55; damp = 0.4; mix = 1; width = 0.6
gate1: threshold = b; hyst = 3; hold = 0.04; range = -70
stage1: y = x`,
    opt(p, { a: p("Size", 0.3, 0.65, 0.45), b: p("Open", -28, -8, -16), c: p("Mix", 0.15, 0.55, 0.32), mix: 0.35 }));

  add("Cathedral Tail", "Reverb",
    "Very large, dark, slow. Score / organ.",
    `param a = Size [0.55, 0.92]
param b = Decay [1.4, 3.5]
param c = Mix [0.12, 0.45]
filter1: type = highpass; cutoff = 160; resonance = 0.2
reverb1: size = a; decay = b; damp = 0.58; mix = c; width = 0.88`,
    opt(p, { a: p("Size", 0.55, 0.92, 0.74), b: p("Decay", 1.4, 3.5, 2.2), c: p("Mix", 0.12, 0.45, 0.24), mix: 0.28 }));

  add("Chamber Wood", "Reverb",
    "Wooden chamber: mid-small, warmer damp than Studio Room.",
    `param a = Size [0.2, 0.48]
param b = Decay [0.25, 0.7]
param c = Mix [0.1, 0.4]
reverb1: size = a; decay = b; damp = 0.62; mix = c; width = 0.5`,
    opt(p, { a: p("Size", 0.2, 0.48, 0.32), b: p("Decay", 0.25, 0.7, 0.4), c: p("Mix", 0.1, 0.4, 0.2), mix: 0.24 }));

  add("Shimmer Verb Send", "Reverb",
    "Hall on a send with an up-octave in the wet only.",
    `param a = Size [0.4, 0.8]
param b = Octave [0.15, 0.55]
param c = Mix [0.12, 0.45]
stage1: y = x
bus shine:
  send: in = 1
  octaver1: mix = b; sub = 0; up = 1; tone = 380; thresh = 0.05
  reverb1: size = a; decay = 2.0; damp = 0.36; mix = 1; width = 0.82
out: main = 1-c; shine = c`,
    opt(p, { a: p("Size", 0.4, 0.8, 0.58), b: p("Octave", 0.15, 0.55, 0.3), c: p("Mix", 0.12, 0.45, 0.24), mix: 0.26 }));

  add("Snare Plate Hit", "Reverb",
    "Plate with a 200 Hz cut so snares do not boom.",
    `param a = Size [0.18, 0.45]
param b = Decay [0.2, 0.6]
param c = Mix [0.12, 0.45]
filter1: type = highpass; cutoff = 200; resonance = 0.28
reverb1: size = a; decay = b; damp = 0.34; mix = c; width = 0.68`,
    opt(p, { a: p("Size", 0.18, 0.45, 0.28), b: p("Decay", 0.2, 0.6, 0.34), c: p("Mix", 0.12, 0.45, 0.22), mix: 0.26 }));

  add("Dark Hall Score", "Reverb",
    "Darker than Concert Hall. Low-passed tail.",
    `param a = Size [0.4, 0.8]
param b = Decay [0.8, 2.4]
param c = Mix [0.12, 0.42]
reverb1: size = a; decay = b; damp = 0.62; mix = c; width = 0.78
filter1: type = lowpass; cutoff = 4500; resonance = 0.22`,
    opt(p, { a: p("Size", 0.4, 0.8, 0.58), b: p("Decay", 0.8, 2.4, 1.4), c: p("Mix", 0.12, 0.42, 0.24), mix: 0.28 }));

  add("Bright Tile Room", "Reverb",
    "Hard tiles. Fast, bright, small.",
    `param a = Size [0.12, 0.32]
param b = Decay [0.1, 0.32]
param c = Mix [0.1, 0.4]
reverb1: size = a; decay = b; damp = 0.22; mix = c; width = 0.55`,
    opt(p, { a: p("Size", 0.12, 0.32, 0.2), b: p("Decay", 0.1, 0.32, 0.18), c: p("Mix", 0.1, 0.4, 0.2), mix: 0.24 }));

  add("Reverse Feel Bloom", "Reverb",
    "Env opens the verb. Swell / bloom, not a reverse IR.",
    `param a = Rise [0.06, 0.35]
param b = Size [0.3, 0.7]
param c = Mix [0.2, 0.6]
env1: attack = a; release = 0.25
reverb1: size = b; decay = 1.2; damp = 0.4; mix = 0.15 + env1 * c; width = 0.7`,
    opt(p, { a: p("Rise", 0.06, 0.35, 0.14), b: p("Size", 0.3, 0.7, 0.48), c: p("Mix", 0.2, 0.6, 0.35) }));

  add("Mono Room Center", "Reverb",
    "Narrow room. Width 0.15 so the center stays solid.",
    `param a = Size [0.14, 0.36]
param b = Decay [0.12, 0.4]
param c = Mix [0.1, 0.38]
reverb1: size = a; decay = b; damp = 0.5; mix = c; width = 0.15`,
    opt(p, { a: p("Size", 0.14, 0.36, 0.22), b: p("Decay", 0.12, 0.4, 0.2), c: p("Mix", 0.1, 0.38, 0.18), mix: 0.22 }));

  add("Wide Score Cloud", "Reverb",
    "Very wide, long, light damp. Pads / strings.",
    `param a = Size [0.45, 0.88]
param b = Decay [1.2, 3]
param c = Mix [0.15, 0.5]
widen1: width = 0.55; delay = 12; bass = 180
reverb1: size = a; decay = b; damp = 0.38; mix = c; width = 0.92`,
    opt(p, { a: p("Size", 0.45, 0.88, 0.66), b: p("Decay", 1.2, 3, 1.8), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.32 }));

  add("Predelay Hall", "Reverb",
    "Short delay then a hall so words stay dry first.",
    `param a = Pre [20, 80]
param b = Size [0.35, 0.7]
param c = Mix [0.12, 0.42]
delay1: time = a; feedback = 0.02; mix = 0.2; damp = 6000
reverb1: size = b; decay = 1.1; damp = 0.42; mix = c; width = 0.75`,
    opt(p, { a: p("Pre", 20, 80, 38), b: p("Size", 0.35, 0.7, 0.5), c: p("Mix", 0.12, 0.42, 0.24), mix: 0.28 }));

  add("Spring Splash Verb", "Reverb",
    "Splashier spring than Spring Tank. More drip.",
    `param a = Drip [0.22, 0.58]
param b = Decay [0.2, 0.55]
param c = Mix [0.12, 0.5]
filter1: type = highpass; cutoff = 180; resonance = 0.3
reverb1: size = a; decay = b; damp = 0.24; mix = c; width = 0.32`,
    opt(p, { a: p("Drip", 0.22, 0.58, 0.36), b: p("Decay", 0.2, 0.55, 0.32), c: p("Mix", 0.12, 0.5, 0.26), mix: 0.28 }));

  add("Nonlin Drum Snap", "Reverb",
    "Short non-linear burst. Snare / clap. Not AMS stock.",
    `param a = Size [0.1, 0.3]
param b = Decay [0.06, 0.22]
param c = Mix [0.12, 0.45]
reverb1: size = a; decay = b; damp = 0.3; mix = c; width = 0.4
gate1: threshold = -18; hyst = 3; hold = 0.03; range = -65`,
    opt(p, { a: p("Size", 0.1, 0.3, 0.18), b: p("Decay", 0.06, 0.22, 0.12), c: p("Mix", 0.12, 0.45, 0.24), mix: 0.28 }));

  add("Side Hall Only", "Reverb",
    "Hall on the sides. Mid dry.",
    `param a = Size [0.3, 0.7]
param b = Decay [0.5, 1.6]
param c = Mix [0.15, 0.5]
ms1: mode = encode
reverb1: channel = side; size = a; decay = b; damp = 0.42; mix = c; width = 0.8
ms2: mode = decode`,
    opt(p, { a: p("Size", 0.3, 0.7, 0.48), b: p("Decay", 0.5, 1.6, 0.9), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Mid Hall Glue", "Reverb",
    "Tiny hall on the mid only. Mix bus glue.",
    `param a = Size [0.12, 0.32]
param b = Decay [0.12, 0.4]
param c = Mix [0.08, 0.3]
ms1: mode = encode
reverb1: channel = mid; size = a; decay = b; damp = 0.5; mix = c; width = 0.2
ms2: mode = decode`,
    opt(p, { a: p("Size", 0.12, 0.32, 0.2), b: p("Decay", 0.12, 0.4, 0.22), c: p("Mix", 0.08, 0.3, 0.14) }));

  add("Plate Into Chorus", "Reverb",
    "Plate then a slow chorus on the wet.",
    `param a = Size [0.2, 0.5]
param b = Rate [0.15, 1.2]
param c = Mix [0.12, 0.45]
reverb1: size = a; decay = 0.5; damp = 0.36; mix = 0.4; width = 0.65
osc1: shape = sine; freq = b; depth = 1
delay1: time = 10 + osc1 * 5; feedback = 0.05; mix = c; damp = 7000`,
    opt(p, { a: p("Size", 0.2, 0.5, 0.32), b: p("Rate", 0.15, 1.2, 0.4), c: p("Mix", 0.12, 0.45, 0.22), mix: 0.3 }));

  add("Abyss Low Verb", "Reverb",
    "Low-passed abyss. Drones / bass pads.",
    `param a = Size [0.45, 0.85]
param b = Decay [1, 2.8]
param c = Top [800, 2800]
reverb1: size = a; decay = b; damp = 0.7; mix = 0.4; width = 0.7
filter1: type = lowpass; cutoff = c; resonance = 0.25`,
    opt(p, { a: p("Size", 0.45, 0.85, 0.64), b: p("Decay", 1, 2.8, 1.6), c: p("Top", 800, 2800, 1600), mix: 0.35 }));

  add("Air Verb Only", "Reverb",
    "Verb only above 4 kHz. Silk, no mud.",
    `param a = Size [0.2, 0.55]
param b = Decay [0.3, 1.1]
param c = Mix [0.1, 0.4]
xover1: f1 = 4000
bus high:
  reverb1: size = a; decay = b; damp = 0.3; mix = c; width = 0.75
out: low = 1; high = 1`,
    opt(p, { a: p("Size", 0.2, 0.55, 0.34), b: p("Decay", 0.3, 1.1, 0.55), c: p("Mix", 0.1, 0.4, 0.22), mix: 0.26 }));

  add("Concert Seat Near", "Reverb",
    "Near-seat hall: more early, less tail than Concert Hall.",
    `param a = Size [0.28, 0.55]
param b = Decay [0.35, 1]
param c = Mix [0.1, 0.38]
reverb1: size = a; decay = b; damp = 0.44; mix = c; width = 0.62`,
    opt(p, { a: p("Size", 0.28, 0.55, 0.4), b: p("Decay", 0.35, 1, 0.55), c: p("Mix", 0.1, 0.38, 0.2), mix: 0.24 }));
}

function registerDynamics (add, p, opt)
{
  add("Slow Opto Ride", "Dynamics",
    "Slow leveler. Vocal / bass / pad. Mix 100.",
    `param a = Pull [4, 14]
param b = Makeup [0, 8]
param c = Air [0, 3]
comp1: threshold = -a; ratio = 2.5; attack = 0.02; release = 0.32; knee = 12; makeup = b
eq1: type = highshelf; freq = 9000; q = 0.5; gain = c`,
    opt(p, { a: p("Pull", 4, 14, 8), b: p("Makeup", 0, 8, 2), c: p("Air", 0, 3, 1) }));

  add("Fast FET Catch", "Dynamics",
    "Fast peak catch. Drums / vocal / DI.",
    `param a = Drive [3, 12]
param b = Attack [0.0002, 0.006]
param c = Release [0.03, 0.16]
comp1: threshold = -a; ratio = 12; attack = b; release = c; knee = 1; makeup = 3`,
    opt(p, { a: p("Drive", 3, 12, 6.5), b: p("Attack", 0.0002, 0.006, 0.0008), c: p("Release", 0.03, 0.16, 0.07) }));

  add("All Buttons Smash", "Dynamics",
    "Aggressive FET, high ratio. Parallel drum / vocal.",
    `param a = Smash [6, 16]
param b = Makeup [2, 12]
param c = Tone [2500, 7000]
filter1: type = highpass; cutoff = 80; resonance = 0.25
comp1: threshold = -a; ratio = 20; attack = 0.0003; release = 0.05; knee = 0; makeup = b
filter2: type = lowpass; cutoff = c; resonance = 0.28`,
    opt(p, { a: p("Smash", 6, 16, 10), b: p("Makeup", 2, 12, 6), c: p("Tone", 2500, 7000, 4500) }));

  add("Bus Glue Light", "Dynamics",
    "Gentle VCA-style mixbus. 2:1, auto-ish.",
    `param a = Glue [4, 12]
param b = Attack [0.008, 0.04]
param c = Makeup [0, 4]
comp1: threshold = -a; ratio = 2; attack = b; release = 0.18; knee = 6; makeup = c; hpf = 90`,
    opt(p, { a: p("Glue", 4, 12, 7), b: p("Attack", 0.008, 0.04, 0.018), c: p("Makeup", 0, 4, 1.2) }));

  add("NY Parallel Smash", "Dynamics",
    "Dry plus a smashed bus. Drums / vocal.",
    `param a = Smash [5, 16]
param b = Blend [0.12, 0.55]
param c = HPF [60, 200]
stage1: y = x
bus smash:
  send: in = 1
  filter1: type = highpass; cutoff = c; resonance = 0.3
  comp1: threshold = -a; ratio = 12; attack = 0.0004; release = 0.07; makeup = 8
out: main = 1-b; smash = b`,
    opt(p, { a: p("Smash", 5, 16, 9), b: p("Blend", 0.12, 0.55, 0.3), c: p("HPF", 60, 200, 110) }));

  add("Sidechain Pump Bus", "Dynamics",
    "Mixbus / pad ducks from host sidechain.",
    `param a = Depth [0.25, 1]
param b = Release [0.06, 0.4]
param c = Floor [-28, -8]
sidechain1: mix = a
comp1: threshold = c; ratio = 8; attack = 0.001; release = b; source = sidechain; makeup = 0`,
    opt(p, { a: p("Depth", 0.25, 1, 0.7), b: p("Release", 0.06, 0.4, 0.16), c: p("Floor", -28, -8, -16) }));

  add("De-Ess Wide", "Dynamics",
    "Wider de-ess than the vocal one. Podcast / VO.",
    `param a = Ess [3, 14]
param b = BandHz [2800, 6500]
param c = Range [4, 12]
xover1: f1 = b; f2 = 10000
bus high:
  comp1: threshold = -a; ratio = c; attack = 0.0005; release = 0.04
out: low = 1; mid = 1; high = 1`,
    opt(p, { a: p("Ess", 3, 14, 7), b: p("BandHz", 2800, 6500, 4000), c: p("Range", 4, 12, 7) }));

  add("OTT Insert Lift", "Dynamics",
    "OTT on the insert, lighter than OTT Smash.",
    `param a = Depth [0.2, 0.8]
param b = Time [0.008, 0.05]
param c = Mix [0.3, 1]
ott1: depth = a; time = b; mix = c`,
    opt(p, { a: p("Depth", 0.2, 0.8, 0.42), b: p("Time", 0.008, 0.05, 0.018), c: p("Mix", 0.3, 1, 0.62) }));

  add("OTT Send Smash", "Dynamics",
    "OTT on a send bus so dry stays.",
    `param a = Depth [0.3, 1]
param b = Blend [0.15, 0.6]
param c = In [0.5, 2]
stage1: y = x
bus lift:
  send: in = 1
  ott1: depth = a; in = c; mix = 1
out: main = 1-b; lift = b`,
    opt(p, { a: p("Depth", 0.3, 1, 0.55), b: p("Blend", 0.15, 0.6, 0.32), c: p("In", 0.5, 2, 1) }));

  add("Peak Limit Polish", "Dynamics",
    "Transparent limiter. Last insert.",
    `param a = Ceiling [-3, -0.1]
param b = Release [0.04, 0.2]
param c = In [0.8, 1.4]
stage1: y = x * c
limit1: ceiling = a; release = b`,
    opt(p, { a: p("Ceiling", -3, -0.1, -0.5), b: p("Release", 0.04, 0.2, 0.09), c: p("In", 0.8, 1.4, 1) }));

  add("Drum FET Fast", "Dynamics",
    "Snare / kit FET. Faster than 1176 stock.",
    `param a = Hit [4, 14]
param b = Attack [0.0002, 0.004]
param c = Click [0, 4]
comp1: threshold = -a; ratio = 8; attack = b; release = 0.06; makeup = 3
eq1: type = peak; freq = 4500; q = 1.2; gain = c`,
    opt(p, { a: p("Hit", 4, 14, 8), b: p("Attack", 0.0002, 0.004, 0.0006), c: p("Click", 0, 4, 1.6) }));

  add("Vocal Opto Soft", "Dynamics",
    "Softer than LA-2A stock. Low ratio, long release.",
    `param a = Pull [3, 12]
param b = Release [0.15, 0.5]
param c = Makeup [0, 6]
comp1: threshold = -a; ratio = 2; attack = 0.018; release = b; knee = 14; makeup = c`,
    opt(p, { a: p("Pull", 3, 12, 6.5), b: p("Release", 0.15, 0.5, 0.28), c: p("Makeup", 0, 6, 1.8) }));

  add("Vari-Mu Bus Soft", "Dynamics",
    "Soft knee bus, slower than Fairchild stock.",
    `param a = Pull [4, 12]
param b = Attack [0.01, 0.05]
param c = Makeup [0, 5]
comp1: threshold = -a; ratio = 2.5; attack = b; release = 0.35; knee = 16; makeup = c`,
    opt(p, { a: p("Pull", 4, 12, 7), b: p("Attack", 0.01, 0.05, 0.022), c: p("Makeup", 0, 5, 1.5) }));

  add("Diode Bridge Punch", "Dynamics",
    "Punchy diode-bridge style + a little even harmonic.",
    `param a = Pull [4, 13]
param b = Harm [0.5, 2]
param c = Makeup [1, 7]
stage1: y = diode(x, b)
comp1: threshold = -a; ratio = 4; attack = 0.004; release = 0.1; knee = 3; makeup = c`,
    opt(p, { a: p("Pull", 4, 13, 7.5), b: p("Harm", 0.5, 2, 1.05), c: p("Makeup", 1, 7, 3) }));

  add("HPF Comp Kick", "Dynamics",
    "Comp ignores the sub via HPF detect. Kick / bass.",
    `param a = Pull [5, 14]
param b = Detect [60, 180]
param c = Makeup [1, 6]
comp1: threshold = -a; ratio = 4; attack = 0.008; release = 0.12; hpf = b; makeup = c`,
    opt(p, { a: p("Pull", 5, 14, 8), b: p("Detect", 60, 180, 100), c: p("Makeup", 1, 6, 2.5) }));

  add("Upward Soft Lift", "Dynamics",
    "Low-level lift via OTT-ish light depth. Quiet sources.",
    `param a = Lift [0.15, 0.55]
param b = Time [0.015, 0.08]
param c = Mix [0.25, 0.85]
ott1: depth = a; time = b; mix = c; in = 1`,
    opt(p, { a: p("Lift", 0.15, 0.55, 0.32), b: p("Time", 0.015, 0.08, 0.03), c: p("Mix", 0.25, 0.85, 0.5) }));

  add("Downward Only Crush", "Dynamics",
    "OTT downward only. Tames peaks without lifting noise.",
    `param a = Crush [0.2, 0.8]
param b = Time [0.008, 0.04]
param c = Mix [0.3, 1]
ott1: depth = a; time = b; mix = c; in = 0.9`,
    opt(p, { a: p("Crush", 0.2, 0.8, 0.45), b: p("Time", 0.008, 0.04, 0.016), c: p("Mix", 0.3, 1, 0.7) }));

  add("Serial 76 Into 2A", "Dynamics",
    "FET then opto on one insert. Any source.",
    `param a = Peak [3, 12]
param b = Ride [4, 12]
param c = Makeup [0, 5]
comp1: threshold = -a; ratio = 8; attack = 0.0008; release = 0.07; knee = 1; makeup = 2
comp2: threshold = -b; ratio = 3; attack = 0.016; release = 0.26; knee = 10; makeup = c`,
    opt(p, { a: p("Peak", 3, 12, 6), b: p("Ride", 4, 12, 7), c: p("Makeup", 0, 5, 1.5) }));

  add("Mixbus Soft Clip", "Dynamics",
    "Glue then a soft ceiling. Mix 100.",
    `param a = Glue [4, 11]
param b = Ceiling [0.55, 0.95]
param c = Drive [0.8, 1.6]
comp1: threshold = -a; ratio = 2.2; attack = 0.015; release = 0.2; knee = 8; makeup = 1
stage1: y = hardclip(softclip(x, c), b)`,
    opt(p, { a: p("Glue", 4, 11, 6.5), b: p("Ceiling", 0.55, 0.95, 0.82), c: p("Drive", 0.8, 1.6, 1.05) }));

  add("Transient Snap Dyn", "Dynamics",
    "More attack than Envelope Shaper stock. Drums.",
    `param a = Attack [0.1, 1]
param b = Fast [0.004, 0.03]
param c = Mix [0.3, 1]
env1: type = peak; attack = 0.001; release = b
stage1: y = lerp(x, x * clamp(1 + a * env1, 0.2, 3.2), c)`,
    opt(p, { a: p("Attack", 0.1, 1, 0.45), b: p("Fast", 0.004, 0.03, 0.01), c: p("Mix", 0.3, 1, 0.7) }));

  add("Body Sustain Dyn", "Dynamics",
    "Lifts the body, not the click. Opposite of Transient Snap.",
    `param a = Body [0.1, 0.9]
param b = Slow [0.08, 0.35]
param c = Mix [0.3, 1]
env1: type = rms; attack = 0.02; release = b
stage1: y = lerp(x, x * clamp(1 + a * env1, 0.25, 2.6), c)`,
    opt(p, { a: p("Body", 0.1, 0.9, 0.4), b: p("Slow", 0.08, 0.35, 0.16), c: p("Mix", 0.3, 1, 0.65) }));

  add("Noise Gate Tight", "Dynamics",
    "Instrument gate. Guitar / snare.",
    `param a = Open [-52, -24]
param b = Hold [0.01, 0.1]
param c = Range [-80, -20]
gate1: threshold = a; hyst = 5; hold = b; range = c`,
    opt(p, { a: p("Open", -52, -24, -38), b: p("Hold", 0.01, 0.1, 0.03), c: p("Range", -80, -20, -64) }));

  add("Expander Room", "Dynamics",
    "Soft gate as expander. Room mics.",
    `param a = Open [-48, -22]
param b = Range [-24, -6]
param c = Rel [0.05, 0.25]
gate1: threshold = a; hyst = 8; hold = c; range = b`,
    opt(p, { a: p("Open", -48, -22, -34), b: p("Range", -24, -6, -14), c: p("Rel", 0.05, 0.25, 0.12) }));

  add("Lookahead-ish Limit", "Dynamics",
    "Softclip into a limiter. Safer than a naked brickwall.",
    `param a = Ceiling [-2, -0.15]
param b = Soft [0.7, 1.4]
param c = Rel [0.05, 0.18]
stage1: y = softclip(x, b)
limit1: ceiling = a; release = c`,
    opt(p, { a: p("Ceiling", -2, -0.15, -0.4), b: p("Soft", 0.7, 1.4, 1.0), c: p("Rel", 0.05, 0.18, 0.09) }));
}

function registerDrums (add, p, opt)
{
  add("Snare Crack Desk", "Drums",
    "Snare insert: 180 HPF, 3.5 kHz crack, fast FET.",
    `param a = Crack [1, 7]
param b = Hit [4, 13]
param c = Body [0, 4]
filter1: type = highpass; cutoff = 180; resonance = 0.28
eq1: type = peak; freq = 3500; q = 1.3; gain = a
eq2: type = peak; freq = 220; q = 0.9; gain = c
comp1: threshold = -b; ratio = 6; attack = 0.0006; release = 0.08; makeup = 2`,
    opt(p, { a: p("Crack", 1, 7, 3.4), b: p("Hit", 4, 13, 7.5), c: p("Body", 0, 4, 1.4) }));

  add("Hat Air Desk", "Drums",
    "Hats / ride. HPF 400, air shelf, tiny crush.",
    `param a = Air [1, 6]
param b = Cut [250, 700]
param c = Grit [0.4, 1.6]
filter1: type = highpass; cutoff = b; resonance = 0.25
eq1: type = highshelf; freq = 9000; q = 0.5; gain = a
stage1: y = diode(x, c)`,
    opt(p, { a: p("Air", 1, 6, 2.8), b: p("Cut", 250, 700, 420), c: p("Grit", 0.4, 1.6, 0.75) }));

  add("Kick Click Desk", "Drums",
    "Kick click without rumble. 3 kHz tap, 40 Hz HPF.",
    `param a = Click [0, 6]
param b = Thump [0, 5]
param c = Tight [30, 70]
filter1: type = highpass; cutoff = c; resonance = 0.22
eq1: type = peak; freq = 55; q = 1.1; gain = b
eq2: type = peak; freq = 3200; q = 1.4; gain = a`,
    opt(p, { a: p("Click", 0, 6, 2.6), b: p("Thump", 0, 5, 2.2), c: p("Tight", 30, 70, 42) }));

  add("808 Sub Kick", "Drums",
    "Sine-like 808: tiny HPF, tube, LPF 120.",
    `param a = Round [0.5, 2.2]
param b = Sub [22, 50]
param c = Ceiling [0.45, 0.95]
filter1: type = highpass; cutoff = b; resonance = 0.18
stage1: y = tube(x, a)
stage2: y = hardclip(softclip(y, 1.05), c)
filter2: type = lowpass; cutoff = 140; resonance = 0.22`,
    opt(p, { a: p("Round", 0.5, 2.2, 1.05), b: p("Sub", 22, 50, 30), c: p("Ceiling", 0.45, 0.95, 0.75) }));

  add("Parallel Crush Kit", "Drums",
    "Kit bus: dry plus crushed. Not Room Crush.",
    `param a = Crush [3, 12]
param b = Blend [0.12, 0.5]
param c = Tone [2500, 7000]
stage1: y = x
bus crush:
  send: in = 1
  filter1: type = highpass; cutoff = 90; resonance = 0.28
  stage2: y = hardclip(softclip(x, a * 0.35), 0.5)
  filter2: type = lowpass; cutoff = c; resonance = 0.3
out: main = 1-b; crush = b`,
    opt(p, { a: p("Crush", 3, 12, 6.5), b: p("Blend", 0.12, 0.5, 0.26), c: p("Tone", 2500, 7000, 4200) }));

  add("Gated Room Snare", "Drums",
    "Snare send: room then gate.",
    `param a = Size [0.22, 0.55]
param b = Open [-24, -8]
param c = Mix [0.15, 0.5]
stage1: y = x
bus room:
  send: in = 1
  reverb1: size = a; decay = 0.4; damp = 0.42; mix = 1; width = 0.55
  gate1: threshold = b; hyst = 3; hold = 0.035; range = -70
out: main = 1-c; room = c`,
    opt(p, { a: p("Size", 0.22, 0.55, 0.36), b: p("Open", -24, -8, -14), c: p("Mix", 0.15, 0.5, 0.28), mix: 0.3 }));

  add("Tom Body Desk", "Drums",
    "Toms: 80 HPF, 180 body, slow comp.",
    `param a = Body [1, 6]
param b = Glue [3, 10]
param c = Cut [50, 110]
filter1: type = highpass; cutoff = c; resonance = 0.25
eq1: type = peak; freq = 180; q = 0.9; gain = a
comp1: threshold = -b; ratio = 3; attack = 0.012; release = 0.16; makeup = 2`,
    opt(p, { a: p("Body", 1, 6, 3), b: p("Glue", 3, 10, 5.5), c: p("Cut", 50, 110, 75) }));

  add("Overhead Glue", "Drums",
    "OH pair: gentle glue, high shelf, small room.",
    `param a = Glue [3, 9]
param b = Air [0, 4]
param c = Room [0.08, 0.28]
comp1: threshold = -a; ratio = 2.5; attack = 0.01; release = 0.14; makeup = 1
eq1: type = highshelf; freq = 10000; q = 0.5; gain = b
reverb1: size = 0.2; decay = c; damp = 0.4; mix = 0.12; width = 0.7`,
    opt(p, { a: p("Glue", 3, 9, 5), b: p("Air", 0, 4, 1.6), c: p("Room", 0.08, 0.28, 0.14) }));

  add("Room Mic Smash", "Drums",
    "Room mics: HPF, smash, dark.",
    `param a = Smash [6, 16]
param b = Dark [2000, 5500]
param c = HPF [80, 220]
filter1: type = highpass; cutoff = c; resonance = 0.3
comp1: threshold = -a; ratio = 12; attack = 0.0005; release = 0.09; makeup = 7
filter2: type = lowpass; cutoff = b; resonance = 0.28`,
    opt(p, { a: p("Smash", 6, 16, 10), b: p("Dark", 2000, 5500, 3600), c: p("HPF", 80, 220, 130) }));

  add("Clap Plate Desk", "Drums",
    "Clap / snap: short plate, 300 HPF.",
    `param a = Size [0.12, 0.35]
param b = Mix [0.12, 0.45]
param c = Cut [180, 400]
filter1: type = highpass; cutoff = c; resonance = 0.28
reverb1: size = a; decay = 0.28; damp = 0.32; mix = b; width = 0.6`,
    opt(p, { a: p("Size", 0.12, 0.35, 0.2), b: p("Mix", 0.12, 0.45, 0.24), c: p("Cut", 180, 400, 260) }));

  add("Kick Sidechain Kit", "Drums",
    "Kit bus ducks from host sidechain (kick).",
    `param a = Depth [0.2, 0.85]
param b = Release [0.05, 0.25]
param c = Floor [-24, -8]
sidechain1: mix = a
comp1: threshold = c; ratio = 6; attack = 0.001; release = b; source = sidechain`,
    opt(p, { a: p("Depth", 0.2, 0.85, 0.5), b: p("Release", 0.05, 0.25, 0.12), c: p("Floor", -24, -8, -14) }));

  add("Transient Kit Snap", "Drums",
    "Whole kit transient. Faster env than Envelope Shaper.",
    `param a = Snap [0.15, 1]
param b = Fast [0.004, 0.025]
param c = Mix [0.3, 1]
env1: type = peak; attack = 0.001; release = b
stage1: y = lerp(x, x * clamp(1 + a * env1, 0.2, 3), c)`,
    opt(p, { a: p("Snap", 0.15, 1, 0.45), b: p("Fast", 0.004, 0.025, 0.009), c: p("Mix", 0.3, 1, 0.7) }));

  add("Lo-Fi Kit Crush", "Drums",
    "Kit: bitcrush + LPF. Breakbeat / sample.",
    `param a = Bits [4, 10]
param b = Tone [1800, 7000]
param c = Mix [0.25, 1]
stage1: y = lerp(x, bitcrush(softclip(x, 1.3), a), c)
filter1: type = lowpass; cutoff = b; resonance = 0.35`,
    opt(p, { a: p("Bits", 4, 10, 7), b: p("Tone", 1800, 7000, 3800), c: p("Mix", 0.25, 1, 0.55) }));

  add("Cymbal Splash Verb", "Drums",
    "Crash / splash: bright plate, 500 HPF.",
    `param a = Size [0.2, 0.55]
param b = Mix [0.1, 0.4]
param c = Cut [300, 800]
filter1: type = highpass; cutoff = c; resonance = 0.22
reverb1: size = a; decay = 0.7; damp = 0.28; mix = b; width = 0.8`,
    opt(p, { a: p("Size", 0.2, 0.55, 0.34), b: p("Mix", 0.1, 0.4, 0.2), c: p("Cut", 300, 800, 480) }));

  add("Rimshot Crack", "Drums",
    "Rimshot: 4 kHz spike, very fast FET.",
    `param a = Spike [1, 8]
param b = Hit [4, 14]
param c = Cut [250, 600]
filter1: type = highpass; cutoff = c; resonance = 0.3
eq1: type = peak; freq = 4200; q = 1.6; gain = a
comp1: threshold = -b; ratio = 10; attack = 0.0003; release = 0.05; makeup = 2`,
    opt(p, { a: p("Spike", 1, 8, 3.8), b: p("Hit", 4, 14, 8), c: p("Cut", 250, 600, 360) }));

  add("Perc Shaker Air", "Drums",
    "Shaker / tamb: 1 kHz HPF, air, no low.",
    `param a = Air [1, 6]
param b = Cut [700, 1600]
param c = Grit [0.3, 1.4]
filter1: type = highpass; cutoff = b; resonance = 0.22
eq1: type = highshelf; freq = 8000; q = 0.5; gain = a
stage1: y = softclip(x, c)`,
    opt(p, { a: p("Air", 1, 6, 2.6), b: p("Cut", 700, 1600, 1100), c: p("Grit", 0.3, 1.4, 0.7) }));

  add("Live Kit Glue", "Drums",
    "Full live kit bus. SSL-ish + tiny room.",
    `param a = Glue [4, 11]
param b = Room [0.08, 0.28]
param c = Click [0, 3]
comp1: threshold = -a; ratio = 3; attack = 0.008; release = 0.14; hpf = 80; makeup = 2
eq1: type = peak; freq = 4000; q = 1.1; gain = c
reverb1: size = 0.18; decay = b; damp = 0.45; mix = 0.1; width = 0.55`,
    opt(p, { a: p("Glue", 4, 11, 6.5), b: p("Room", 0.08, 0.28, 0.14), c: p("Click", 0, 3, 1.2) }));

  add("Sample Drum Polish", "Drums",
    "One-shot polish: limit, click, no room.",
    `param a = Click [0, 5]
param b = Ceiling [-2, -0.2]
param c = Cut [40, 100]
filter1: type = highpass; cutoff = c; resonance = 0.22
eq1: type = peak; freq = 2800; q = 1.2; gain = a
limit1: ceiling = b; release = 0.08`,
    opt(p, { a: p("Click", 0, 5, 2), b: p("Ceiling", -2, -0.2, -0.6), c: p("Cut", 40, 100, 55) }));

  add("Brush Kit Soft", "Drums",
    "Brushes: slow comp, dark, small booth.",
    `param a = Glue [3, 9]
param b = Dark [2500, 6000]
param c = Booth [0.08, 0.25]
comp1: threshold = -a; ratio = 2.5; attack = 0.02; release = 0.22; makeup = 1.5
filter1: type = lowpass; cutoff = b; resonance = 0.25
reverb1: size = 0.16; decay = c; damp = 0.5; mix = 0.14; width = 0.4`,
    opt(p, { a: p("Glue", 3, 9, 5), b: p("Dark", 2500, 6000, 4000), c: p("Booth", 0.08, 0.25, 0.14) }));
}

function registerModulation (add, p, opt)
{
  add("Slow Chorus Pad", "Modulation",
    "Slow wide chorus for pads. Not Chorus Delay.",
    `param a = Rate [0.08, 0.6]
param b = Depth [6, 20]
param c = Mix [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 14 + osc1 * b; feedback = 0.08; mix = c; damp = 7500`,
    opt(p, { a: p("Rate", 0.08, 0.6, 0.22), b: p("Depth", 6, 20, 11), c: p("Mix", 0.2, 0.7, 0.4) }));

  add("Fast Flange Jet", "Modulation",
    "Faster, more feedback than chorus. Jet / flange.",
    `param a = Rate [0.15, 2.5]
param b = Depth [1, 8]
param c = Feedback [0.15, 0.55]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 2 + osc1 * b; feedback = c; mix = 0.45; damp = 8000`,
    opt(p, { a: p("Rate", 0.15, 2.5, 0.55), b: p("Depth", 1, 8, 3.5), c: p("Feedback", 0.15, 0.55, 0.32) }));

  add("Barber Pole Phaser", "Modulation",
    "Two sweeping bandpasses. Slow climb.",
    `param a = Rate [0.05, 0.8]
param b = Span [300, 2000]
param c = Mix [0.25, 0.85]
osc1: shape = sine; freq = a; depth = 1
stage1: y = x
bus swirl:
  send: in = 1
  filter1: type = bandpass; center = 500 + osc1 * b; width = 400; resonance = 0.85
  filter2: type = bandpass; center = 1800 + osc1 * b * 0.6; width = 500; resonance = 0.8
out: main = 1-c; swirl = c`,
    opt(p, { a: p("Rate", 0.05, 0.8, 0.18), b: p("Span", 300, 2000, 900), c: p("Mix", 0.25, 0.85, 0.5) }));

  add("Tremolo Square Cut", "Modulation",
    "Harder than Classic Tremolo. Square AM.",
    `param a = Rate [1, 12]
param b = Depth [0.3, 1]
param c = Shape [0.2, 0.9]
osc1: shape = square; freq = a; depth = 1
stage1: y = x * (1 - b + b * (c + (1-c) * (0.5 + 0.5 * osc1)))`,
    opt(p, { a: p("Rate", 1, 12, 4), b: p("Depth", 0.3, 1, 0.7), c: p("Shape", 0.2, 0.9, 0.45) }));

  add("Tremolo Sine Warm", "Modulation",
    "Soft sine trem + tiny tube. Rhodes / guitar.",
    `param a = Rate [1.5, 8]
param b = Depth [0.2, 0.8]
param c = Warm [0.4, 1.6]
osc1: shape = sine; freq = a; depth = 1
stage1: y = tube(x * (1 - b + b * (0.5 + 0.5 * osc1)), c)`,
    opt(p, { a: p("Rate", 1.5, 8, 3.2), b: p("Depth", 0.2, 0.8, 0.45), c: p("Warm", 0.4, 1.6, 0.8) }));

  add("Auto Pan Wide", "Modulation",
    "LFO left/right via mid/side trick.",
    `param a = Rate [0.1, 2]
param b = Width [0.3, 1]
param c = Mix [0.3, 1]
osc1: shape = sine; freq = a; depth = 1
widen1: width = b * (0.4 + 0.6 * (0.5 + 0.5 * osc1)); delay = 12; bass = 140
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Rate", 0.1, 2, 0.4), b: p("Width", 0.3, 1, 0.7), c: p("Mix", 0.3, 1, 0.8) }));

  add("Ring Mod Metallic", "Modulation",
    "Audio-rate AM. Bells / robots. Not Ring Modulator stock.",
    `param a = Freq [40, 400]
param b = Mix [0.15, 0.8]
param c = Tone [2000, 8000]
osc1: shape = sine; freq = a; depth = 1
stage1: y = lerp(x, x * osc1, b)
filter1: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Freq", 40, 400, 120), b: p("Mix", 0.15, 0.8, 0.4), c: p("Tone", 2000, 8000, 5000) }));

  add("Vibrato Only", "Modulation",
    "Pitch wobble, no dry. Guitar / vocal special.",
    `param a = Rate [3, 8]
param b = Depth [2, 10]
param c = Mix [0.2, 1]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 8 + osc1 * b; feedback = 0.02; mix = c; damp = 8000`,
    opt(p, { a: p("Rate", 3, 8, 5), b: p("Depth", 2, 10, 5), c: p("Mix", 0.2, 1, 0.7) }));

  add("Leslie Slow", "Modulation",
    "Slow rotary: AM + bandpass sweep.",
    `param a = Rate [0.4, 1.6]
param b = Horn [400, 1800]
param c = AM [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
filter1: type = bandpass; center = 900 + osc1 * b; width = 700; resonance = 0.6
stage1: y = x * (1 - c + c * (0.5 + 0.5 * osc1))`,
    opt(p, { a: p("Rate", 0.4, 1.6, 0.75), b: p("Horn", 400, 1800, 900), c: p("AM", 0.2, 0.7, 0.4) }));

  add("Leslie Fast", "Modulation",
    "Fast rotary. Same idea as Leslie Slow, higher rate.",
    `param a = Rate [4, 9]
param b = Horn [500, 2200]
param c = AM [0.25, 0.8]
osc1: shape = sine; freq = a; depth = 1
filter1: type = bandpass; center = 1100 + osc1 * b; width = 800; resonance = 0.55
stage1: y = x * (1 - c + c * (0.5 + 0.5 * osc1))`,
    opt(p, { a: p("Rate", 4, 9, 6.2), b: p("Horn", 500, 2200, 1200), c: p("AM", 0.25, 0.8, 0.5) }));

  add("Envelope Phaser", "Modulation",
    "Env, not LFO, sweeps the notch.",
    `param a = Span [300, 1800]
param b = Mix [0.25, 0.9]
param c = Rel [0.04, 0.2]
env1: attack = 0.006; release = c
stage1: y = x
bus swirl:
  send: in = 1
  filter1: type = bandpass; center = 600 + env1 * a; width = 350; resonance = 0.9
out: main = 1-b; swirl = b`,
    opt(p, { a: p("Span", 300, 1800, 900), b: p("Mix", 0.25, 0.9, 0.5), c: p("Rel", 0.04, 0.2, 0.09) }));

  add("Detune Widen Mod", "Modulation",
    "Haas + slow chorus. Synths.",
    `param a = Rate [0.1, 0.8]
param b = Haas [8, 22]
param c = Width [0.3, 1]
osc1: shape = sine; freq = a; depth = 1
widen1: width = c; delay = b; bass = 150
delay1: time = 12 + osc1 * 5; feedback = 0.05; mix = 0.22; damp = 7000`,
    opt(p, { a: p("Rate", 0.1, 0.8, 0.28), b: p("Haas", 8, 22, 14), c: p("Width", 0.3, 1, 0.62) }));

  add("Chopper Sixteenth", "Modulation",
    "Tempo 1/16 gate. Not Chopper stock (free Hz).",
    `param a = Depth [0.4, 1]
param b = Smooth [0.001, 0.02]
param c = Mix [0.3, 1]
osc1: shape = square; freq = 1/16; depth = 1
stage1: y = lerp(x, x * (1 - a + a * (0.05 + 0.95 * (0.5 + 0.5 * osc1))), c)
filter1: type = lowpass; cutoff = 12000; resonance = 0.2`,
    opt(p, { a: p("Depth", 0.4, 1, 0.85), b: p("Smooth", 0.001, 0.02, 0.006), c: p("Mix", 0.3, 1, 0.8) }));

  add("Wow Flutter Tape", "Modulation",
    "Slow wow on a short delay. Tape motion, not Tape Saturate.",
    `param a = Wow [0.2, 1.2]
param b = Depth [1, 6]
param c = Mix [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 18 + osc1 * b; feedback = 0.12; mix = c; damp = 5000
stage1: y = tube(x, 0.9)`,
    opt(p, { a: p("Wow", 0.2, 1.2, 0.45), b: p("Depth", 1, 6, 3), c: p("Mix", 0.2, 0.7, 0.4) }));
}

function registerFilter (add, p, opt)
{
  add("DJ Low Kill", "Filter",
    "Kill the lows. DJ / transition.",
    `param a = Cut [80, 400]
param b = Res [0.2, 1.2]
param c = Mix [0.5, 1]
filter1: type = highpass; cutoff = a; resonance = b
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Cut", 80, 400, 180), b: p("Res", 0.2, 1.2, 0.45), c: p("Mix", 0.5, 1, 1) }));

  add("DJ High Kill", "Filter",
    "Kill the highs.",
    `param a = Cut [800, 6000]
param b = Res [0.2, 1.2]
param c = Mix [0.5, 1]
filter1: type = lowpass; cutoff = a; resonance = b
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Cut", 800, 6000, 2500), b: p("Res", 0.2, 1.2, 0.4), c: p("Mix", 0.5, 1, 1) }));

  add("Reso Acid Slide", "Filter",
    "High-res LPF, env opens. 303 cousin, not Acid Line.",
    `param a = Cut [200, 4000]
param b = Res [0.8, 3.2]
param c = Env [200, 2500]
env1: attack = 0.004; release = 0.12
filter1: type = lowpass; cutoff = a + env1 * c; resonance = b`,
    opt(p, { a: p("Cut", 200, 4000, 800), b: p("Res", 0.8, 3.2, 1.8), c: p("Env", 200, 2500, 900) }));

  add("Notch Honk Cut", "Filter",
    "Deep notch. Feedback / honk removal.",
    `param a = Freq [200, 2500]
param b = Depth [-16, -4]
param c = Q [1, 6]
eq1: type = notch; freq = a; q = c; gain = b`,
    opt(p, { a: p("Freq", 200, 2500, 800), b: p("Depth", -16, -4, -9), c: p("Q", 1, 6, 2.4) }));

  add("Bandpass Radio", "Filter",
    "Radio band without crush.",
    `param a = Center [600, 2500]
param b = Width [300, 1600]
param c = Mix [0.4, 1]
stage1: y = x
bus band:
  send: in = 1
  filter1: type = bandpass; center = a; width = b
out: main = 1-c; band = c`,
    opt(p, { a: p("Center", 600, 2500, 1200), b: p("Width", 300, 1600, 700), c: p("Mix", 0.4, 1, 0.85) }));

  add("LFO Filter Sweep", "Filter",
    "Slow LPF sweep. Pads / breaks.",
    `param a = Rate [0.05, 0.8]
param b = Span [400, 4000]
param c = Floor [200, 1200]
osc1: shape = sine; freq = a; depth = 1
filter1: type = lowpass; cutoff = c + (0.5 + 0.5 * osc1) * b; resonance = 0.7`,
    opt(p, { a: p("Rate", 0.05, 0.8, 0.15), b: p("Span", 400, 4000, 1800), c: p("Floor", 200, 1200, 400) }));

  add("HPF Rumble Cut", "Filter",
    "Just a musical HPF. Mix / bus.",
    `param a = Cut [25, 120]
param b = Res [0.15, 0.7]
param c = Mix [0.5, 1]
filter1: type = highpass; cutoff = a; resonance = b
eq1: type = lowshelf; freq = 80; q = 0.55; gain = -1.2
stage1: y = x`,
    opt(p, { a: p("Cut", 25, 120, 40), b: p("Res", 0.15, 0.7, 0.28), c: p("Mix", 0.5, 1, 1) }));

  add("Tilt Dark Bright", "Filter",
    "Low shelf vs high shelf tilt.",
    `param a = Tilt [-6, 6]
param b = Pivot [400, 1600]
param c = Mix [0.4, 1]
eq1: type = lowshelf; freq = b; q = 0.6; gain = -a
eq2: type = highshelf; freq = b; q = 0.6; gain = a
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Tilt", -6, 6, 2), b: p("Pivot", 400, 1600, 800), c: p("Mix", 0.4, 1, 1) }));

  add("Formant Vowel", "Filter",
    "Two formant peaks. Voice / synth.",
    `param a = Vowel [400, 900]
param b = Bright [0.6, 1.6]
param c = Mix [0.3, 1]
stage1: y = x
bus vow:
  send: in = 1
  filter1: type = bandpass; center = a; width = 180; resonance = 1.4
  filter2: type = bandpass; center = a * b * 2.2; width = 260; resonance = 1.2
out: main = 1-c; vow = c`,
    opt(p, { a: p("Vowel", 400, 900, 600), b: p("Bright", 0.6, 1.6, 1.05), c: p("Mix", 0.3, 1, 0.65) }));

  add("Comb Metallic", "Filter",
    "Short delay comb. Metal / Karplus-ish.",
    `param a = Time [4, 28]
param b = Feedback [0.2, 0.7]
param c = Mix [0.2, 0.8]
delay1: time = a; feedback = b; mix = c; damp = 6000`,
    opt(p, { a: p("Time", 4, 28, 11), b: p("Feedback", 0.2, 0.7, 0.42), c: p("Mix", 0.2, 0.8, 0.45) }));

  add("Shelf Air Only Filt", "Filter",
    "Just a high shelf. Mix bus air.",
    `param a = Air [0, 7]
param b = Freq [6000, 14000]
param c = Mix [0.4, 1]
eq1: type = highshelf; freq = b; q = 0.5; gain = a
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Air", 0, 7, 2.4), b: p("Freq", 6000, 14000, 9500), c: p("Mix", 0.4, 1, 1) }));

  add("Low Shelf Weight", "Filter",
    "Just a low shelf. Weight without a bass plugin.",
    `param a = Weight [-4, 6]
param b = Freq [60, 180]
param c = Mix [0.4, 1]
eq1: type = lowshelf; freq = b; q = 0.6; gain = a
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Weight", -4, 6, 2), b: p("Freq", 60, 180, 100), c: p("Mix", 0.4, 1, 1) }));
}

function registerMastering (add, p, opt)
{
  add("Gentle Bus Glue M", "Mastering",
    "2:1 mixbus. Lighter than Bus Glue.",
    `param a = Glue [3, 10]
param b = Attack [0.01, 0.04]
param c = Ceiling [-1.5, -0.2]
comp1: threshold = -a; ratio = 1.8; attack = b; release = 0.22; knee = 8; makeup = 0.8; hpf = 80
limit1: ceiling = c; release = 0.1`,
    opt(p, { a: p("Glue", 3, 10, 6), b: p("Attack", 0.01, 0.04, 0.02), c: p("Ceiling", -1.5, -0.2, -0.5) }));

  add("Loudness Clip Master", "Mastering",
    "Soft-pre clip then limit. Not Loudness Clip stock.",
    `param a = Drive [0.85, 1.5]
param b = Ceiling [-1.2, -0.15]
param c = Tone [8000, 16000]
stage1: y = hardclip(softclip(x, a), 0.92)
eq1: type = highshelf; freq = c; q = 0.5; gain = 0.6
limit1: ceiling = b; release = 0.08`,
    opt(p, { a: p("Drive", 0.85, 1.5, 1.05), b: p("Ceiling", -1.2, -0.15, -0.4), c: p("Tone", 8000, 16000, 11000) }));

  add("Air And Weight", "Mastering",
    "Low shelf + air. No clip.",
    `param a = Weight [0, 3]
param b = Air [0, 3]
param c = Pivot [80, 160]
eq1: type = lowshelf; freq = c; q = 0.55; gain = a
eq2: type = highshelf; freq = 11000; q = 0.5; gain = b`,
    opt(p, { a: p("Weight", 0, 3, 1.2), b: p("Air", 0, 3, 1.4), c: p("Pivot", 80, 160, 110) }));

  add("Mid Focus Master", "Mastering",
    "MS: lift mid 1–3 kHz, tame sides 300 Hz.",
    `param a = Mid [0, 3]
param b = Side [-4, 0]
param c = Width [0.7, 1.15]
ms1: mode = encode
eq1: channel = mid; type = peak; freq = 1800; q = 0.9; gain = a
eq2: channel = side; type = peak; freq = 300; q = 0.8; gain = b
ms2: mode = decode
widen1: width = c; delay = 8; bass = 120`,
    opt(p, { a: p("Mid", 0, 3, 1.2), b: p("Side", -4, 0, -1.5), c: p("Width", 0.7, 1.15, 0.95) }));

  add("Stereo Widen Safe", "Mastering",
    "Gentle widen, bass mono. Safer than Haas Width.",
    `param a = Width [0.15, 0.7]
param b = Haas [6, 16]
param c = Bass [80, 180]
widen1: width = a; delay = b; bass = c`,
    opt(p, { a: p("Width", 0.15, 0.7, 0.35), b: p("Haas", 6, 16, 10), c: p("Bass", 80, 180, 120) }));

  add("Mono Check Fold", "Mastering",
    "Utility: collapse toward mono to check.",
    `param a = Width [0, 1]
param b = Bass [60, 160]
param c = Level [0.7, 1.1]
widen1: width = a; delay = 0; bass = b
stage1: y = x * c`,
    opt(p, { a: p("Width", 0, 1, 0.15), b: p("Bass", 60, 160, 100), c: p("Level", 0.7, 1.1, 1) }));

  add("Tilt Master", "Mastering",
    "Mastering tilt EQ.",
    `param a = Tilt [-3, 3]
param b = Pivot [500, 1200]
param c = Ceiling [-1, -0.2]
eq1: type = lowshelf; freq = b; q = 0.55; gain = -a
eq2: type = highshelf; freq = b; q = 0.55; gain = a
limit1: ceiling = c; release = 0.1`,
    opt(p, { a: p("Tilt", -3, 3, 0.8), b: p("Pivot", 500, 1200, 800), c: p("Ceiling", -1, -0.2, -0.4) }));

  add("OTT Glue Master", "Mastering",
    "Very light OTT on the mixbus.",
    `param a = Depth [0.08, 0.35]
param b = Time [0.02, 0.08]
param c = Ceiling [-1, -0.2]
ott1: depth = a; time = b; in = 1; low = 0.9; mid = 1; high = 1.05
limit1: ceiling = c; release = 0.1`,
    opt(p, { a: p("Depth", 0.08, 0.35, 0.16), b: p("Time", 0.02, 0.08, 0.035), c: p("Ceiling", -1, -0.2, -0.4) }));

  add("Safety Ceiling Master", "Mastering",
    "Last plugin: just a limiter.",
    `param a = Ceiling [-2, -0.1]
param b = Release [0.05, 0.2]
param c = In [0.9, 1.2]
stage1: y = x * c
limit1: ceiling = a; release = b`,
    opt(p, { a: p("Ceiling", -2, -0.1, -0.3), b: p("Release", 0.05, 0.2, 0.09), c: p("In", 0.9, 1.2, 1) }));

  add("Presence Master", "Mastering",
    "2.8 kHz presence only. Dense mixes.",
    `param a = Cut [0, 2.5]
param b = Q [0.6, 1.4]
param c = Ceiling [-1, -0.2]
eq1: type = peak; freq = 2800; q = b; gain = a
limit1: ceiling = c; release = 0.1`,
    opt(p, { a: p("Cut", 0, 2.5, 1.0), b: p("Q", 0.6, 1.4, 0.9), c: p("Ceiling", -1, -0.2, -0.4) }));

  add("Dark Master", "Mastering",
    "Darken a harsh mix. High cut + tiny clip.",
    `param a = Top [7000, 14000]
param b = Air [-3, 0]
param c = Ceiling [-1, -0.2]
filter1: type = lowpass; cutoff = a; resonance = 0.22
eq1: type = highshelf; freq = 9000; q = 0.5; gain = b
limit1: ceiling = c; release = 0.1`,
    opt(p, { a: p("Top", 7000, 14000, 11000), b: p("Air", -3, 0, -1.2), c: p("Ceiling", -1, -0.2, -0.4) }));

  add("Punch Master", "Mastering",
    "Transient lift then glue. Rock / hip-hop.",
    `param a = Punch [0.05, 0.4]
param b = Glue [3, 9]
param c = Ceiling [-1, -0.2]
env1: type = peak; attack = 0.001; release = 0.012
stage1: y = x * clamp(1 + a * env1, 0.5, 2)
comp1: threshold = -b; ratio = 2; attack = 0.012; release = 0.18; makeup = 0.5
limit1: ceiling = c; release = 0.09`,
    opt(p, { a: p("Punch", 0.05, 0.4, 0.16), b: p("Glue", 3, 9, 5.5), c: p("Ceiling", -1, -0.2, -0.4) }));
}

function registerUtility (add, p, opt)
{
  add("High Pass Utility", "Utility",
    "Clean HPF. Every track.",
    `param a = Cut [20, 200]
param b = Res [0.15, 0.6]
param c = Mix [0.5, 1]
filter1: type = highpass; cutoff = a; resonance = b
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Cut", 20, 200, 40), b: p("Res", 0.15, 0.6, 0.25), c: p("Mix", 0.5, 1, 1) }));

  add("Low Pass Utility", "Utility",
    "Clean LPF.",
    `param a = Cut [1000, 16000]
param b = Res [0.15, 0.6]
param c = Mix [0.5, 1]
filter1: type = lowpass; cutoff = a; resonance = b
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Cut", 1000, 16000, 8000), b: p("Res", 0.15, 0.6, 0.25), c: p("Mix", 0.5, 1, 1) }));

  add("Gain Stage Utility", "Utility",
    "Just level. Gain staging.",
    `param a = Gain [0.25, 2]
param b = Trim [-12, 12]
param c = Ceiling [-1, 0]
stage1: y = x * a * pow(10, b / 20)
limit1: ceiling = c; release = 0.08`,
    opt(p, { a: p("Gain", 0.25, 2, 1), b: p("Trim", -12, 12, 0), c: p("Ceiling", -1, 0, -0.3) }));

  add("Phase Flip Utility", "Utility",
    "Invert polarity.",
    `param a = Flip [0, 1]
param b = Level [0.5, 1.2]
param c = Mix [0, 1]
stage1: y = lerp(x, x * (1 - 2 * a), c) * b`,
    opt(p, { a: p("Flip", 0, 1, 1), b: p("Level", 0.5, 1.2, 1), c: p("Mix", 0, 1, 1) }));

  add("MS Encode Utility", "Utility",
    "Listen mid/side. Encode only — decode after.",
    `param a = Mid [0, 2]
param b = Side [0, 2]
param c = Listen [0, 1]
ms1: mode = encode
stage1: channel = mid; y = x * a
stage2: channel = side; y = x * b * c
ms2: mode = decode`,
    opt(p, { a: p("Mid", 0, 2, 1), b: p("Side", 0, 2, 1), c: p("Listen", 0, 1, 1) }));

  add("Dry Wet Utility", "Utility",
    "Parallel rack: dry vs this track's input copy. Blend.",
    `param a = Blend [0, 1]
param b = WetGain [0.4, 1.6]
param c = HPF [20, 200]
filter1: type = highpass; cutoff = c; resonance = 0.2
stage1: y = x
bus wet:
  send: in = 1
  stage2: y = x * b
out: main = 1-a; wet = a`,
    opt(p, { a: p("Blend", 0, 1, 0.5), b: p("WetGain", 0.4, 1.6, 1), c: p("HPF", 20, 200, 30) }));

  add("Meter Probe Utility", "Utility",
    "Dry meter in the chain. Does not change the sound.",
    `param a = Gain [0.5, 1.5]
param b = Trim [0.7, 1.3]
param c = Mix [0.5, 1]
meter1: mode = loudness
stage1: y = x * a * b
stage2: y = lerp(x, y, c)`,
    opt(p, { a: p("Gain", 0.5, 1.5, 1), b: p("Trim", 0.7, 1.3, 1), c: p("Mix", 0.5, 1, 1) }));

  add("Safety Clip Utility", "Utility",
    "Last-resort clip. Different ceiling range than Safety Clip stock.",
    `param a = Ceiling [0.7, 0.99]
param b = Drive [0.8, 1.3]
param c = Mix [0.5, 1]
stage1: y = lerp(x, hardclip(softclip(x, b), a), c)`,
    opt(p, { a: p("Ceiling", 0.7, 0.99, 0.92), b: p("Drive", 0.8, 1.3, 1.0), c: p("Mix", 0.5, 1, 1) }));
}

function registerPsycho (add, p, opt)
{
  add("Missing Air", "Psychoacoustic",
    "Implied air via 10 kHz shelf + tiny fold. Not Loudness Curve.",
    `param a = Air [1, 6]
param b = Fold [0.05, 0.2]
param c = Mix [0.3, 1]
eq1: type = highshelf; freq = 10500; q = 0.5; gain = a
stage1: y = lerp(x, fold(x, -b, b), c)`,
    opt(p, { a: p("Air", 1, 6, 2.6), b: p("Fold", 0.05, 0.2, 0.1), c: p("Mix", 0.3, 1, 0.55) }));

  add("Center Cut Through", "Psychoacoustic",
    "Boost mid 2.5 kHz, dip sides 2.5 kHz. Vocal in a dense mix.",
    `param a = Mid [0, 4]
param b = Side [-4, 0]
param c = Freq [1800, 3500]
ms1: mode = encode
eq1: channel = mid; type = peak; freq = c; q = 1.1; gain = a
eq2: channel = side; type = peak; freq = c; q = 1.1; gain = b
ms2: mode = decode`,
    opt(p, { a: p("Mid", 0, 4, 2), b: p("Side", -4, 0, -1.8), c: p("Freq", 1800, 3500, 2500) }));

  add("Bass Weight Trick", "Psychoacoustic",
    "Even harmonics under 120 Hz. Implied bass on small speakers.",
    `param a = Harm [0.6, 2.2]
param b = BandHz [80, 160]
param c = Mix [0.2, 0.8]
xover1: f1 = b
bus low:
  stage1: y = tube(x, a)
out: low = c; high = 1`,
    opt(p, { a: p("Harm", 0.6, 2.2, 1.2), b: p("BandHz", 80, 160, 110), c: p("Mix", 0.2, 0.8, 0.45) }));

  add("Ear Tickle 3k", "Psychoacoustic",
    "Narrow 3.2 kHz lift. Speech / lead.",
    `param a = Tickle [0, 4]
param b = Q [1, 3]
param c = Mix [0.4, 1]
eq1: type = peak; freq = 3200; q = b; gain = a
stage1: y = lerp(x, y, c)`,
    opt(p, { a: p("Tickle", 0, 4, 1.8), b: p("Q", 1, 3, 1.6), c: p("Mix", 0.4, 1, 1) }));

  add("Width Without Haas", "Psychoacoustic",
    "Side shelf only. No delay.",
    `param a = Side [0, 5]
param b = Freq [4000, 10000]
param c = Mid [0.8, 1.1]
ms1: mode = encode
stage1: channel = mid; y = x * c
eq1: channel = side; type = highshelf; freq = b; q = 0.5; gain = a
ms2: mode = decode`,
    opt(p, { a: p("Side", 0, 5, 2.4), b: p("Freq", 4000, 10000, 7000), c: p("Mid", 0.8, 1.1, 1) }));
}

function registerDistortion (add, p, opt, irA, irB, irM, irV)
{
  add("Green Boost Only", "Distortion",
    "TS mid-hump, almost no clip. Boost into an amp.",
    `param a = Boost [1, 6]
param b = Mid [3, 8]
param c = Tone [1500, 5000]
filter1: type = highpass; cutoff = 700; resonance = 0.25
eq1: type = peak; freq = 720; q = 1.5; gain = b
stage1: y = softclip(x, a * 0.35)
filter2: type = lowpass; cutoff = c; resonance = 0.35`,
    opt(p, { a: p("Boost", 1, 6, 2.4), b: p("Mid", 3, 8, 5), c: p("Tone", 1500, 5000, 2800) }));

  add("Transparent Diode", "Distortion",
    "Klon cousin without the stock blend graph: diode + 1 kHz bump.",
    `param a = Drive [0.8, 6]
param b = Bell [1, 5]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 90; resonance = 0.25
stage1: y = diode(x, a) * c
eq1: type = peak; freq = 1100; q = 0.85; gain = b
filter2: type = lowpass; cutoff = 8500; resonance = 0.28`,
    opt(p, { a: p("Drive", 0.8, 6, 2.6), b: p("Bell", 1, 5, 2.6), c: p("Level", 0.4, 1.2, 0.85) }));

  add("Silicon Fuzz Gate", "Distortion",
    "Brighter fuzz than Fuzz Face, with a faster gate.",
    `param a = Fuzz [3, 14]
param b = Tone [800, 5000]
param c = Open [-48, -26]
gate1: threshold = c; hyst = 6; hold = 0.02; range = -75
filter1: type = highpass; cutoff = 80; resonance = 0.32
stage1: y = hardclip(softclip(x, a * 0.35), 0.42)
filter2: type = lowpass; cutoff = b; resonance = 0.55`,
    opt(p, { a: p("Fuzz", 3, 14, 8), b: p("Tone", 800, 5000, 2400), c: p("Open", -48, -26, -36) }));

  add("Octave Fuzz", "Distortion",
    "Fuzz after a little up-octave. Synth-fuzz.",
    `param a = Fuzz [2, 10]
param b = Octave [0.1, 0.5]
param c = Tone [1200, 4500]
octaver1: mix = b; sub = 0; up = 1; tone = 400; thresh = 0.05
stage1: y = fold(x, -0.28, 0.28)
stage2: y = softclip(y, a)
filter1: type = lowpass; cutoff = c; resonance = 0.4`,
    opt(p, { a: p("Fuzz", 2, 10, 5), b: p("Octave", 0.1, 0.5, 0.25), c: p("Tone", 1200, 4500, 2600) }));

  add("Power Amp Grind", "Distortion",
    "Just the power-amp end: tube + hard-knee + cab. No preamp stack.",
    `param a = Grind [1.2, 6]
param b = Ceiling [0.4, 0.85]
param c = Cab [0.25, 0.65]
stage1: y = tube(x, a)
stage2: y = hardclip(softclip(y, 1.3), b)
filter1: type = lowpass; cutoff = 5500; resonance = 0.4
ir1: mix = c; gain = 1`,
    opt(p, { a: p("Grind", 1.2, 6, 2.8), b: p("Ceiling", 0.4, 0.85, 0.62), c: p("Cab", 0.25, 0.65, 0.42), tags: ["ir"] }, { irs: irB }));

  add("Preamp Only Stack", "Distortion",
    "Three tubes, no cab, open LPF. Re-amp later.",
    `param a = Gain [2, 10]
param b = Tight [60, 180]
param c = Top [4000, 9000]
filter1: type = highpass; cutoff = b; resonance = 0.4
stage1: y = tube(x, a * 0.45)
stage2: y = tube(y, a * 0.4)
stage3: y = tube(y, a * 0.3)
filter2: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Gain", 2, 10, 5.5), b: p("Tight", 60, 180, 100), c: p("Top", 4000, 9000, 6500) }));

  add("Hard Ceiling Pedal", "Distortion",
    "Pedal-style hard ceiling after a soft pre. Not Hard Clip Pedal stock.",
    `param a = Drive [1.5, 8]
param b = Ceiling [0.3, 0.75]
param c = Tone [1500, 6000]
filter1: type = highpass; cutoff = 100; resonance = 0.3
stage1: y = hardclip(softclip(x, a), b)
filter2: type = lowpass; cutoff = c; resonance = 0.45`,
    opt(p, { a: p("Drive", 1.5, 8, 3.8), b: p("Ceiling", 0.3, 0.75, 0.5), c: p("Tone", 1500, 6000, 3200) }));

  add("Asym Tube Push", "Distortion",
    "More even harmonics: tube + tiny DC. Blues.",
    `param a = Drive [0.8, 4.5]
param b = Bias [0.0, 0.12]
param c = Tone [2500, 7000]
filter1: type = highpass; cutoff = 70; resonance = 0.25
stage1: y = tube(x + b, a)
filter2: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Drive", 0.8, 4.5, 2.1), b: p("Bias", 0, 0.12, 0.04), c: p("Tone", 2500, 7000, 4500) }));

  add("Fold Then Recover", "Distortion",
    "Fold then a steep LPF. Not Wave Folder stock.",
    `param a = Fold [0.15, 0.55]
param b = Recover [1200, 5000]
param c = Level [0.4, 1.1]
stage1: y = fold(x, -a, a) * c
filter1: type = lowpass; cutoff = b; resonance = 0.42`,
    opt(p, { a: p("Fold", 0.15, 0.55, 0.3), b: p("Recover", 1200, 5000, 2800), c: p("Level", 0.4, 1.1, 0.75) }));

  add("Bitcrush Then Cab", "Distortion",
    "Crush into a vintage cab. Digital amp.",
    `param a = Bits [4, 11]
param b = Drive [0.8, 3]
param c = Cab [0.25, 0.65]
stage1: y = bitcrush(softclip(x, b), a)
filter1: type = lowpass; cutoff = 5500; resonance = 0.35
ir1: mix = c; gain = 2`,
    opt(p, { a: p("Bits", 4, 11, 7), b: p("Drive", 0.8, 3, 1.5), c: p("Cab", 0.25, 0.65, 0.4), tags: ["ir"] }, { irs: irV }));

  add("Diode Stack Pedal", "Distortion",
    "Two diodes in series, mid bump.",
    `param a = Drive [1, 7]
param b = Mid [1, 6]
param c = Tone [1800, 6500]
filter1: type = highpass; cutoff = 110; resonance = 0.3
stage1: y = diode(x, a)
stage2: y = diode(y, a * 0.7)
eq1: type = peak; freq = 900; q = 1.1; gain = b
filter2: type = lowpass; cutoff = c; resonance = 0.38`,
    opt(p, { a: p("Drive", 1, 7, 3.2), b: p("Mid", 1, 6, 3), c: p("Tone", 1800, 6500, 3500) }));

  add("Softclip Open", "Distortion",
    "Widest softclip, almost hi-fi. Not Soft Overdrive.",
    `param a = Drive [0.5, 3.5]
param b = Air [0, 4]
param c = Level [0.5, 1.3]
stage1: y = softclip(x, a) * c
eq1: type = highshelf; freq = 7000; q = 0.5; gain = b`,
    opt(p, { a: p("Drive", 0.5, 3.5, 1.4), b: p("Air", 0, 4, 1.6), c: p("Level", 0.5, 1.3, 0.95) }));

  add("Parallel Dirt Blend", "Distortion",
    "Clean plus a diode bus. Not Klon Centaur.",
    `param a = Drive [1, 7]
param b = Blend [0.15, 0.7]
param c = Tone [2000, 7000]
stage1: y = x
bus dirt:
  send: in = 1
  stage2: y = diode(x, a)
  filter1: type = lowpass; cutoff = c; resonance = 0.32
out: main = 1-b; dirt = b`,
    opt(p, { a: p("Drive", 1, 7, 3), b: p("Blend", 0.15, 0.7, 0.4), c: p("Tone", 2000, 7000, 4000) }));

  add("High Gain American", "Distortion",
    "American high-gain without Mesa name. Tight + cab.",
    `param a = Gain [4, 12]
param b = Tight [50, 140]
param c = Cab [0.3, 0.7]
gate1: threshold = -44; hyst = 6; hold = 0.025; range = -78
filter1: type = highpass; cutoff = b; resonance = 0.55
stage1: y = tube(x, a * 0.5)
stage2: y = tube(y, a * 0.4)
stage3: y = hardclip(softclip(y, 1.4), 0.65)
ir1: mix = c; gain = 0`,
    opt(p, { a: p("Gain", 4, 12, 7.5), b: p("Tight", 50, 140, 85), c: p("Cab", 0.3, 0.7, 0.48), tags: ["gate", "ir"] }, { irs: irA }));

  add("British Plexi Bark", "Distortion",
    "Plexi cousin: 800 Hz bark, no Marshall name.",
    `param a = Drive [1.5, 7]
param b = Bark [2, 7]
param c = Cab [0.25, 0.6]
filter1: type = highpass; cutoff = 105; resonance = 0.36
stage1: y = tube(x, a * 0.6)
stage2: y = tube(y, a * 0.5)
eq1: type = peak; freq = 800; q = 1.05; gain = b
ir1: mix = c; gain = 1`,
    opt(p, { a: p("Drive", 1.5, 7, 3.4), b: p("Bark", 2, 7, 4), c: p("Cab", 0.25, 0.6, 0.4), tags: ["ir"] }, { irs: irB }));

  add("Tweed Edge", "Distortion",
    "Small tweed: loose low, early breakup, vintage cab.",
    `param a = Push [0.7, 3.5]
param b = Loose [40, 90]
param c = Cab [0.2, 0.55]
filter1: type = highpass; cutoff = b; resonance = 0.22
stage1: y = tube(x, a)
eq1: type = peak; freq = 150; q = 0.7; gain = 2
ir1: mix = c; gain = 2`,
    opt(p, { a: p("Push", 0.7, 3.5, 1.6), b: p("Loose", 40, 90, 55), c: p("Cab", 0.2, 0.55, 0.35), tags: ["ir"] }, { irs: irV }));

  add("Chime OD Stack", "Distortion",
    "OD into a chime amp. Not Vox Top Boost.",
    `param a = Pedal [1, 6]
param b = Chime [1, 5]
param c = Cab [0.2, 0.55]
filter1: type = highpass; cutoff = 450; resonance = 0.35
stage1: y = softclip(x, a)
eq1: type = peak; freq = 2000; q = 1.2; gain = b
stage2: y = tube(y, 1.3)
ir1: mix = c; gain = 1`,
    opt(p, { a: p("Pedal", 1, 6, 2.8), b: p("Chime", 1, 5, 2.6), c: p("Cab", 0.2, 0.55, 0.35), tags: ["ir"] }, { irs: irA }));

  add("Rat Into Amp", "Distortion",
    "RAT filter into a tube stage + medium cab.",
    `param a = Dist [2, 10]
param b = Filter [500, 3500]
param c = Amp [0.8, 3]
filter1: type = highpass; cutoff = 80; resonance = 0.3
stage1: y = hardclip(softclip(x, a * 0.5), 0.52)
filter2: type = lowpass; cutoff = b; resonance = 0.58
stage2: y = tube(y, c)
ir1: mix = 0.4; gain = 1`,
    opt(p, { a: p("Dist", 2, 10, 5.5), b: p("Filter", 500, 3500, 1600), c: p("Amp", 0.8, 3, 1.6), tags: ["ir"] }, { irs: irM }));

  add("Muff Into Amp", "Distortion",
    "Stacked-filter fuzz into a dark cab.",
    `param a = Fuzz [2, 9]
param b = Scoop [500, 1200]
param c = Cab [0.3, 0.7]
filter1: type = highpass; cutoff = 70; resonance = 0.32
filter2: type = lowpass; cutoff = 3800; resonance = 0.4
stage1: y = softclip(x, a)
eq1: type = peak; freq = b; q = 0.9; gain = -4.5
ir1: mix = c; gain = 2`,
    opt(p, { a: p("Fuzz", 2, 9, 5), b: p("Scoop", 500, 1200, 800), c: p("Cab", 0.3, 0.7, 0.45), tags: ["ir"] }, { irs: irV }));

  add("Crunch Rhythm Pedal", "Distortion",
    "Medium crunch, 1.2 kHz, no IR.",
    `param a = Drive [1.4, 6]
param b = Mid [1, 5]
param c = Tone [2500, 6500]
filter1: type = highpass; cutoff = 100; resonance = 0.32
stage1: y = tube(x, a)
eq1: type = peak; freq = 1200; q = 1.05; gain = b
filter2: type = lowpass; cutoff = c; resonance = 0.35`,
    opt(p, { a: p("Drive", 1.4, 6, 3.2), b: p("Mid", 1, 5, 2.6), c: p("Tone", 2500, 6500, 4200) }));

  add("Lead Satin Clip", "Distortion",
    "Smoother lead clip + presence, no gate.",
    `param a = Drive [1.6, 7]
param b = Air [1, 5]
param c = Level [0.4, 1.1]
filter1: type = highpass; cutoff = 130; resonance = 0.3
stage1: y = softclip(x, a) * c
eq1: type = highshelf; freq = 4200; q = 0.55; gain = b`,
    opt(p, { a: p("Drive", 1.6, 7, 3.5), b: p("Air", 1, 5, 2.4), c: p("Level", 0.4, 1.1, 0.75) }));

  add("Bass Dirt Pedal", "Distortion",
    "Bass-safe dirt: blend, LPF 3 kHz.",
    `param a = Drive [1, 6]
param b = Blend [0.1, 0.55]
param c = Top [1500, 4000]
stage1: y = x
bus dirt:
  send: in = 1
  filter1: type = highpass; cutoff = 50; resonance = 0.22
  stage2: y = tube(x, a)
  filter2: type = lowpass; cutoff = c; resonance = 0.3
out: main = 1-b; dirt = b`,
    opt(p, { a: p("Drive", 1, 6, 2.6), b: p("Blend", 0.1, 0.55, 0.28), c: p("Top", 1500, 4000, 2600) }));

  add("Synth Clip Desk", "Distortion",
    "Synths: softclip + 8 kHz recovery. Not a guitar amp.",
    `param a = Drive [0.8, 4]
param b = Top [4000, 12000]
param c = Level [0.5, 1.2]
stage1: y = softclip(x, a) * c
filter1: type = lowpass; cutoff = b; resonance = 0.25`,
    opt(p, { a: p("Drive", 0.8, 4, 1.8), b: p("Top", 4000, 12000, 8000), c: p("Level", 0.5, 1.2, 0.9) }));

  add("Vocal Tube Desk", "Distortion",
    "Vocal: tiny tube, HPF 80, no cab.",
    `param a = Warm [0.5, 2.2]
param b = Air [0, 4]
param c = Cut [50, 140]
filter1: type = highpass; cutoff = c; resonance = 0.22
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 8000; q = 0.5; gain = b`,
    opt(p, { a: p("Warm", 0.5, 2.2, 1.05), b: p("Air", 0, 4, 1.6), c: p("Cut", 50, 140, 80) }));

  add("Drum Clip Bus", "Distortion",
    "Drum bus clip, 80 HPF, 6 kHz LPF.",
    `param a = Drive [1, 5]
param b = Ceiling [0.45, 0.9]
param c = Top [3500, 8000]
filter1: type = highpass; cutoff = 70; resonance = 0.25
stage1: y = hardclip(softclip(x, a), b)
filter2: type = lowpass; cutoff = c; resonance = 0.28`,
    opt(p, { a: p("Drive", 1, 5, 2.2), b: p("Ceiling", 0.45, 0.9, 0.7), c: p("Top", 3500, 8000, 5500) }));

  add("Mixbus Satin", "Distortion",
    "Mixbus: barely-there tube + limit.",
    `param a = Satin [0.4, 1.4]
param b = Ceiling [-1.2, -0.2]
param c = Air [0, 2]
stage1: y = tube(x, a)
eq1: type = highshelf; freq = 12000; q = 0.5; gain = c
limit1: ceiling = b; release = 0.1`,
    opt(p, { a: p("Satin", 0.4, 1.4, 0.75), b: p("Ceiling", -1.2, -0.2, -0.4), c: p("Air", 0, 2, 0.6) }));

  add("Lo-Fi Amp Sim", "Distortion",
    "Tiny speaker: 300–3.5 kHz + crush.",
    `param a = Bits [5, 11]
param b = Low [180, 450]
param c = High [2500, 5000]
filter1: type = highpass; cutoff = b; resonance = 0.35
filter2: type = lowpass; cutoff = c; resonance = 0.4
stage1: y = bitcrush(softclip(x, 1.5), a)`,
    opt(p, { a: p("Bits", 5, 11, 8), b: p("Low", 180, 450, 280), c: p("High", 2500, 5000, 3500) }));
}

function registerClub (add, p, opt)
{
  add("Kick Click Split", "Club",
    "Kick: click on a high bus, body on low. Not Kick Rumble.",
    `param a = Click [0, 6]
param b = Body [0, 5]
param c = BandHz [180, 400]
xover1: f1 = c
bus low:
  eq1: type = peak; freq = 55; q = 1.1; gain = b
bus high:
  eq2: type = peak; freq = 3000; q = 1.3; gain = a
out: low = 1; high = 1`,
    opt(p, { a: p("Click", 0, 6, 2.8), b: p("Body", 0, 5, 2.2), c: p("BandHz", 180, 400, 260) }));

  add("Warehouse Body", "Club",
    "Kick body + short metal delay. Not Warehouse Rumble.",
    `param a = Metal [8, 28]
param b = Body [1, 5]
param c = Mix [0.08, 0.3]
eq1: type = peak; freq = 60; q = 1.05; gain = b
delay1: time = a; feedback = 0.22; mix = c; damp = 2500`,
    opt(p, { a: p("Metal", 8, 28, 16), b: p("Body", 1, 5, 2.6), c: p("Mix", 0.08, 0.3, 0.14) }));

  add("Hard Clip Club", "Club",
    "Club clip lighter than Hardcore Clip.",
    `param a = Drive [2, 9]
param b = Ceiling [0.28, 0.7]
param c = Top [3000, 7000]
filter1: type = highpass; cutoff = 80; resonance = 0.3
stage1: y = hardclip(softclip(x, a * 0.4), b)
filter2: type = lowpass; cutoff = c; resonance = 0.32`,
    opt(p, { a: p("Drive", 2, 9, 5), b: p("Ceiling", 0.28, 0.7, 0.45), c: p("Top", 3000, 7000, 4800) }));

  add("Gabber Mid Bark", "Club",
    "Mid bark + sub bus. Not Gabber Drive.",
    `param a = Bark [2, 8]
param b = Sub [0.2, 0.8]
param c = Drive [2, 8]
filter1: type = highpass; cutoff = 90; resonance = 0.35
eq1: type = peak; freq = 1400; q = 1.2; gain = a
stage1: y = hardclip(softclip(x, c * 0.35), 0.4)
bus sub:
  send: in = 1
  filter2: type = lowpass; cutoff = 90; resonance = 0.25
  stage2: y = tube(x, 1.2)
out: main = 1; sub = b`,
    opt(p, { a: p("Bark", 2, 8, 4), b: p("Sub", 0.2, 0.8, 0.4), c: p("Drive", 2, 8, 4.5) }));

  add("Acid Soft Slide", "Club",
    "Softer acid than Acid Hash. Env LPF.",
    `param a = Cut [250, 3500]
param b = Res [0.7, 2.8]
param c = Env [200, 2000]
env1: attack = 0.005; release = 0.14
filter1: type = lowpass; cutoff = a + env1 * c; resonance = b
stage1: y = diode(x, 1.3)`,
    opt(p, { a: p("Cut", 250, 3500, 900), b: p("Res", 0.7, 2.8, 1.5), c: p("Env", 200, 2000, 800) }));

  add("Tekno Metal Delay", "Club",
    "Shorter comb than Tekno Comb.",
    `param a = Time [6, 22]
param b = Feedback [0.2, 0.6]
param c = Drive [1, 5]
stage1: y = tube(x, c)
delay1: time = a; feedback = b; mix = 0.28; damp = 4500`,
    opt(p, { a: p("Time", 6, 22, 11), b: p("Feedback", 0.2, 0.6, 0.38), c: p("Drive", 1, 5, 2.2) }));

  add("Industrial Chop", "Club",
    "Gate + clip. Not Industrial Gate stock.",
    `param a = Open [-36, -14]
param b = Drive [1.5, 7]
param c = Hold [0.01, 0.08]
gate1: threshold = a; hyst = 4; hold = c; range = -70
stage1: y = hardclip(softclip(x, b), 0.55)
filter1: type = highpass; cutoff = 100; resonance = 0.3`,
    opt(p, { a: p("Open", -36, -14, -24), b: p("Drive", 1.5, 7, 3.2), c: p("Hold", 0.01, 0.08, 0.03) }));

  add("Hoover Fold Club", "Club",
    "Fold + chorus. Not Hoover Dirt.",
    `param a = Fold [0.18, 0.5]
param b = Rate [0.2, 1.5]
param c = Mix [0.2, 0.6]
stage1: y = fold(x, -a, a)
osc1: shape = sine; freq = b; depth = 1
delay1: time = 11 + osc1 * 6; feedback = 0.08; mix = c; damp = 5000`,
    opt(p, { a: p("Fold", 0.18, 0.5, 0.3), b: p("Rate", 0.2, 1.5, 0.55), c: p("Mix", 0.2, 0.6, 0.32) }));

  add("Rave Stab Desk", "Club",
    "Stab: fast env LPF + clip.",
    `param a = Cut [400, 4000]
param b = Drive [1.2, 6]
param c = Rel [0.05, 0.25]
env1: attack = 0.003; release = c
filter1: type = lowpass; cutoff = a * (0.3 + 0.7 * env1); resonance = 0.8
stage1: y = softclip(x, b)`,
    opt(p, { a: p("Cut", 400, 4000, 1600), b: p("Drive", 1.2, 6, 2.8), c: p("Rel", 0.05, 0.25, 0.12) }));

  add("Club Sidechain Pad", "Club",
    "Pad ducks from kick sidechain.",
    `param a = Depth [0.3, 1]
param b = Release [0.08, 0.35]
param c = Floor [-22, -8]
sidechain1: mix = a
comp1: threshold = c; ratio = 8; attack = 0.001; release = b; source = sidechain`,
    opt(p, { a: p("Depth", 0.3, 1, 0.7), b: p("Release", 0.08, 0.35, 0.16), c: p("Floor", -22, -8, -14) }));

  add("Club OTT Lift", "Club",
    "OTT for club busses. Punchier than OTT Smash.",
    `param a = Depth [0.25, 0.85]
param b = Time [0.008, 0.04]
param c = In [0.8, 1.8]
ott1: depth = a; time = b; in = c; low = 1.1; mid = 0.95; high = 1.05`,
    opt(p, { a: p("Depth", 0.25, 0.85, 0.48), b: p("Time", 0.008, 0.04, 0.016), c: p("In", 0.8, 1.8, 1.15) }));

  add("Club Mono Kick", "Club",
    "Force kick mono below 150 Hz.",
    `param a = BandHz [80, 200]
param b = Weight [0, 4]
param c = Click [0, 3]
xover1: f1 = a
bus low:
  eq1: type = peak; freq = 55; q = 1.05; gain = b
ms1: mode = encode
stage1: channel = side; y = x * 0
ms2: mode = decode
eq2: type = peak; freq = 3000; q = 1.2; gain = c
out: low = 1; high = 1`,
    opt(p, { a: p("BandHz", 80, 200, 140), b: p("Weight", 0, 4, 1.8), c: p("Click", 0, 3, 1.2) }));
}

function registerEdm (add, p, opt)
{
  add("Supersaw Glue", "EDM",
    "Widen + light OTT + clip. Leads / stacks.",
    `param a = Width [0.3, 1]
param b = Depth [0.15, 0.55]
param c = Drive [0.8, 2.2]
widen1: width = a; delay = 12; bass = 160
ott1: depth = b; time = 0.02; in = 1
stage1: y = softclip(x, c)`,
    opt(p, { a: p("Width", 0.3, 1, 0.62), b: p("Depth", 0.15, 0.55, 0.3), c: p("Drive", 0.8, 2.2, 1.2) }));

  add("Pluck Envelope", "EDM",
    "Fast env LPF for plucks.",
    `param a = Cut [400, 5000]
param b = Rel [0.04, 0.22]
param c = Res [0.4, 1.8]
env1: attack = 0.002; release = b
filter1: type = lowpass; cutoff = a * (0.25 + 0.75 * env1); resonance = c`,
    opt(p, { a: p("Cut", 400, 5000, 1800), b: p("Rel", 0.04, 0.22, 0.1), c: p("Res", 0.4, 1.8, 0.9) }));

  add("Drop Impact FX", "EDM",
    "Impact: clip + short verb + click EQ.",
    `param a = Drive [1.5, 7]
param b = Room [0.12, 0.4]
param c = Click [0, 5]
stage1: y = hardclip(softclip(x, a), 0.55)
eq1: type = peak; freq = 80; q = 1.2; gain = c
reverb1: size = b; decay = 0.22; damp = 0.45; mix = 0.16; width = 0.5`,
    opt(p, { a: p("Drive", 1.5, 7, 3.2), b: p("Room", 0.12, 0.4, 0.22), c: p("Click", 0, 5, 2) }));

  add("Build Filter Rise", "EDM",
    "LFO (or automate Cut) opens an LPF.",
    `param a = Floor [200, 1500]
param b = Span [500, 6000]
param c = Rate [0.03, 0.25]
osc1: shape = saw; freq = c; depth = 1
filter1: type = lowpass; cutoff = a + (0.5 + 0.5 * osc1) * b; resonance = 0.75`,
    opt(p, { a: p("Floor", 200, 1500, 500), b: p("Span", 500, 6000, 2800), c: p("Rate", 0.03, 0.25, 0.08) }));

  add("Sidechain Pad EDM", "EDM",
    "Four-on-the-floor duck.",
    `param a = Depth [0.35, 1]
param b = Release [0.1, 0.4]
param c = Floor [-24, -8]
sidechain1: mix = a
comp1: threshold = c; ratio = 10; attack = 0.001; release = b; source = sidechain`,
    opt(p, { a: p("Depth", 0.35, 1, 0.75), b: p("Release", 0.1, 0.4, 0.2), c: p("Floor", -24, -8, -14) }));

  add("Reese Mid Growl", "EDM",
    "Mid grit + chorus. Bass reese.",
    `param a = Grit [1, 5]
param b = Rate [0.15, 1]
param c = BandHz [120, 280]
xover1: f1 = c
bus high:
  stage1: y = diode(x, a)
  osc1: shape = sine; freq = b; depth = 1
  delay1: time = 12 + osc1 * 5; feedback = 0.06; mix = 0.25; damp = 5000
out: low = 1; high = 1`,
    opt(p, { a: p("Grit", 1, 5, 2.4), b: p("Rate", 0.15, 1, 0.4), c: p("BandHz", 120, 280, 180) }));

  add("White Noise Riser", "EDM",
    "HPF sweep on noise / FX.",
    `param a = Cut [200, 4000]
param b = Res [0.3, 1.4]
param c = Drive [0.6, 2]
filter1: type = highpass; cutoff = a; resonance = b
stage1: y = softclip(x, c)`,
    opt(p, { a: p("Cut", 200, 4000, 800), b: p("Res", 0.3, 1.4, 0.6), c: p("Drive", 0.6, 2, 1.1) }));

  add("Festival Group Vocal", "EDM",
    "Gang vocal: widen + short slap + OTT.",
    `param a = Width [0.3, 1]
param b = Depth [0.15, 0.5]
param c = Slap [70, 130]
widen1: width = a; delay = 14; bass = 180
delay1: time = c; feedback = 0.08; mix = 0.16; damp = 4500
ott1: depth = b; time = 0.02; in = 1`,
    opt(p, { a: p("Width", 0.3, 1, 0.55), b: p("Depth", 0.15, 0.5, 0.28), c: p("Slap", 70, 130, 95) }));

  add("Festival Lead Clip", "EDM",
    "Lead: clip + 3 kHz + widen.",
    `param a = Drive [1.2, 5]
param b = Cut [1, 5]
param c = Width [0.2, 0.8]
stage1: y = softclip(x, a)
eq1: type = peak; freq = 3000; q = 1.15; gain = b
widen1: width = c; delay = 11; bass = 200`,
    opt(p, { a: p("Drive", 1.2, 5, 2.4), b: p("Cut", 1, 5, 2.4), c: p("Width", 0.2, 0.8, 0.45) }));

  add("Festival Drum Bus", "EDM",
    "EDM drum bus: OTT + clip + HPF.",
    `param a = Depth [0.2, 0.7]
param b = Drive [1, 4]
param c = Cut [30, 80]
filter1: type = highpass; cutoff = c; resonance = 0.22
ott1: depth = a; time = 0.015; in = 1.1
stage1: y = hardclip(softclip(x, b), 0.72)`,
    opt(p, { a: p("Depth", 0.2, 0.7, 0.38), b: p("Drive", 1, 4, 1.8), c: p("Cut", 30, 80, 45) }));
}

function registerLofi (add, p, opt)
{
  add("Tape Hiss Bed", "Lo-Fi",
    "Dark LPF + wow. Not Cassette.",
    `param a = Top [2500, 7000]
param b = Wow [0.2, 1]
param c = Drive [0.6, 2]
osc1: shape = sine; freq = b; depth = 1
delay1: time = 16 + osc1 * 3; feedback = 0.08; mix = 0.2; damp = 4000
stage1: y = tube(x, c)
filter1: type = lowpass; cutoff = a; resonance = 0.28`,
    opt(p, { a: p("Top", 2500, 7000, 4200), b: p("Wow", 0.2, 1, 0.4), c: p("Drive", 0.6, 2, 1.1) }));

  add("AM Radio Narrow", "Lo-Fi",
    "Narrower than Phone Line. 500–2.8 kHz.",
    `param a = Low [350, 700]
param b = High [2000, 3500]
param c = Bits [6, 11]
filter1: type = highpass; cutoff = a; resonance = 0.4
filter2: type = lowpass; cutoff = b; resonance = 0.35
stage1: y = bitcrush(softclip(x, 1.3), c)`,
    opt(p, { a: p("Low", 350, 700, 500), b: p("High", 2000, 3500, 2800), c: p("Bits", 6, 11, 8) }));

  add("VHS Tracking", "Lo-Fi",
    "Wow + dark + tiny crush. Video tape.",
    `param a = Wow [0.15, 0.8]
param b = Bits [6, 12]
param c = Top [2000, 5500]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 20 + osc1 * 4; feedback = 0.1; mix = 0.18; damp = 3500
stage1: y = bitcrush(x, b)
filter1: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Wow", 0.15, 0.8, 0.35), b: p("Bits", 6, 12, 9), c: p("Top", 2000, 5500, 3400) }));

  add("Broken Speaker", "Lo-Fi",
    "300–2.5 kHz + fold.",
    `param a = Fold [0.12, 0.4]
param b = Low [200, 450]
param c = High [1800, 3500]
filter1: type = highpass; cutoff = b; resonance = 0.4
filter2: type = lowpass; cutoff = c; resonance = 0.42
stage1: y = fold(x, -a, a)`,
    opt(p, { a: p("Fold", 0.12, 0.4, 0.22), b: p("Low", 200, 450, 300), c: p("High", 1800, 3500, 2500) }));

  add("Dusty Vinyl Bed", "Lo-Fi",
    "Notch + dark + gentle clip. Not Vinyl Dirt.",
    `param a = Scratch [400, 2000]
param b = Top [3000, 7000]
param c = Dust [0.6, 1.8]
eq1: type = notch; freq = a; q = 2.4; gain = -8
filter1: type = lowpass; cutoff = b; resonance = 0.28
stage1: y = diode(x, c)`,
    opt(p, { a: p("Scratch", 400, 2000, 900), b: p("Top", 3000, 7000, 4800), c: p("Dust", 0.6, 1.8, 1.05) }));

  add("Pocket Recorder", "Lo-Fi",
    "8 kHz LPF, light crush, HPF 120.",
    `param a = Bits [6, 12]
param b = Top [4000, 9000]
param c = Cut [80, 200]
filter1: type = highpass; cutoff = c; resonance = 0.25
stage1: y = bitcrush(softclip(x, 1.2), a)
filter2: type = lowpass; cutoff = b; resonance = 0.3`,
    opt(p, { a: p("Bits", 6, 12, 9), b: p("Top", 4000, 9000, 6500), c: p("Cut", 80, 200, 120) }));

  add("Wow Only Tape", "Lo-Fi",
    "Just wow, almost no tone loss.",
    `param a = Rate [0.25, 1.4]
param b = Depth [1, 7]
param c = Mix [0.2, 0.8]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 14 + osc1 * b; feedback = 0.06; mix = c; damp = 7000`,
    opt(p, { a: p("Rate", 0.25, 1.4, 0.55), b: p("Depth", 1, 7, 3.2), c: p("Mix", 0.2, 0.8, 0.4) }));

  add("Flutter Only Tape", "Lo-Fi",
    "Faster than wow. Capstan flutter.",
    `param a = Rate [4, 12]
param b = Depth [0.5, 3]
param c = Mix [0.15, 0.6]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 12 + osc1 * b; feedback = 0.05; mix = c; damp = 6500`,
    opt(p, { a: p("Rate", 4, 12, 7), b: p("Depth", 0.5, 3, 1.4), c: p("Mix", 0.15, 0.6, 0.32) }));

  add("Telephone Then Room", "Lo-Fi",
    "Phone band plus a cheap room.",
    `param a = Low [280, 600]
param b = High [2400, 4000]
param c = Room [0.1, 0.35]
filter1: type = highpass; cutoff = a; resonance = 0.4
filter2: type = lowpass; cutoff = b; resonance = 0.35
reverb1: size = c; decay = 0.25; damp = 0.45; mix = 0.18; width = 0.4`,
    opt(p, { a: p("Low", 280, 600, 400), b: p("High", 2400, 4000, 3200), c: p("Room", 0.1, 0.35, 0.2) }));

  add("Chip Tune Crush", "Lo-Fi",
    "4–7 bits + 5 kHz. Game / chiptune.",
    `param a = Bits [3, 8]
param b = Top [2500, 7000]
param c = Drive [0.8, 2.4]
stage1: y = bitcrush(softclip(x, c), a)
filter1: type = lowpass; cutoff = b; resonance = 0.4`,
    opt(p, { a: p("Bits", 3, 8, 5), b: p("Top", 2500, 7000, 4500), c: p("Drive", 0.8, 2.4, 1.3) }));

  add("Worn Cassette 2", "Lo-Fi",
    "Darker cassette: 3 kHz LPF + wow + tube.",
    `param a = Top [1800, 4500]
param b = Wow [0.2, 0.9]
param c = Warm [0.6, 1.8]
osc1: shape = sine; freq = b; depth = 1
delay1: time = 18 + osc1 * 3; feedback = 0.1; mix = 0.16; damp = 3000
stage1: y = tube(x, c)
filter1: type = lowpass; cutoff = a; resonance = 0.3`,
    opt(p, { a: p("Top", 1800, 4500, 2800), b: p("Wow", 0.2, 0.9, 0.4), c: p("Warm", 0.6, 1.8, 1.05) }));
}

function registerAmbient (add, p, opt)
{
  add("Pad Cloud Hall", "Ambient",
    "Long hall + slow chorus. Pads.",
    `param a = Size [0.45, 0.88]
param b = Rate [0.05, 0.4]
param c = Mix [0.2, 0.6]
reverb1: size = a; decay = 2.4; damp = 0.4; mix = c; width = 0.85
osc1: shape = sine; freq = b; depth = 1
delay1: time = 16 + osc1 * 8; feedback = 0.08; mix = 0.2; damp = 6000`,
    opt(p, { a: p("Size", 0.45, 0.88, 0.66), b: p("Rate", 0.05, 0.4, 0.14), c: p("Mix", 0.2, 0.6, 0.36) }));

  add("Drone Low Bed", "Ambient",
    "Dark drone: LPF + long verb + octave sub.",
    `param a = Sub [0.1, 0.5]
param b = Top [400, 1600]
param c = Hall [0.4, 0.85]
octaver1: mix = a; sub = 1; up = 0; tone = 120; thresh = 0.05
filter1: type = lowpass; cutoff = b; resonance = 0.3
reverb1: size = c; decay = 2.6; damp = 0.6; mix = 0.4; width = 0.7`,
    opt(p, { a: p("Sub", 0.1, 0.5, 0.25), b: p("Top", 400, 1600, 900), c: p("Hall", 0.4, 0.85, 0.62) }));

  add("Ice Crystal Verb", "Ambient",
    "Bright long verb + up octave. Not Crystal Edge.",
    `param a = Size [0.4, 0.8]
param b = Octave [0.1, 0.45]
param c = Mix [0.2, 0.55]
stage1: y = x
bus ice:
  send: in = 1
  octaver1: mix = b; sub = 0; up = 1; tone = 420; thresh = 0.05
  reverb1: size = a; decay = 2.2; damp = 0.28; mix = 1; width = 0.88
out: main = 1-c; ice = c`,
    opt(p, { a: p("Size", 0.4, 0.8, 0.58), b: p("Octave", 0.1, 0.45, 0.24), c: p("Mix", 0.2, 0.55, 0.32) }));

  add("Fog Lowpass", "Ambient",
    "Just a slow darkening. Mix / pad.",
    `param a = Top [800, 5000]
param b = Res [0.2, 0.8]
param c = Mix [0.4, 1]
filter1: type = lowpass; cutoff = a; resonance = b
stage1: y = tube(x, 0.7)
stage2: y = lerp(x, y, c)`,
    opt(p, { a: p("Top", 800, 5000, 2200), b: p("Res", 0.2, 0.8, 0.35), c: p("Mix", 0.4, 1, 0.85) }));

  add("Night Hall", "Ambient",
    "Darker than Dark Ambient Verb. 3 kHz LPF on the tail.",
    `param a = Size [0.4, 0.8]
param b = Decay [1, 2.8]
param c = Mix [0.2, 0.55]
reverb1: size = a; decay = b; damp = 0.65; mix = c; width = 0.8
filter1: type = lowpass; cutoff = 3200; resonance = 0.22`,
    opt(p, { a: p("Size", 0.4, 0.8, 0.58), b: p("Decay", 1, 2.8, 1.7), c: p("Mix", 0.2, 0.55, 0.34) }));

  add("Slow Swell Ambient", "Ambient",
    "Env opens a hall. Guitar / keys.",
    `param a = Rise [0.08, 0.45]
param b = Size [0.35, 0.8]
param c = Mix [0.25, 0.7]
env1: attack = a; release = 0.4
stage1: y = x * (0.12 + 0.88 * env1)
reverb1: size = b; decay = 2.0; damp = 0.42; mix = c; width = 0.82`,
    opt(p, { a: p("Rise", 0.08, 0.45, 0.18), b: p("Size", 0.35, 0.8, 0.55), c: p("Mix", 0.25, 0.7, 0.42) }));

  add("Distant Radio Amb", "Ambient",
    "Bandpass + long verb. Memory / flashback.",
    `param a = Center [700, 1800]
param b = Size [0.35, 0.75]
param c = Mix [0.2, 0.6]
filter1: type = bandpass; center = a; width = 900
reverb1: size = b; decay = 1.8; damp = 0.45; mix = c; width = 0.75`,
    opt(p, { a: p("Center", 700, 1800, 1100), b: p("Size", 0.35, 0.75, 0.52), c: p("Mix", 0.2, 0.6, 0.35) }));

  add("Tidal Delay Amb", "Ambient",
    "Long dotted delay into a hall.",
    `param a = Feedback [0.25, 0.55]
param b = Size [0.3, 0.7]
param c = Mix [0.2, 0.55]
delay1: time = 1/4.; feedback = a; mix = 0.35; damp = 3500
reverb1: size = b; decay = 1.6; damp = 0.45; mix = c; width = 0.78`,
    opt(p, { a: p("Feedback", 0.25, 0.55, 0.36), b: p("Size", 0.3, 0.7, 0.48), c: p("Mix", 0.2, 0.55, 0.32) }));

  add("Glass Plate Amb", "Ambient",
    "Bright plate, long, no octave.",
    `param a = Size [0.28, 0.6]
param b = Decay [0.8, 2.2]
param c = Mix [0.18, 0.5]
filter1: type = highpass; cutoff = 220; resonance = 0.22
reverb1: size = a; decay = b; damp = 0.26; mix = c; width = 0.8`,
    opt(p, { a: p("Size", 0.28, 0.6, 0.42), b: p("Decay", 0.8, 2.2, 1.3), c: p("Mix", 0.18, 0.5, 0.3) }));

  add("Wind Highpass Amb", "Ambient",
    "HPF + air verb. Wind / air beds.",
    `param a = Cut [200, 1200]
param b = Size [0.3, 0.7]
param c = Mix [0.2, 0.55]
filter1: type = highpass; cutoff = a; resonance = 0.28
reverb1: size = b; decay = 1.5; damp = 0.32; mix = c; width = 0.85`,
    opt(p, { a: p("Cut", 200, 1200, 500), b: p("Size", 0.3, 0.7, 0.48), c: p("Mix", 0.2, 0.55, 0.32) }));

  add("Soft Chorus Cloud", "Ambient",
    "Very slow chorus, no delay slap.",
    `param a = Rate [0.04, 0.25]
param b = Depth [8, 22]
param c = Mix [0.25, 0.7]
osc1: shape = sine; freq = a; depth = 1
delay1: time = 18 + osc1 * b; feedback = 0.1; mix = c; damp = 6500`,
    opt(p, { a: p("Rate", 0.04, 0.25, 0.1), b: p("Depth", 8, 22, 14), c: p("Mix", 0.25, 0.7, 0.42) }));

  add("Underwater LPF", "Ambient",
    "Deep LPF + short room. Underwater.",
    `param a = Top [400, 1800]
param b = Room [0.15, 0.45]
param c = Mix [0.2, 0.6]
filter1: type = lowpass; cutoff = a; resonance = 0.35
reverb1: size = b; decay = 0.7; damp = 0.7; mix = c; width = 0.5`,
    opt(p, { a: p("Top", 400, 1800, 900), b: p("Room", 0.15, 0.45, 0.28), c: p("Mix", 0.2, 0.6, 0.35) }));
}

function registerCinematic (add, p, opt)
{
  add("Impact Body Boom", "Cinematic",
    "Impact: sub bump + short clip. Not Boom Tail.",
    `param a = Sub [1, 6]
param b = Drive [1, 5]
param c = Room [0.1, 0.35]
eq1: type = peak; freq = 45; q = 1.2; gain = a
stage1: y = hardclip(softclip(x, b), 0.6)
reverb1: size = c; decay = 0.28; damp = 0.55; mix = 0.18; width = 0.45`,
    opt(p, { a: p("Sub", 1, 6, 3), b: p("Drive", 1, 5, 2.2), c: p("Room", 0.1, 0.35, 0.2) }));

  add("Dialogue Phone", "Cinematic",
    "Phone filter for dialogue. Not Dialogue Seat.",
    `param a = Low [300, 700]
param b = High [2500, 4200]
param c = Grit [0.5, 2]
filter1: type = highpass; cutoff = a; resonance = 0.4
filter2: type = lowpass; cutoff = b; resonance = 0.35
stage1: y = diode(x, c)`,
    opt(p, { a: p("Low", 300, 700, 420), b: p("High", 2500, 4200, 3200), c: p("Grit", 0.5, 2, 1.0) }));

  add("Far Plane Darker", "Cinematic",
    "More HPF and darker than Far Plane.",
    `param a = Cut [250, 800]
param b = Size [0.35, 0.75]
param c = Mix [0.2, 0.55]
filter1: type = highpass; cutoff = a; resonance = 0.25
reverb1: size = b; decay = 1.4; damp = 0.58; mix = c; width = 0.7`,
    opt(p, { a: p("Cut", 250, 800, 420), b: p("Size", 0.35, 0.75, 0.52), c: p("Mix", 0.2, 0.55, 0.32) }));

  add("Tension Pulse", "Cinematic",
    "Slow trem + dark hall. Not Tension Bed.",
    `param a = Rate [0.15, 1.2]
param b = Size [0.3, 0.7]
param c = Depth [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
stage1: y = x * (1 - c + c * (0.5 + 0.5 * osc1))
reverb1: size = b; decay = 1.6; damp = 0.55; mix = 0.3; width = 0.65`,
    opt(p, { a: p("Rate", 0.15, 1.2, 0.4), b: p("Size", 0.3, 0.7, 0.48), c: p("Depth", 0.2, 0.7, 0.4) }));

  add("Score String Hall", "Cinematic",
    "Strings: long hall, no octave. Not Score Hall.",
    `param a = Size [0.4, 0.82]
param b = Decay [1.2, 2.8]
param c = Mix [0.18, 0.5]
filter1: type = highpass; cutoff = 80; resonance = 0.2
reverb1: size = a; decay = b; damp = 0.42; mix = c; width = 0.8`,
    opt(p, { a: p("Size", 0.4, 0.82, 0.6), b: p("Decay", 1.2, 2.8, 1.8), c: p("Mix", 0.18, 0.5, 0.3) }));

  add("Wide Canvas Soft", "Cinematic",
    "Softer widen than Wide Canvas.",
    `param a = Width [0.25, 0.85]
param b = Haas [8, 20]
param c = Air [0, 3]
widen1: width = a; delay = b; bass = 150
eq1: type = highshelf; freq = 9000; q = 0.5; gain = c`,
    opt(p, { a: p("Width", 0.25, 0.85, 0.5), b: p("Haas", 8, 20, 13), c: p("Air", 0, 3, 1.2) }));

  add("Trailer Whoosh", "Cinematic",
    "HPF sweep + short verb. Whoosh / fly-by.",
    `param a = Cut [200, 2500]
param b = Room [0.12, 0.4]
param c = Drive [0.6, 2.2]
filter1: type = highpass; cutoff = a; resonance = 0.45
stage1: y = softclip(x, c)
reverb1: size = b; decay = 0.35; damp = 0.4; mix = 0.22; width = 0.7`,
    opt(p, { a: p("Cut", 200, 2500, 700), b: p("Room", 0.12, 0.4, 0.22), c: p("Drive", 0.6, 2.2, 1.1) }));

  add("Horror Low Drone", "Cinematic",
    "Sub + dark verb. Horror bed.",
    `param a = Sub [0.15, 0.6]
param b = Top [300, 1200]
param c = Hall [0.4, 0.85]
octaver1: mix = a; sub = 1; up = 0; tone = 100; thresh = 0.05
filter1: type = lowpass; cutoff = b; resonance = 0.32
reverb1: size = c; decay = 2.4; damp = 0.7; mix = 0.38; width = 0.6`,
    opt(p, { a: p("Sub", 0.15, 0.6, 0.32), b: p("Top", 300, 1200, 600), c: p("Hall", 0.4, 0.85, 0.62) }));

  add("Dialogue Air Seat", "Cinematic",
    "Dialogue: HPF 80, air, tiny booth.",
    `param a = Air [0, 4]
param b = Cut [60, 140]
param c = Booth [0.08, 0.25]
filter1: type = highpass; cutoff = b; resonance = 0.22
eq1: type = highshelf; freq = 8000; q = 0.5; gain = a
reverb1: size = 0.16; decay = c; damp = 0.48; mix = 0.12; width = 0.35`,
    opt(p, { a: p("Air", 0, 4, 1.6), b: p("Cut", 60, 140, 85), c: p("Booth", 0.08, 0.25, 0.14) }));
}

function registerCreative (add, p, opt)
{
  add("Infinite Repeat", "Creative",
    "High feedback delay, dark. Not Feedback Screamer.",
    `param a = Feedback [0.45, 0.78]
param b = Time [180, 600]
param c = Tone [1500, 4500]
delay1: time = b; feedback = a; mix = 0.45; damp = c
filter1: type = highpass; cutoff = 160; resonance = 0.25`,
    opt(p, { a: p("Feedback", 0.45, 0.78, 0.6), b: p("Time", 180, 600, 320), c: p("Tone", 1500, 4500, 2800) }));

  add("Glitch Hold Gate", "Creative",
    "Choppy gate, 1/16. Not Glitch Gate.",
    `param a = Depth [0.5, 1]
param b = Open [-30, -8]
param c = Mix [0.3, 1]
osc1: shape = square; freq = 1/16; depth = 1
gate1: threshold = b; hyst = 3; hold = 0.01; range = -70
stage1: y = lerp(x, x * (1 - a + a * (0.05 + 0.95 * (0.5 + 0.5 * osc1))), c)`,
    opt(p, { a: p("Depth", 0.5, 1, 0.85), b: p("Open", -30, -8, -18), c: p("Mix", 0.3, 1, 0.8) }));

  add("Alien Formant", "Creative",
    "Moving formant. Not Alien Ring.",
    `param a = Rate [0.1, 2]
param b = Center [400, 1400]
param c = Mix [0.25, 0.9]
osc1: shape = sine; freq = a; depth = 1
stage1: y = x
bus mouth:
  send: in = 1
  filter1: type = bandpass; center = b + osc1 * 400; width = 220; resonance = 1.5
out: main = 1-c; mouth = c`,
    opt(p, { a: p("Rate", 0.1, 2, 0.45), b: p("Center", 400, 1400, 800), c: p("Mix", 0.25, 0.9, 0.55) }));

  add("Stutter Repeat", "Creative",
    "Very short delay, high mix. Stutter.",
    `param a = Time [8, 40]
param b = Feedback [0.2, 0.65]
param c = Mix [0.25, 0.85]
delay1: time = a; feedback = b; mix = c; damp = 6000`,
    opt(p, { a: p("Time", 8, 40, 18), b: p("Feedback", 0.2, 0.65, 0.4), c: p("Mix", 0.25, 0.85, 0.5) }));

  add("Freeze-ish Wash", "Creative",
    "Long time, high feedback, LPF. Pseudo freeze.",
    `param a = Time [400, 1200]
param b = Feedback [0.4, 0.75]
param c = Top [1500, 5000]
delay1: time = a; feedback = b; mix = 0.5; damp = c
filter1: type = lowpass; cutoff = c; resonance = 0.28`,
    opt(p, { a: p("Time", 400, 1200, 720), b: p("Feedback", 0.4, 0.75, 0.58), c: p("Top", 1500, 5000, 3000) }));

  add("Ring Then Verb", "Creative",
    "Ring mod into a hall.",
    `param a = Freq [50, 300]
param b = Size [0.25, 0.65]
param c = Mix [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
stage1: y = lerp(x, x * osc1, 0.55)
reverb1: size = b; decay = 1.1; damp = 0.4; mix = c; width = 0.7`,
    opt(p, { a: p("Freq", 50, 300, 110), b: p("Size", 0.25, 0.65, 0.42), c: p("Mix", 0.2, 0.7, 0.35) }));

  add("Cascade Taps", "Creative",
    "Three delay times stacked. Not Cascade Loop Dirt.",
    `param a = Mix [0.15, 0.5]
param b = Feedback [0.08, 0.3]
param c = Tone [2500, 7000]
delay1: time = 1/16; feedback = b; mix = a * 0.5; damp = c
delay2: time = 1/8; feedback = b; mix = a * 0.4; damp = c
delay3: time = 1/4; feedback = b * 0.8; mix = a * 0.35; damp = c`,
    opt(p, { a: p("Mix", 0.15, 0.5, 0.28), b: p("Feedback", 0.08, 0.3, 0.16), c: p("Tone", 2500, 7000, 4200) }));

  add("Noise Blow Soft", "Creative",
    "HPF + fold, softer than Noise Blow.",
    `param a = Cut [200, 2000]
param b = Fold [0.1, 0.4]
param c = Mix [0.2, 0.8]
filter1: type = highpass; cutoff = a; resonance = 0.35
stage1: y = lerp(x, fold(x, -b, b), c)`,
    opt(p, { a: p("Cut", 200, 2000, 700), b: p("Fold", 0.1, 0.4, 0.22), c: p("Mix", 0.2, 0.8, 0.45) }));

  add("Comb Taste Soft", "Creative",
    "Longer comb, less feedback than Comb Taste.",
    `param a = Time [12, 48]
param b = Feedback [0.15, 0.5]
param c = Mix [0.15, 0.6]
delay1: time = a; feedback = b; mix = c; damp = 5500`,
    opt(p, { a: p("Time", 12, 48, 24), b: p("Feedback", 0.15, 0.5, 0.28), c: p("Mix", 0.15, 0.6, 0.32) }));
}

function registerSoundDesign (add, p, opt)
{
  add("Impact Designer", "Sound Design",
    "Design hits: clip, sub, short room.",
    `param a = Drive [1.2, 7]
param b = Sub [0, 6]
param c = Room [0.08, 0.32]
stage1: y = hardclip(softclip(x, a), 0.55)
eq1: type = peak; freq = 50; q = 1.3; gain = b
reverb1: size = c; decay = 0.2; damp = 0.5; mix = 0.15; width = 0.4`,
    opt(p, { a: p("Drive", 1.2, 7, 3), b: p("Sub", 0, 6, 2.4), c: p("Room", 0.08, 0.32, 0.16) }));

  add("Riser Designer", "Sound Design",
    "HPF + resonance + verb. Automate Cut.",
    `param a = Cut [150, 4000]
param b = Res [0.4, 2]
param c = Hall [0.2, 0.6]
filter1: type = highpass; cutoff = a; resonance = b
reverb1: size = c; decay = 1.0; damp = 0.4; mix = 0.28; width = 0.75`,
    opt(p, { a: p("Cut", 150, 4000, 600), b: p("Res", 0.4, 2, 0.9), c: p("Hall", 0.2, 0.6, 0.36) }));

  add("Downer Designer", "Sound Design",
    "LPF + long delay. Downer / collapse.",
    `param a = Top [400, 4000]
param b = Time [300, 900]
param c = Feedback [0.2, 0.55]
filter1: type = lowpass; cutoff = a; resonance = 0.4
delay1: time = b; feedback = c; mix = 0.35; damp = 3000`,
    opt(p, { a: p("Top", 400, 4000, 1400), b: p("Time", 300, 900, 520), c: p("Feedback", 0.2, 0.55, 0.34) }));

  add("Texture Crush", "Sound Design",
    "Crush + bandpass. Texture layer.",
    `param a = Bits [4, 10]
param b = Center [400, 2000]
param c = Width [200, 1200]
stage1: y = bitcrush(softclip(x, 1.4), a)
filter1: type = bandpass; center = b; width = c`,
    opt(p, { a: p("Bits", 4, 10, 7), b: p("Center", 400, 2000, 900), c: p("Width", 200, 1200, 500) }));

  add("Sub Drop FX", "Sound Design",
    "Octave sub + clip. Drop FX.",
    `param a = Sub [0.3, 0.9]
param b = Drive [1, 5]
param c = Top [80, 200]
octaver1: mix = a; sub = 1; up = 0; tone = 90; thresh = 0.04
stage1: y = hardclip(softclip(x, b), 0.65)
filter1: type = lowpass; cutoff = c; resonance = 0.25`,
    opt(p, { a: p("Sub", 0.3, 0.9, 0.55), b: p("Drive", 1, 5, 2.2), c: p("Top", 80, 200, 130) }));

  add("Metal Hit Ring", "Sound Design",
    "Short comb + clip. Metal hit.",
    `param a = Time [5, 20]
param b = Drive [1.2, 6]
param c = Mix [0.2, 0.7]
stage1: y = hardclip(softclip(x, b), 0.5)
delay1: time = a; feedback = 0.35; mix = c; damp = 5000`,
    opt(p, { a: p("Time", 5, 20, 10), b: p("Drive", 1.2, 6, 2.8), c: p("Mix", 0.2, 0.7, 0.4) }));

  add("Air Whoosh SD", "Sound Design",
    "HPF + air verb. Whoosh layer.",
    `param a = Cut [300, 2500]
param b = Size [0.2, 0.55]
param c = Mix [0.2, 0.55]
filter1: type = highpass; cutoff = a; resonance = 0.4
reverb1: size = b; decay = 0.55; damp = 0.3; mix = c; width = 0.8`,
    opt(p, { a: p("Cut", 300, 2500, 800), b: p("Size", 0.2, 0.55, 0.34), c: p("Mix", 0.2, 0.55, 0.32) }));

  add("Glitch Buffer", "Sound Design",
    "Tiny delay, square AM. Buffer glitch.",
    `param a = Time [6, 30]
param b = Rate [4, 16]
param c = Mix [0.25, 0.85]
osc1: shape = square; freq = b; depth = 1
delay1: time = a; feedback = 0.25; mix = c; damp = 6000
stage1: y = x * (0.2 + 0.8 * (0.5 + 0.5 * osc1))`,
    opt(p, { a: p("Time", 6, 30, 14), b: p("Rate", 4, 16, 8), c: p("Mix", 0.25, 0.85, 0.5) }));

  add("Body Resonator", "Sound Design",
    "Resonant bandpass as a body.",
    `param a = Center [80, 400]
param b = Q [0.8, 2.8]
param c = Mix [0.2, 0.8]
stage1: y = x
bus body:
  send: in = 1
  filter1: type = bandpass; center = a; width = 80; resonance = b
out: main = 1-c; body = c`,
    opt(p, { a: p("Center", 80, 400, 160), b: p("Q", 0.8, 2.8, 1.5), c: p("Mix", 0.2, 0.8, 0.4) }));
}

function registerSynth (add, p, opt)
{
  add("Lead Presence Synth", "Synth",
    "Lead: 2.8 kHz, clip, widen.",
    `param a = Drive [0.8, 4]
param b = Cut [1, 5]
param c = Width [0.15, 0.7]
stage1: y = softclip(x, a)
eq1: type = peak; freq = 2800; q = 1.15; gain = b
widen1: width = c; delay = 11; bass = 180`,
    opt(p, { a: p("Drive", 0.8, 4, 1.8), b: p("Cut", 1, 5, 2.4), c: p("Width", 0.15, 0.7, 0.38) }));

  add("Pad Widen Synth", "Synth",
    "Pad: widen + slow chorus + hall.",
    `param a = Width [0.25, 0.9]
param b = Rate [0.05, 0.4]
param c = Hall [0.2, 0.55]
widen1: width = a; delay = 14; bass = 160
osc1: shape = sine; freq = b; depth = 1
delay1: time = 15 + osc1 * 7; feedback = 0.08; mix = 0.22; damp = 6500
reverb1: size = 0.4; decay = c; damp = 0.42; mix = 0.22; width = 0.8`,
    opt(p, { a: p("Width", 0.25, 0.9, 0.55), b: p("Rate", 0.05, 0.4, 0.14), c: p("Hall", 0.2, 0.55, 0.34) }));

  add("Bass Reese Synth", "Synth",
    "Reese: mid grit + chorus above 150 Hz.",
    `param a = Grit [1, 4.5]
param b = Rate [0.2, 1.2]
param c = BandHz [100, 220]
xover1: f1 = c
bus high:
  stage1: y = diode(x, a)
  osc1: shape = sine; freq = b; depth = 1
  delay1: time = 11 + osc1 * 5; feedback = 0.06; mix = 0.22; damp = 4800
out: low = 1; high = 1`,
    opt(p, { a: p("Grit", 1, 4.5, 2.1), b: p("Rate", 0.2, 1.2, 0.45), c: p("BandHz", 100, 220, 150) }));

  add("Pluck Desk Synth", "Synth",
    "Pluck: env LPF + tiny room.",
    `param a = Cut [500, 5000]
param b = Rel [0.05, 0.25]
param c = Room [0.08, 0.3]
env1: attack = 0.002; release = b
filter1: type = lowpass; cutoff = a * (0.3 + 0.7 * env1); resonance = 0.7
reverb1: size = 0.18; decay = c; damp = 0.4; mix = 0.14; width = 0.55`,
    opt(p, { a: p("Cut", 500, 5000, 2000), b: p("Rel", 0.05, 0.25, 0.1), c: p("Room", 0.08, 0.3, 0.16) }));

  add("Keys Rhodes Trem", "Synth",
    "Keys: sine trem + warm tube.",
    `param a = Rate [2, 7]
param b = Depth [0.2, 0.7]
param c = Warm [0.4, 1.6]
osc1: shape = sine; freq = a; depth = 1
stage1: y = tube(x * (1 - b + b * (0.5 + 0.5 * osc1)), c)`,
    opt(p, { a: p("Rate", 2, 7, 4), b: p("Depth", 0.2, 0.7, 0.4), c: p("Warm", 0.4, 1.6, 0.85) }));

  add("Keys Piano Hall", "Synth",
    "Piano: HPF 40, small hall, gentle glue.",
    `param a = Glue [3, 9]
param b = Size [0.18, 0.45]
param c = Mix [0.08, 0.32]
filter1: type = highpass; cutoff = 40; resonance = 0.2
comp1: threshold = -a; ratio = 2.5; attack = 0.012; release = 0.16; makeup = 1
reverb1: size = b; decay = 0.7; damp = 0.42; mix = c; width = 0.65`,
    opt(p, { a: p("Glue", 3, 9, 5), b: p("Size", 0.18, 0.45, 0.28), c: p("Mix", 0.08, 0.32, 0.16) }));

  add("Organ Rotary Synth", "Synth",
    "Organ: rotary AM + bandpass.",
    `param a = Rate [0.6, 7]
param b = Horn [400, 2000]
param c = AM [0.2, 0.7]
osc1: shape = sine; freq = a; depth = 1
filter1: type = bandpass; center = 1000 + osc1 * b; width = 800; resonance = 0.55
stage1: y = x * (1 - c + c * (0.5 + 0.5 * osc1))`,
    opt(p, { a: p("Rate", 0.6, 7, 1.2), b: p("Horn", 400, 2000, 1000), c: p("AM", 0.2, 0.7, 0.4) }));

  add("Arp Sparkle Synth", "Synth",
    "Arp: HPF, delay 1/16, air.",
    `param a = Cut [80, 300]
param b = Mix [0.12, 0.4]
param c = Air [0, 4]
filter1: type = highpass; cutoff = a; resonance = 0.25
delay1: time = 1/16; feedback = 0.18; mix = b; damp = 7000
eq1: type = highshelf; freq = 8000; q = 0.5; gain = c`,
    opt(p, { a: p("Cut", 80, 300, 140), b: p("Mix", 0.12, 0.4, 0.22), c: p("Air", 0, 4, 1.8) }));

  add("Brass Stab Synth", "Synth",
    "Brass: fast env + clip + 800 Hz.",
    `param a = Drive [1, 5]
param b = Rel [0.06, 0.25]
param c = Honk [1, 5]
env1: attack = 0.004; release = b
stage1: y = softclip(x * (0.4 + 0.6 * env1), a)
eq1: type = peak; freq = 800; q = 1.1; gain = c`,
    opt(p, { a: p("Drive", 1, 5, 2.2), b: p("Rel", 0.06, 0.25, 0.12), c: p("Honk", 1, 5, 2.4) }));

  add("String Lush Synth", "Synth",
    "Strings: widen + long hall.",
    `param a = Width [0.25, 0.85]
param b = Size [0.35, 0.75]
param c = Mix [0.15, 0.45]
widen1: width = a; delay = 13; bass = 150
reverb1: size = b; decay = 1.8; damp = 0.4; mix = c; width = 0.82`,
    opt(p, { a: p("Width", 0.25, 0.85, 0.5), b: p("Size", 0.35, 0.75, 0.52), c: p("Mix", 0.15, 0.45, 0.26) }));

  add("FX Swoosh Synth", "Synth",
    "FX: LFO HPF + verb.",
    `param a = Rate [0.05, 0.4]
param b = Span [300, 3000]
param c = Hall [0.2, 0.55]
osc1: shape = saw; freq = a; depth = 1
filter1: type = highpass; cutoff = 200 + (0.5 + 0.5 * osc1) * b; resonance = 0.5
reverb1: size = c; decay = 1.0; damp = 0.38; mix = 0.28; width = 0.75`,
    opt(p, { a: p("Rate", 0.05, 0.4, 0.12), b: p("Span", 300, 3000, 1400), c: p("Hall", 0.2, 0.55, 0.34) }));

  add("Poly Glue Synth", "Synth",
    "Poly stack glue: OTT light + clip.",
    `param a = Depth [0.12, 0.5]
param b = Drive [0.7, 2]
param c = Width [0.2, 0.7]
ott1: depth = a; time = 0.025; in = 1
stage1: y = softclip(x, b)
widen1: width = c; delay = 10; bass = 170`,
    opt(p, { a: p("Depth", 0.12, 0.5, 0.26), b: p("Drive", 0.7, 2, 1.1), c: p("Width", 0.2, 0.7, 0.4) }));

  add("Mono Lead Dirt", "Synth",
    "Mono lead: tube + 1.5 kHz + no widen.",
    `param a = Drive [1, 5]
param b = Cut [1, 5]
param c = Top [3000, 8000]
stage1: y = tube(x, a)
eq1: type = peak; freq = 1500; q = 1.15; gain = b
filter1: type = lowpass; cutoff = c; resonance = 0.3`,
    opt(p, { a: p("Drive", 1, 5, 2.4), b: p("Cut", 1, 5, 2.6), c: p("Top", 3000, 8000, 5500) }));

  add("Bell Sparkle Synth", "Synth",
    "Bells: HPF, short delay, air.",
    `param a = Cut [200, 800]
param b = Mix [0.1, 0.4]
param c = Air [1, 5]
filter1: type = highpass; cutoff = a; resonance = 0.25
delay1: time = 1/8; feedback = 0.14; mix = b; damp = 7500
eq1: type = highshelf; freq = 7000; q = 0.5; gain = c`,
    opt(p, { a: p("Cut", 200, 800, 380), b: p("Mix", 0.1, 0.4, 0.2), c: p("Air", 1, 5, 2.4) }));
}




