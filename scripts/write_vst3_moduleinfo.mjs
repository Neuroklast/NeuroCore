#!/usr/bin/env node
/**
 * Steinberg moduleinfo.json for the VST3 bundle (Contents/Resources).
 * CID strings are Steinberg UID::toString (defaultComFormat): the four
 * big-endian words of convertJucePluginId (manufacturer NRKL, plugin NRKO).
 * That is the form JUCE hosts parse from moduleinfo.json. Do not emit the
 * raw Windows TUID hex (01EFCDAB…) — pluginval then hashes a different uniqueId.
 * Do not load the plugin to generate this — Cubase scan hang is why
 * VST3_AUTO_MANIFEST stays false.
 */
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve (dirname (fileURLToPath (import.meta.url)), "..");

function cmakeVersionLabel() {
  const cmake = readFileSync (join (repoRoot, "CMakeLists.txt"), "utf8");
  const match = cmake.match (/set\(NEUROKORE_VERSION_LABEL "([^"]+)"\)/);
  if (! match) {
    throw new Error ("NEUROKORE_VERSION_LABEL missing from CMakeLists.txt");
  }
  return match[1];
}

const MFG = 0x4e524b4c; // NRKL
const PLUG = 0x4e524b4f; // NRKO

function wordHex (word) {
  return word.toString (16).padStart (8, "0").toUpperCase();
}

function cid (type) {
  const word0 = type === "processor" ? 0x0101abab : 0xabcdef01;
  const word1 = {
    ara: 0xa1b2c3d4,
    controller: 0x1234abcd,
    compatibility: 0xc0def00d,
    component: 0x9182faeb,
    processor: 0xabcdef01,
  }[type];
  return wordHex (word0) + wordHex (word1) + wordHex (MFG) + wordHex (PLUG);
}

function classEntry (cidHex, category, version, subCategories) {
  return `    {
      "CID": "${cidHex}",
      "Category": "${category}",
      "Name": "NEUROKORE",
      "Vendor": "Neuroklast",
      "Version": "${version}",
      "SDKVersion": "VST 3.7.12",
      "Sub Categories": ${subCategories},
      "Class Flags": 0,
      "Cardinality": 2147483647,
      "Snapshots": []
    }`;
}

function moduleInfo (version) {
  const component = cid ("component");
  const controller = cid ("controller");
  const compatibility = cid ("compatibility");
  const fx = '["Fx", "Distortion"]';
  return `{
  "Name": "NEUROKORE",
  "Version": "${version}",
  "Factory Info": {
    "Vendor": "Neuroklast",
    "URL": "https://neuroklast.net",
    "E-Mail": "mailto:info@neuroklast.net",
    "Flags": {
      "Unicode": true,
      "Classes Discardable": false,
      "Component Non Discardable": false
    }
  },
  "Classes": [
${classEntry (component, "Audio Module Class", version, fx)},
${classEntry (controller, "Component Controller Class", version, fx)},
${classEntry (compatibility, "Plugin Compatibility Class", version, "[]")}
  ]
}
`;
}

function assertSteinbergCids (json) {
  const parsed = JSON.parse (json);
  const got = parsed.Classes.map ((entry) => entry.CID);
  const required = [cid ("component"), cid ("controller"), cid ("compatibility")];
  for (const want of required) {
    if (! got.includes (want)) {
      throw new Error (`moduleinfo CID missing Steinberg UID::toString ${want}; got ${got.join (",")}`);
    }
  }
  for (const hex of got) {
    if (String (hex).startsWith ("01EFCDAB")) {
      throw new Error (`moduleinfo CID is Windows TUID hex ${hex}; use UID::toString`);
    }
  }
}

function writeJson (path, json) {
  assertSteinbergCids (json);
  mkdirSync (dirname (resolve (path)), { recursive: true });
  writeFileSync (path, json);
  console.log ("wrote", path);
}

function stripVst3Junk (bundle, version) {
  const root = resolve (bundle);
  for (const p of [
    join (root, "resources"),
    join (root, "web"),
    join (root, "Contents", "x86_64-win", "resources"),
    join (root, "Contents", "x86_64-win", "web"),
    join (root, "Contents", "moduleinfo.json"),
  ]) {
    if (existsSync (p)) {
      rmSync (p, { recursive: true, force: true });
      console.log ("removed", p);
    }
  }
  const win = join (root, "Contents", "x86_64-win");
  if (existsSync (win)) {
    for (const name of readdirSync (win)) {
      if (! name.toLowerCase().endsWith (".vst3")) {
        continue;
      }
      if (name.toLowerCase() === "neurokore.vst3") {
        continue;
      }
      const p = join (win, name);
      rmSync (p, { force: true });
      console.log ("removed", p);
    }
  }
  const res = join (root, "Contents", "Resources");
  if (! existsSync (res)) {
    return;
  }
  for (const name of readdirSync (res)) {
    if (name.toLowerCase() === "moduleinfo.json") {
      continue;
    }
    const p = join (res, name);
    rmSync (p, { recursive: true, force: true });
    console.log ("removed", p);
  }
}

const args = process.argv.slice (2);
let version = cmakeVersionLabel();
let out = "";
const also = [];
let bundle = "";
for (let i = 0; i < args.length; i += 1) {
  if (args[i] === "--endian" || args[i] === "--windows") {
    console.error ("CID encoding is Steinberg UID::toString, not host TUID byte order. Do not pass --endian.");
    process.exit (1);
  } else if (args[i] === "--version" && args[i + 1]) {
    version = args[i + 1];
    i += 1;
  } else if (args[i] === "--out" && args[i + 1]) {
    out = args[i + 1];
    i += 1;
  } else if (args[i] === "--also" && args[i + 1]) {
    also.push (args[i + 1]);
    i += 1;
  } else if (args[i] === "--bundle" && args[i + 1]) {
    bundle = args[i + 1];
    i += 1;
  }
}
if (! out) {
  console.error ("usage: write_vst3_moduleinfo.mjs --out <path> [--also <path>] [--bundle <vst3>] [--version x]");
  process.exit (1);
}
const json = moduleInfo (version);
writeJson (out, json);
for (const extra of also) {
  writeJson (extra, json);
}
if (bundle) {
  stripVst3Junk (bundle, version);
}
