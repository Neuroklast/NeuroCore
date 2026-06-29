import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const shapeMap = { sin: "sine", tri: "triangle", saw: "saw", square: "square", noise: "noise" };

function parseKvBody(body) {
  const flat = [...body.matchAll(/"([^"]+)"|(\S+)/g)].map((m) => m[1] ?? m[2]);
  const kv = {};
  let key = null;
  for (const item of flat) {
    if (key === null) key = item.toLowerCase();
    else {
      kv[key] = item;
      key = null;
    }
  }
  return kv;
}

function extractBlocks(script) {
  const blocks = [];
  let i = 0;
  const s = script.replace(/\r\n/g, "\n");
  while (i < s.length) {
    if (/[\s\n]/.test(s[i])) {
      i++;
      continue;
    }
    const m = s.slice(i).match(/^(\w+)\s*\{/);
    if (!m) {
      i++;
      continue;
    }
    const btype = m[1].toLowerCase();
    let start = i + m[0].length - 1;
    let depth = 0;
    let j = start;
    for (; j < s.length; j++) {
      if (s[j] === "{") depth++;
      else if (s[j] === "}") {
        depth--;
        if (depth === 0) {
          blocks.push([btype, s.slice(start + 1, j).trim()]);
          i = j + 1;
          break;
        }
      }
    }
    if (j >= s.length) break;
  }
  return blocks;
}

function convertScript(script) {
  const lines = [];
  const counters = { stage: 0, filter: 0, comp: 0, env: 0, osc: 0 };
  for (const [btype, body] of extractBlocks(script)) {
    if (btype === "param") {
      const kv = parseKvBody(body);
      lines.push(`param ${kv.alias ?? "a"} = ${kv.name ?? kv.alias ?? "a"} [${kv.min ?? "0"}, ${kv.max ?? "1"}]`);
    } else if (btype === "stage") {
      counters.stage++;
      const compact = body.replace(/\s+/g, " ").trim();
      const formula = compact.startsWith("y ") ? compact.slice(2).trim() : compact;
      lines.push(`stage${counters.stage}: y = ${formula}`);
    } else if (btype === "filter") {
      counters.filter++;
      const kv = parseKvBody(body);
      lines.push(`filter${counters.filter}: ` + Object.entries(kv).map(([k, v]) => `${k} = ${v}`).join("; "));
    } else if (btype === "osc") {
      const kv = parseKvBody(body);
      const name = kv.name ?? `osc${counters.osc + 1}`;
      counters.osc++;
      const shape = shapeMap[kv.shape?.toLowerCase()] ?? kv.shape ?? "sine";
      const parts = [`shape = ${shape}`];
      if (kv.freq) parts.push(`freq = ${kv.freq}`);
      if (kv.sync) parts.push(`sync = ${kv.sync}`);
      if (kv.depth) parts.push(`depth = ${kv.depth}`);
      lines.push(`${name}: ` + parts.join("; "));
    } else if (btype === "env") {
      const kv = parseKvBody(body);
      const name = kv.name ?? `env${counters.env + 1}`;
      counters.env++;
      const parts = [`type = ${(kv.mode ?? "rms").toLowerCase()}`];
      if (kv.attack) parts.push(`attack = ${kv.attack}`);
      if (kv.release) parts.push(`release = ${kv.release}`);
      if (kv.trigger) parts.push(`trigger = ${kv.trigger}`);
      lines.push(`${name}: ` + parts.join("; "));
    } else if (btype === "comp") {
      counters.comp++;
      const kv = parseKvBody(body);
      const parts = ["threshold", "ratio", "attack", "release"].filter((k) => kv[k]).map((k) => `${k} = ${kv[k]}`);
      lines.push(`comp${counters.comp}: ` + parts.join("; "));
    }
  }
  return lines.join("\n");
}

const file = path.join(__dirname, "..", "resources", "factory_presets.json");
const data = JSON.parse(fs.readFileSync(file, "utf8"));
for (const preset of data) preset.script = convertScript(preset.script);
fs.writeFileSync(file, JSON.stringify(data, null, 2) + "\n", "utf8");
console.log(`Converted ${data.length} presets`);