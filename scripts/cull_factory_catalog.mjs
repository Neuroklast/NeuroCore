/**
 * One-shot catalog pass: drop cheap/generic jobs, rename fake brands,
 * keep Mesa High Gain and every Vocals preset.
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const src = path.join(root, "resources", "factory_presets.json");
const namesOut = path.join(root, "resources", "_preset_names.txt");

const RENAME = new Map([
  ["Fender Clean", "Airy Clean"],
  ["Marshall Crunch", "Plexi Crunch"],
  ["Vox Top Boost", "Top Boost Chime"],
  ["Tube Screamer", "Mid Boost OD"],
  ["Klon Centaur", "Transparent Blend"],
  ["ProCo RAT", "Filter Dist"],
  ["Fuzz Face", "Germanium Fuzz"],
  ["Uni-Vibe AM", "Optical Vibe"],
  ["Orange Crush", "British Mid Grind"],
  ["JCM Hot Lead", "Hot British Lead"],
  ["SLO Crunch", "American Crunch"],
  ["SVT Grind", "Foldback Bass"],
  ["AC30 Chime", "Chime Combo"],
  ["5150 Lead", "Tight Lead Stack"],
  ["Bassman Grind", "Tweed Bass Grind"],
  ["B15 Flip", "Flip-Top Bass"],
  ["Trace Filter", "Envelope Bass Filter"],
  ["1176 FET", "FET Peak Comp"],
  ["1176 All In", "All Buttons Comp"],
  ["LA-2A Opto", "Optical Leveler Comp"],
  ["SSL Bus Comp", "VCA Bus Glue"],
  ["Fairchild Mu", "Vari-Mu Bus"],
  ["dbx 160 VCA", "VCA Punch Comp"],
  ["CL-1B Vocal", "Tube Opto Vocal"],
  ["Neve Diode Bus", "Diode Bridge Bus"],
  ["Distressor Punch", "Punch Comp"],
  ["Space Echo RE-201", "Tape Echo Heads"],
  ["Memory Man BBD", "Analog Bucket Echo"],
  ["Echoplex EP-3", "Tape Slap Echo"],
  ["TC 2290 Grid", "Digital Grid Delay"],
  ["EMT 140 Plate", "Foil Plate"],
  ["Lexicon 480 Hall", "Large Hall"],
  ["AMS RMX Nonlin", "Nonlinear Snap"],
  ["Blues Driver", "Blues Break OD"],
  ["Direct Amp SVT Dark", "Foldback Bass Dark"],
  ["Rat Into Amp", "Filter Dist Amp"],
  ["Muff Into Amp", "Fuzz Into Amp"],
  ["Serial 76 Into 2A", "FET Then Opto"],
]);

const MUST_KEEP = new Set([
  "Mesa High Gain",
  "Metal Gate",
  "Stereo Guitar Wall",
  "Cyberpunk Drive",
  "Glitch Laboratory",
  "Kick Rumble",
  "Warehouse Rumble",
  "Hardcore Clip",
  "Gabber Drive",
  "Acid Hash",
  "Neon Clip",
  "Industrial Gate",
  "Hoover Dirt",
  "Tekno Comb",
  "Data Mosher",
  "Chrome Fold",
  "Sidechain Pump",
  "Classic Tremolo",
  "Chopper",
  "Low Pass Sweep",
  "Shimmer Drive",
  "Wide Motion",
  "Trailer Impact",
  "Tape Echo Dirt",
  "Far Plane",
  "Acid Line",
  "Bass Architect",
  "Doubler AM",
  "Cinematic Space",
  "Side Delay",
  "Side Hall",
  "Vocal Send",
  "NY Drum Bus",
  "Mono Below",
  "MS Mix Desk",
  "MS Imager",
  "Plate Send",
  "Width Delay",
  "Haas Width",
  "Score Hall",
  "Boom Tail",
  "Wide Canvas",
  "Tension Bed",
  "Phaser Lab",
  "Spring Tank",
  "Multiband Glue",
  "Envelope Shaper",
  "Dark Atmosphere",
  "Distant Radio Amb",
]);

const DROP = new Set([
  "Soft Overdrive", "Bitcrusher", "Tremolo Free", "Chopper Free",
  "Amp Crunch", "Pedal OD", "Cassette", "Clean Boost", "Safety Clip",
  "Soft Clip Tone", "Soft-Knee Ceiling", "Parallel Soft Clip",
  "Bypass Clean", "Chorus-like", "Compressor+", "Feedback Delay (einfach)",
  "Gain Staging", "Hard Clipper", "Phaser-like", "Vibrato",
  "High Pass Utility", "Low Pass Utility", "Gain Stage Utility",
  "Phase Flip Utility", "MS Encode Utility", "Dry Wet Utility",
  "Meter Probe Utility", "Safety Clip Utility",
  "Wow Only Tape", "Flutter Only Tape", "Worn Cassette 2",
  "Wide Canvas Soft", "Noise Blow Soft", "Comb Taste Soft",
  "Tilt Master", "Presence Master", "Dark Master", "Punch Master",
  "Mono Check Fold", "Air Verb Only", "Ping Tight", "Ping Wide Slow",
  "Air Exciter", "Crystal Edge", "Dual Amp Stack",
  "Tape Saturate", "Bus Glue", "Loudness Clip", "Lo-Fi Crush",
  "Club Clip", "Riff Saturate", "Bass Folder", "Sub Push",
  "Whisper Edge", "Vinyl Dirt", "Optical Comp", "Parallel Crush",
  "OTT Smash", "Reso Peak", "Phaser Sweep",
  "MS Width", "MS Mid Focus", "Mono Punch",
  "Softclip Open", "Bass Dirt Pedal", "Synth Clip Desk",
  "Kick Click Split", "Pluck Envelope", "White Noise Riser",
  "Chip Tune Crush", "Infinite Repeat", "Stutter Repeat",
  "Freeze-ish Wash", "Riser Designer", "Downer Designer",
  "Metal Hit Ring", "Air Whoosh SD", "Body Resonator",
  "Noise Generator", "Tube Warmth", "Mix Glue", "Clean Sparkle",
  "DI Warmth", "Stereo Shine", "Transient Shaper", "Gate + Compress",
  "Parallel Punch", "Fold Distortion", "Granular Cloud",
  "8-Bit Game", "Cassette Warm", "Crystal Pad", "Ethereal Wash",
  "Hi-Hat Tamer", "Radio Interference", "Reverse Swell FX",
  "Riser Builder", "Room Explosion", "Supersaw Grit", "Trance Gate",
  "Vinyl Crackle", "Lookahead-ish Limit", "Upward Soft Lift",
  "Downward Only Crush", "Noise Gate Tight", "Expander Room",
  "Gentle Bus Glue M", "Safety Ceiling Master", "OTT Glue Master",
  "Stereo Widen Safe", "Shelf Air Only Filt", "Low Shelf Weight",
  "DJ Low Kill", "DJ High Kill", "Notch Honk Cut", "Bandpass Radio",
  "Vibrato Only", "Envelope Phaser", "Slow Chorus Pad",
  "Barber Pole Phaser", "Fast Flange Jet", "Tremolo Square Cut",
  "Tremolo Sine Warm", "Bus Glue Light", "OTT Insert Lift",
  "OTT Send Smash", "Peak Limit Polish", "HPF Comp Kick",
  "De-Ess Wide", "Sidechain Pump Bus", "NY Parallel Smash",
  "Fast FET Catch", "Slow Opto Ride", "All Buttons Smash",
  "Vocal Opto Soft", "Vari-Mu Bus Soft", "Drum FET Fast",
  "Parallel Dirt Blend", "Transparent Diode", "Hard Clip Pedal",
  "Filter Dist Pedal", "Pedalboard Crunch", "Chime Top End",
  "High Gain American", "Chime OD Stack",
  "Straight Eighth Pop", "Dark Bucket Brigade",
  "Small Booth", "Concert Seat Near", "Abyss Low Verb",
  "Mono Room Center", "Haas Pre Delay", "Looper Pad Delay",
  "Saturation Echo", "Gate Tail Delay", "Swell Delay Rise",
  "Long Wash Delay", "Bright Digital Grid", "Multi Tap Stair",
  "Half Note Wash", "Sixteenth Sprinkle", "Triplet Delay",
  "Dotted Quarter Dub", "Up Octave Chime", "Both Octaves Stack",
  "Octave Then Gate", "Harmony Bed Octave", "Tracking Octave Tight",
  "Octave Shimmer Send", "Vocoder Formant Shift", "Vocoder Talk Box",
  "Grit Mid Bass", "Fuzz Bass Stack", "Bass Limit Polish",
  "Bass DI Chorus", "DI Amp Blend Bass", "Funk Wah Env",
  "Script Phaser Guitar", "Analog Chorus Guitar", "Clean Spring Send",
  "Wet Dry Split Guitar", "Slap Room Combo", "Delay Into Plate",
  "Chorus Into Delay", "Mid Only Echo", "Side Only Echo",
  "Filtered Throw", "Slap Double", "Parallel Tape",
  "Width Delay", "Mono to Stereo", "Loudness Curve", "Missing Bass",
  "Speech Band", "Glass Blend", "Porta Bass",
  "Precision Octaver", "Bass Sub Octave", "Vocoder Lite",
  "Rhythmic Gate Delay",
  "Soft Chorus Cloud", "Underwater LPF", "Wind Highpass Amb",
  "Glass Plate Amb", "Tidal Delay Amb", "Night Hall",
  "Ice Crystal Verb", "Fog Lowpass", "Slow Swell Ambient",
  "Far Plane Darker", "Wide Canvas Soft",
  "Keys Rhodes Trem", "String Lush Synth", "Bass Reese Synth",
  "Lead Presence Synth", "Pad Widen Synth", "Pluck Desk Synth",
  "Keys Piano Hall", "Organ Rotary Synth", "Arp Sparkle Synth",
  "Brass Stab Synth", "FX Swoosh Synth", "Poly Glue Synth",
  "Mono Lead Dirt", "Bell Sparkle Synth",
  "Build Filter Rise", "Club Sidechain Pad", "Club OTT Lift",
  "Tekno Metal Delay", "Warehouse Body",
  "Fold Then Recover", "Bitcrush Then Cab", "Diode Stack Pedal",
  "Power Amp Grind", "Preamp Only Stack", "Hard Ceiling Pedal",
  "Asym Tube Push", "Green Boost Only", "Octave Fuzz",
  "Silicon Fuzz Gate", "Width Without Haas", "Bass Weight Trick",
  "Ear Tickle 3k", "Center Cut Through", "Missing Air",
  "High Pass Utility", "Air And Weight", "Mid Focus Master",
  "Tilt Dark Bright", "Formant Vowel", "Comb Metallic",
  "LFO Filter Sweep", "HPF Rumble Cut", "Reso Acid Slide",
  "Leslie Slow", "Leslie Fast", "Detune Widen Mod",
  "Chopper Sixteenth", "Wow Flutter Tape", "Auto Pan Wide",
  "Ring Mod Metallic",
  "Tom Body Desk", "Overhead Glue", "Room Mic Smash",
  "Clap Plate Desk", "Kick Sidechain Kit", "Transient Kit Snap",
  "Lo-Fi Kit Crush", "Cymbal Splash Verb", "Rimshot Crack",
  "Perc Shaker Air", "Live Kit Glue", "Sample Drum Polish",
  "Brush Kit Soft", "Hat Air Desk", "Kick Click Desk",
  "808 Sub Kick", "Parallel Crush Kit", "Gated Room Snare",
  "Snare Crack Desk", "Transient Snap Dyn", "Body Sustain Dyn",
  "Diode Bridge Punch", "Mixbus Soft Clip",
  "Bright Tile Room", "Reverse Feel Bloom", "Wide Score Cloud",
  "Predelay Hall", "Spring Splash Verb", "Nonlin Drum Snap",
  "Side Hall Only", "Mid Hall Glue", "Plate Into Chorus",
  "Drum Room Close", "Vocal Plate Short", "Cathedral Tail",
  "Chamber Wood", "Shimmer Verb Send", "Snare Plate Hit",
  "Dark Hall Score", "Gated Eighties",
  "Studio Room", "Concert Hall", "Bright Plate", "Drum Chamber",
  "Eighth Note Echo", "Slap Echo", "Ping Pong Wide",
  "Analog Delay", "Quarter Dub",
  "Alien Ring", "Stutter Gate", "Formant Crush", "Noise Blow",
  "Fold Universe", "Feedback Screamer", "Glitch Gate",
  "Drone Layer", "Riser Noise", "Dubstep Growl",
  "Phone Line",
  "Pad Swell", "Lead Scream", "PWM Texture", "Supersaw Dirt",
  "Hat Sizzle", "Room Crush", "Snare Crack", "Kick Punch", "Drum Smash",
  "Bass Comp Drive", "Bass Growl", "Lead Boost", "Blues Breakup",
  "Heavy Comp", "High Pass Gate", "Auto-Wah", "Ring Modulator",
  "Wave Folder",
]);

// Cyberpunk Drive is MUST_KEEP — DROP must not win. Built below.

const VOCAL_KEEP = /vocal|voice|speech|dialogue|narrat|choir|de-ess|podcast|talk |shout|whisper|rap punch|telephone cup|radio band|breath gate|presence pocket|formant|doubler/i;

function blocks(script) {
  return (script.match(/^[a-z][a-z0-9_]*\s*:/gmi) || [])
    .filter((l) => ! /^param\s/i.test(l) && ! /^out\s*:/i.test(l)
      && ! /^bus\s/i.test(l) && ! /^send\s*:/i.test(l));
}

function scrubBrand(text) {
  return String(text ?? "")
    .replace(/\bFender\b/gi, "airy")
    .replace(/\bMarshall\b/gi, "Plexi")
    .replace(/\bVox\b/gi, "chime combo")
    .replace(/\bTube Screamer\b/gi, "mid-boost OD")
    .replace(/\bKlon(?: Centaur)?\b/gi, "transparent OD")
    .replace(/\bProCo RAT\b/gi, "filter-dist")
    .replace(/\bRAT-style\b/gi, "filter-dist")
    .replace(/\bFuzz Face\b/gi, "germanium fuzz")
    .replace(/\bUni-?Vibe\b/gi, "optical vibe")
    .replace(/\bOrange Crush\b/gi, "British mid grind")
    .replace(/\b5150\b/g, "tight lead")
    .replace(/\bJCM\b/g, "hot British")
    .replace(/\bSVT\b/g, "foldback bass")
    .replace(/\bAC30\b/g, "chime combo")
    .replace(/\bBassman\b/gi, "tweed bass")
    .replace(/\b1176(?:LN)?\b/g, "FET peak")
    .replace(/\bLA-2A\b/g, "optical leveler")
    .replace(/\bSSL(?: G-series)?\b/g, "VCA bus")
    .replace(/\bFairchild(?: 670)?\b/gi, "vari-mu")
    .replace(/\bTeletronix\b/gi, "")
    .replace(/\bUREI\b/g, "")
    .replace(/\bNeve(?: 33609)?\b/gi, "diode-bridge")
    .replace(/\bLexicon(?: 480L?)?\b/gi, "large hall")
    .replace(/\bEMT 140\b/g, "foil plate")
    .replace(/\bAMS RMX\b/g, "nonlinear")
    .replace(/\bRoland RE-201\b/gi, "tape echo")
    .replace(/\bRE-201\b/g, "tape echo")
    .replace(/\bMemory Man\b/gi, "bucket echo")
    .replace(/\bEchoplex(?: EP-3)?\b/gi, "tape slap")
    .replace(/\bTC 2290\b/g, "digital grid")
    .replace(/\bBlues Driver\b/gi, "blues-break OD")
    .replace(/\bMesa(?! High Gain)\b/gi, "American high-gain")
    .replace(/ {2,}/g, " ")
    .trim();
}

const slim = JSON.parse(fs.readFileSync(path.join(root, "web", "src", "presets", "factoryCatalog.gen.json"), "utf8"));
const disk = JSON.parse(fs.readFileSync(src, "utf8"));
const byName = new Map(disk.map((p) => [p.name, p]));
const LETTER = ["paramA", "paramB", "paramC", "paramD", "paramE", "paramF"];
const raw = slim.map((s) => {
  const full = byName.get(s.name);
  if (full) {
    return full;
  }
  const row = {
    name: s.name,
    category: s.category,
    description: s.description,
    inputGain: 0,
    outputGain: 0,
    mix: s.mix ?? 1,
    script: s.script,
    tags: s.tags ?? [],
  };
  for (const k of s.knobs ?? []) {
    const i = "abcdef".indexOf(k.id);
    if (i < 0) {
      continue;
    }
    row[LETTER[i]] = { name: k.name, min: k.min, max: k.max, default: k.default };
  }
  return row;
});
const out = [];
const dropped = [];

for (const p of raw) {
  const name = String(p.name ?? "");
  const cat = String(p.category ?? "");
  const vocal = cat === "Vocals" || VOCAL_KEEP.test(name);
  const renamed = RENAME.get(name) ?? name;
  const must = MUST_KEEP.has(name) || MUST_KEEP.has(renamed) || name === "Mesa High Gain"
    || RENAME.has(name);

  let drop = false;
  if (must || vocal) {
    drop = false;
  } else if (DROP.has(name) || cat === "Utility") {
    drop = true;
  }

  if (drop) {
    dropped.push(name);
    continue;
  }

  const next = { ...p, name: renamed };
  if (renamed !== name) {
    next.description = scrubBrand(p.description).replace(name, renamed);
    next.script = String(p.script ?? "")
      .replace(new RegExp(`^# ${name.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}`, "m"), `# ${renamed}`)
      .replace(name, renamed);
    next.script = scrubBrand(next.script);
    next.description = scrubBrand(next.description);
  } else if (name !== "Mesa High Gain") {
    next.description = scrubBrand(p.description);
    if (/\b(Fender|Marshall|Klon|1176|Lexicon|SSL|Fairchild|Neve|Tube Screamer|Vox|5150)\b/i.test(next.description)) {
      next.description = scrubBrand(next.description);
    }
  }
  out.push(next);
}

const seen = new Set();
const unique = [];
for (const p of out) {
  if (seen.has(p.name)) {
    dropped.push(`DUP:${p.name}`);
    continue;
  }
  seen.add(p.name);
  unique.push(p);
}

fs.writeFileSync(src, `${JSON.stringify(unique, null, 2)}\n`);
fs.writeFileSync(namesOut, `${unique.map((p) => p.name).join("\n")}\n`);
console.log(JSON.stringify({
  before: raw.length,
  after: unique.length,
  dropped: dropped.length,
  mesa: unique.some((p) => p.name === "Mesa High Gain"),
  vocals: unique.filter((p) => p.category === "Vocals").length,
}, null, 2));
console.log("dropped:", dropped.join(", "));
