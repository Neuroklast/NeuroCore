/** Tag inference for factory presets. Vocoder ≠ sidechain. */

export function inferTags(script, name, description, category, extra = []) {
  const blob = `${script}\n${name}\n${description}\n${category}`.toLowerCase();
  const tags = new Set((extra || []).map((t) => String(t).toLowerCase().trim()).filter(Boolean));
  if (category) {
    tags.add(String(category).toLowerCase());
  }
  const addIf = (re, ...ts) => {
    if (re.test(blob)) {
      ts.forEach((t) => tags.add(t));
    }
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
  addIf(/vocoder\d*\s*:/, "vocoder", "sidechain");
  addIf(/sidechain|source\s*=\s*sidechain/, "sidechain");
  addIf(/octav|subharmonic/, "octaver", "pitch");
  addIf(/octaver\d*\s*:/, "octaver", "pitch");
  addIf(/pingpong/, "pingpong", "stereo");
  addIf(/osc\d*\s*:/, "modulation", "lfo");
  addIf(/env\d*\s*:/, "envelope");
  const word = (w) => new RegExp(`(^|[^a-z0-9])${w}([^a-z0-9]|$)`, "i").test(blob);
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
    if (word(w)) {
      tags.add(w);
    }
  }
  return [...tags].sort();
}
