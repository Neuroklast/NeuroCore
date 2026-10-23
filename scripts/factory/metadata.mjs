/** Explicit factory metadata. Tag inference may add keywords; it does not own these fields. */

export const SOUND_SOURCES = [
  "Any", "Guitar", "Bass Guitar", "Synth", "Synth Bass", "Drums", "Percussion",
  "Vocals", "Speech", "Acoustic Instruments", "Full Mix", "FX / Field Recording",
];
export const PROCESSING_TYPES = [
  "Distortion", "Saturation", "Dynamics", "Transient", "EQ", "Filter", "Modulation",
  "Pitch", "Delay", "Reverb", "Stereo", "Spatial", "Multi-Band", "Glitch", "Utility", "Restoration",
];
export const STYLES = [
  "General", "Rock", "Metal", "Industrial", "Techno", "Hard Dance", "Drum & Bass",
  "Electronic", "Ambient", "Cinematic", "Lo-Fi",
];
export const ROLES = ["Insert", "Send / Return", "Bus", "Master", "Parallel", "Creative", "Template"];
export const REQUIRED_INPUTS = ["midi", "sidechain"];
export const META_KEYS = ["soundSources", "processingTypes", "style", "role", "requiredInputs"];
export const PROTECTED_NAMES = new Set(["Mesa High Gain"]);

export function parseBlocks(script) {
  const counts = {};
  const order = [];
  for (const line of String(script).split(/\r?\n/)) {
    const m = /^([a-z]+)\d+\s*:/i.exec(line.trim());
    if (! m) continue;
    const type = m[1].toLowerCase();
    counts[type] = (counts[type] ?? 0) + 1;
    order.push(type);
  }
  return { counts, order, signature: Object.keys(counts).sort().map((k) => `${k}:${counts[k]}`).join(",") };
}

export function parseMacros(preset) {
  const macros = {};
  for (const letter of "ABCDEF") {
    const rec = preset[`param${letter}`];
    if (rec && typeof rec === "object") macros[letter] = String(rec.name ?? letter);
  }
  return macros;
}

function blob(p) {
  return `${p.name}\n${p.category}\n${p.description}\n${p.script}`.toLowerCase();
}
function word(text, w) {
  return new RegExp(`(^|[^a-z0-9])${w}([^a-z0-9]|$)`, "i").test(text);
}

const CAT_SOURCE = {
  Distortion: ["Guitar"], Guitar: ["Guitar"], Bass: ["Bass Guitar"], Vocals: ["Vocals"],
  Drums: ["Drums"], Club: ["Drums"], Synth: ["Synth"], Metal: ["Guitar"],
  Mastering: ["Full Mix"], Mixing: ["Full Mix"], Master: ["Full Mix"],
  Speech: ["Speech"], Dialogue: ["Speech"],
};

export function requiredInputsOf(script) {
  const s = String(script).toLowerCase();
  const out = [];
  if (/\bgate\s*=\s*midi\b|\bsource\s*=\s*midi\b|\bsource\s*=\s*either\b/.test(s)) out.push("midi");
  if (/\bsource\s*=\s*sidechain\b|sidechain\d+\s*:/.test(s)) out.push("sidechain");
  return out;
}

function sourcesOf(p, text, blocks) {
  const found = new Set();
  const cat = String(p.category ?? "");
  for (const s of CAT_SOURCE[cat] ?? []) found.add(s);
  if (word(text, "guitar") || word(text, "plexi") || word(text, "mesa") || word(text, "jcm")
    || word(text, "cab") || word(text, "fuzz") || word(text, "overdrive") || word(text, "amp")) {
    found.add("Guitar");
  }
  if (word(text, "bass") && ! word(text, "guitar")) found.add("Bass Guitar");
  if (word(text, "reese") || word(text, "808") || /synth bass|sub bass/.test(text)) found.add("Synth Bass");
  if (word(text, "synth") || word(text, "pad") || word(text, "arp") || word(text, "lead")) found.add("Synth");
  if (word(text, "kick") || word(text, "snare") || word(text, "hat") || word(text, "cymbal")
    || word(text, "drum") || word(text, "909")) found.add("Drums");
  if (word(text, "perc") || word(text, "tom") || word(text, "clap")) found.add("Percussion");
  if (word(text, "vocal") || word(text, "voice") || word(text, "singer") || word(text, "choir")) found.add("Vocals");
  if (word(text, "speech") || word(text, "podcast") || word(text, "dialogue") || word(text, "radio")
    || word(text, "telephone") || word(text, "phone")) found.add("Speech");
  if (word(text, "acoustic") || word(text, "piezo") || word(text, "nylon")) found.add("Acoustic Instruments");
  if (word(text, "mixbus") || word(text, "master") || word(text, "streaming") || word(text, "full mix")) {
    found.add("Full Mix");
  }
  if (word(text, "atmosphere") || word(text, "drone") || word(text, "texture") || word(text, "field")) {
    found.add("FX / Field Recording");
  }
  if (blocks.counts.ir && (cat === "Distortion" || cat === "Guitar" || cat === "Metal")) found.add("Guitar");
  if (found.size === 0) found.add("Any");
  if (found.has("Guitar") && found.size > 1 && cat === "Distortion") {
    /* guitar amps stay guitar-primary; extra sources remain */
  }
  return [...found].filter((s) => SOUND_SOURCES.includes(s));
}

