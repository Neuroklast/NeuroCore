#!/usr/bin/env node
/**
 * Steinberg moduleinfo.json for the VST3 bundle (Contents/Resources).
 * CID bytes match juce::VST3ClientExtensions::convertJucePluginId
 * (manufacturer NRKL, plugin NRKO). Do not load the plugin to generate this —
 * Cubase scan hang is why VST3_AUTO_MANIFEST stays false.
 */
import { existsSync, mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const MFG = 0x4e524b4c; // NRKL
const PLUG = 0x4e524b4f; // NRKO

function byte (word, index) {
  return (word >>> (index * 8)) & 0xff;
}

function cid (type, windows) {
  const word0 = type === "processor" ? 0x0101abab : 0xabcdef01;
  const word1 = {
    ara: 0xa1b2c3d4,
    controller: 0x1234abcd,
    compatibility: 0xc0def00d,
    component: 0x9182faeb,
    processor: 0xabcdef01,
  }[type];
  const b0 = windows
    ? [0, 1, 2, 3].map ((i) => byte (word0, i))
    : [3, 2, 1, 0].map ((i) => byte (word0, i));
  const b1 = windows
    ? [2, 3, 0, 1].map ((i) => byte (word1, i))
    : [3, 2, 1, 0].map ((i) => byte (word1, i));
  const mfg = [3, 2, 1, 0].map ((i) => byte (MFG, i));
  const plug = [3, 2, 1, 0].map ((i) => byte (PLUG, i));
  return [...b0, ...b1, ...mfg, ...plug]
    .map ((b) => b.toString (16).padStart (2, "0").toUpperCase())
    .join ("");
}

function moduleInfo (version, windows) {
  const component = cid ("component", windows);
  const controller = cid ("controller", windows);
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
    {
      "CID": "${component}",
      "Category": "Audio Module Class",
      "Name": "NEUROKORE",
      "Vendor": "Neuroklast",
      "Version": "${version}",
      "SDKVersion": "VST 3.7.12",
      "Sub Categories": ["Fx", "Distortion"],
      "Class Flags": 0,
      "Cardinality": 2147483647,
      "Snapshots": []
    },
    {
      "CID": "${controller}",
      "Category": "Component Controller Class",
      "Name": "NEUROKORE",
      "Vendor": "Neuroklast",
      "Version": "${version}",
      "SDKVersion": "VST 3.7.12",
      "Sub Categories": ["Fx", "Distortion"],
      "Class Flags": 0,
      "Cardinality": 2147483647,
      "Snapshots": []
    }
  ]
}
`;
}

function writeJson (path, json) {
  mkdirSync (dirname (resolve (path)), { recursive: true });
  writeFileSync (path, json);
  console.log ("wrote", path);
}

function stripVst3Junk (bundle) {
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
      if (name.toLowerCase().includes ("0.6.4-beta")) {
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
let endian = process.platform === "win32" ? "windows" : "apple";
let version = "0.6.4-beta";
let out = "";
const also = [];
let bundle = "";
for (let i = 0; i < args.length; i += 1) {
  if (args[i] === "--endian" && args[i + 1]) {
    endian = args[i + 1];
    i += 1;
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
  } else if (args[i] === "--windows") {
    endian = "windows";
  }
}
if (! out) {
  console.error ("usage: write_vst3_moduleinfo.mjs --out <path> [--also <path>] [--bundle <vst3>] [--endian windows|apple] [--version x]");
  process.exit (1);
}
const json = moduleInfo (version, endian === "windows");
writeJson (out, json);
for (const extra of also) {
  writeJson (extra, json);
}
if (bundle) {
  stripVst3Junk (bundle);
}
