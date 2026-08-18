/**
 * Build-time bind: resources/factory_presets.json is the only committed catalog.
 * The web shell reads the generated copy; CMake embeds the same JSON into BinaryData.
 */
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const src = path.join(root, "resources", "factory_presets.json");
const dest = path.join(root, "web", "src", "presets", "factoryCatalog.gen.json");

const raw = JSON.parse(fs.readFileSync(src, "utf8"));
if (! Array.isArray(raw) || raw.length === 0) {
  throw new Error("resources/factory_presets.json is empty");
}
const PARAM_KEYS = ["paramA", "paramB", "paramC", "paramD", "paramE", "paramF"];
const slim = raw.map((p) => {
  const knobs = PARAM_KEYS.flatMap((key, i) => {
    const rec = p[key];
    if (! rec || typeof rec !== "object") {
      return [];
    }
    return [{
      id: "abcdef"[i],
      name: String(rec.name ?? "abcdef"[i].toUpperCase()),
      min: Number(rec.min ?? 0),
      max: Number(rec.max ?? 1),
      default: Number(rec.default ?? 0),
    }];
  });
  return {
    name: String(p.name ?? ""),
    category: String(p.category ?? "Factory"),
    description: String(p.description ?? ""),
    tags: Array.isArray(p.tags) ? p.tags.map(String) : [],
    script: String(p.script ?? ""),
    mix: Number(p.mix ?? 1),
    knobs,
  };
});
fs.mkdirSync(path.dirname(dest), { recursive: true });
fs.writeFileSync(dest, JSON.stringify(slim));
console.log(`embedded ${slim.length} factory presets -> ${path.relative(root, dest)}`);

const helpSrc = path.join(root, "resources", "UserManual_en.txt");
const helpDest = path.join(root, "web", "src", "overlays", "userManual.gen.txt");
fs.mkdirSync(path.dirname(helpDest), { recursive: true });
fs.copyFileSync(helpSrc, helpDest);
console.log(`embedded user manual -> ${path.relative(root, helpDest)}`);