function typesOf(p, text, blocks) {
  const found = [];
  const add = (t) => { if (PROCESSING_TYPES.includes(t) && ! found.includes(t)) found.push(t); };
  const c = blocks.counts;
  const cat = String(p.category ?? "");
  if (c.ir || cat === "Distortion" || cat === "Guitar" || cat === "Metal") add("Distortion");
  if (c.saturate || /\btube\s*\(|\btanh\s*\(|\bdiode\s*\(/.test(text)) add("Saturation");
  if (c.comp || c.limit || c.gate || c.ott || c.noisegate) add("Dynamics");
  if (c.deesser) add("Restoration");
  if (c.transient) add("Transient");
  if (c.eq) add("EQ");
  if (c.filter || c.nlfilter) add("Filter");
  if (c.osc || c.phaser || c.flanger || c.chorus || c.mseg || c.stepseq || c.adsr
    || c.modmix || c.modmath || c.random || c.samplehold || c.envtrigger || c.env) add("Modulation");
  if (c.pitch || c.octaver || c.freqshift) add("Pitch");
  if (c.delay || c.reversedelay) add("Delay");
  if (c.reverb) add("Reverb");
  if (c.widen || c.ms || /\bbingpong\b/.test(text)) add("Stereo");
  if (c.reverb && (c.widen || c.chorus)) add("Spatial");
  if (c.xover) add("Multi-Band");
  if (c.bitcrush || c.decimate || c.tapestop || c.shaper || c.wavefold || c.ringmod) add("Glitch");
  if (c.amp || c.meter || c.utility || cat === "Utility") add("Utility");
  if (found.length === 0) {
    if (cat === "Delay") add("Delay");
    else if (cat === "Reverb") add("Reverb");
    else if (cat === "Modulation") add("Modulation");
    else if (cat === "EQ") add("EQ");
    else add("Utility");
  }
  return found;
}

function styleOf(p, text) {
  const cat = String(p.category ?? "");
  if (cat === "Metal" || word(text, "metal") || word(text, "chug")) return "Metal";
  if (cat === "Industrial" || word(text, "industrial")) return "Industrial";
  if (word(text, "gabber") || word(text, "hardcore") || word(text, "hardstyle") || word(text, "schranz")) return "Hard Dance";
  if (cat === "Club" || word(text, "techno") || word(text, "909")) return "Techno";
  if (word(text, "dnb") || word(text, "drum & bass") || word(text, "neurofunk") || word(text, "reese")) return "Drum & Bass";
  if (cat === "Lo-Fi" || word(text, "lo-fi") || word(text, "lofi") || word(text, "bitcrush")) return "Lo-Fi";
  if (word(text, "ambient") || word(text, "drone") || word(text, "atmosphere")) return "Ambient";
  if (word(text, "cinematic") || word(text, "trailer") || word(text, "score")) return "Cinematic";
  if (word(text, "rock") || word(text, "plexi") || word(text, "crunch")) return "Rock";
  if (word(text, "edm") || word(text, "synth") || cat === "Synth") return "Electronic";
  return "";
}

function roleOf(p, text, blocks) {
  const cat = String(p.category ?? "");
  if (/\bsend\s*:/.test(text) || word(text, "send")) return "Send / Return";
  if (cat === "Mastering" || word(text, "master")) return "Master";
  if (word(text, "template") || word(text, "gain staging")) return "Template";
  if (blocks.counts.bus || /\bout\s*:/.test(String(p.script))) return "Parallel";
  if (cat === "Creative" || cat === "Sound Design") return "Creative";
  if (word(text, "bus") || cat === "Mixing") return "Bus";
  return "Insert";
}

export function classify(preset) {
  const text = blob(preset);
  const blocks = parseBlocks(preset.script);
  const soundSources = sourcesOf(preset, text, blocks);
  const processingTypes = typesOf(preset, text, blocks);
  return {
    soundSources,
    processingTypes,
    style: styleOf(preset, text),
    role: roleOf(preset, text, blocks),
    requiredInputs: requiredInputsOf(preset.script),
  };
}

export function pickMeta(preset) {
  const out = {};
  for (const k of META_KEYS) {
    if (preset[k] !== undefined) out[k] = preset[k];
  }
  return out;
}

export function applyMetadata(preset) {
  if (PROTECTED_NAMES.has(preset.name)) return preset;
  return { ...preset, ...classify(preset) };
}

export function topology(script) {
  const lines = String(script).split(/\r?\n/).map((l) => l.trim()).filter((l) => l && ! l.startsWith("#") && ! l.startsWith("param "));
  return lines;
}
