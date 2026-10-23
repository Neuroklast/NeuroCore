/** Build a machine-readable factory inventory from the committed catalog. */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { classify, parseBlocks, parseMacros, topology, PROTECTED_NAMES } from "./metadata.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const catalog = JSON.parse(fs.readFileSync(path.join(root, "resources/factory_presets.json"), "utf8"));

const GUITAR_CRUNCH = ["Plexi Crunch", "British Plexi Bark", "British Crunch Stack", "British Mid Grind", "American Crunch"];
const GUITAR_CLEAN = ["Airy Clean", "American Clean Cab", "Deluxe Sparkle", "Top Boost Chime", "Chime Combo", "Edge Of Breakup"];
const GUITAR_METAL = ["Metal Gate", "Tight Metal Regen", "Metal Scoop Cab", "Metal Scoop", "Tight Lead Stack", "Scooped Rhythm Wall", "Rhythm Mid Scoop", "Tight Gate Lead", "Chug Palm Mute"];

function byName(name) {
  return catalog.find((p) => p.name === name);
}

function decision(p, meta, blocks) {
  if (PROTECTED_NAMES.has(p.name)) {
    return { action: "KEEP", reason: "Protected Mesa High Gain. Sonic object must stay byte-identical." };
  }
  const desc = String(p.description ?? "");
  if (/massive analog|powerful professional sound/i.test(desc)) {
    return { action: "IMPROVE", reason: "Marketing copy; rewrite to state the actual processing." };
  }
  if (/sidechain/i.test(p.name) && ! meta.requiredInputs.includes("sidechain")) {
    return { action: "IMPROVE", reason: "Name implies sidechain but the script has no sidechain input." };
  }
  if (p.category === "Distortion" && ! meta.soundSources.includes("Guitar") && ! meta.soundSources.includes("Any")) {
    return { action: "RECLASSIFY", reason: "Distortion category is not always guitar; metadata records actual sources." };
  }
  return { action: "KEEP", reason: "Distinct script or use case; no listening/render proof of redundancy." };
}

const entries = catalog.map((p) => {
  const blocks = parseBlocks(p.script);
  const meta = classify(p);
  const macros = parseMacros(p);
  const flow = topology(p.script);
  const dec = decision(p, meta, blocks);
  return {
    name: p.name,
    category: p.category,
    description: p.description,
    soundSources: meta.soundSources,
    processingTypes: meta.processingTypes,
    style: meta.style || null,
    role: meta.role,
    requiredInputs: meta.requiredInputs,
    blocks: blocks.counts,
    topology: flow,
    blockSignature: blocks.signature,
    macros,
    modulationSources: ["osc", "env", "mseg", "stepseq", "adsr", "random", "samplehold", "envtrigger", "modmath", "modmix"]
      .filter((t) => blocks.counts[t]),
    routing: {
      bus: Boolean(blocks.counts.bus),
      xover: Boolean(blocks.counts.xover),
      send: flow.some((l) => /^send\s*:/i.test(l)),
      join: Boolean(blocks.counts.join),
    },
    protected: PROTECTED_NAMES.has(p.name),
    template: macros && Object.keys(macros).length > 0 && flow.length <= 12,
    levelsLatencyCpu: "not measured",
    listening: "open",
    decision: dec.action,
    reason: dec.reason,
  };
});

const sigMap = new Map();
for (const e of entries) {
  const list = sigMap.get(e.blockSignature) ?? [];
  list.push(e.name);
  sigMap.set(e.blockSignature, list);
}
for (const e of entries) {
  e.similarBySignature = (sigMap.get(e.blockSignature) ?? []).filter((n) => n !== e.name);
}

function cluster(names) {
  return names.map((name) => {
    const e = entries.find((x) => x.name === name);
    const p = byName(name);
    return p && e ? {
      name,
      category: p.category,
      blocks: e.blocks,
      signature: e.blockSignature,
      macros: e.macros,
      topology: e.topology,
      decision: e.decision,
      reason: "Script topology differs or is unproven-equal; KEEP without listening evidence.",
    } : { name, missing: true };
  });
}

const bySource = {};
const byType = {};
for (const e of entries) {
  for (const s of e.soundSources) bySource[s] = (bySource[s] ?? 0) + 1;
  for (const t of e.processingTypes) byType[t] = (byType[t] ?? 0) + 1;
}

const report = {
  generated: "2026-10-23",
  catalogCount: catalog.length,
  protected: [...PROTECTED_NAMES],
  decisions: entries.reduce((acc, e) => { acc[e.decision] = (acc[e.decision] ?? 0) + 1; return acc; }, {}),
  bySoundSource: bySource,
  byProcessingType: byType,
  guitarCrunch: cluster(GUITAR_CRUNCH),
  guitarClean: cluster(GUITAR_CLEAN),
  guitarMetal: cluster(GUITAR_METAL),
  presets: entries,
};

const outJson = path.join(root, "docs/presets/2026-10-23-inventory.json");
fs.writeFileSync(outJson, JSON.stringify(report, null, 2) + "\n");

function clusterMd(title, rows) {
  const lines = [`### ${title}`, "", "| Name | Signature | Macros | Decision |", "|---|---|---|---|"];
  for (const r of rows) {
    if (r.missing) { lines.push(`| ${r.name} | missing | | |`); continue; }
    lines.push(`| ${r.name} | \`${r.signature}\` | ${Object.values(r.macros).join(", ")} | KEEP |`);
  }
  return lines.join("\n");
}

const md = [
  "# Factory inventory, 2026-10-23",
  "",
  `Active catalog: **${catalog.length}** presets (PR #328 reference was 353).`,
  "",
  "This pass classifies use cases from the committed script, not from marketing names. Latency, CPU, I/O levels and listening are **open** — no DAW session was run.",
  "",
  "## Decisions",
  "",
  Object.entries(report.decisions).map(([k, v]) => `- ${k}: ${v}`).join("\n"),
  "",
  "No preset is deprecated in this inventory. Guitar crunch/clean/metal clusters keep distinct topologies; correlation was not used as a delete rule.",
  "",
  "## Sound source counts (a preset may appear in more than one)",
  "",
  Object.entries(bySource).sort((a, b) => b[1] - a[1]).map(([k, v]) => `- ${k}: ${v}`).join("\n"),
  "",
  "## Processing type counts",
  "",
  Object.entries(byType).sort((a, b) => b[1] - a[1]).map(([k, v]) => `- ${k}: ${v}`).join("\n"),
  "",
  clusterMd("British / American crunch", report.guitarCrunch),
  "",
  clusterMd("Clean / edge-of-breakup", report.guitarClean),
  "",
  clusterMd("High-gain / metal (Mesa High Gain protected, not listed as a cull candidate)", report.guitarMetal),
  "",
  "## Gaps vs the twelve candidates",
  "",
  "- Poly Synth Motion: Control Crossfade Filter is stepseq+random; no slow MSEG+random filter template. **Add.**",
  "- Velocity Driven Drive: MIDI Pluck Filter is cutoff, not saturation. **Add.**",
  "- Hybrid Dynamic Wah: envelope+LFO mix into a filter is missing. **Add.**",
  "- Subsafe Groove Bass: split-band bass exists, but not stepseq groove on the upper band. **Add.**",
  "- Resonant Drum Body: Metallic Resonant Body is always-on; MIDI Bowed Resonator needs MIDI. **Add** audio-triggered variant.",
  "- Dynamic Hat Tamer: Metal Cymbal Tamer is static EQ+light comp, not a de-esser. **Add.**",
  "- Triggered Room Send: MIDI Swell Return needs MIDI. **Add** audio-triggered send.",
  "- Patterned Delay Throws: no stepseq-gated delay send. **Add.**",
  "- Vocal Presence Duck: Sidechain Duck Bass is full-band bass ducking. **Add** presence-band duck.",
  "- Metered Drive Template: no factory `meter` block. **Add.**",
  "- Evolving Texture Pad: overlaps Dark Atmosphere / Dynamic Space Former / Poly Synth Motion. **Skip.**",
  "- Transient Controlled Texture: overlaps Transient Accent Fold. **Skip.**",
  "",
  "Per-preset records: `2026-10-23-inventory.json`.",
  "",
].join("\n");

fs.writeFileSync(path.join(root, "docs/presets/2026-10-23-review.md"), md);
console.log(`inventory ${catalog.length} -> docs/presets/2026-10-23-inventory.json`);
