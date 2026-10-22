import fs from "node:fs";
import { curateFactory, retired, revisions, dcCleanupNames } from "./factory/curation-2026-10-22.mjs";
const file="resources/factory_presets.json";
const original=JSON.parse(fs.readFileSync(file,"utf8"));
const curated=curateFactory(original);
const targetByName=new Map(curated.map(p=>[p.name,p]));
fs.mkdirSync("resources/deprecated",{recursive:true});
const archiveFile="resources/deprecated/factory-2026-10-22.json";
const archived=fs.existsSync(archiveFile)?JSON.parse(fs.readFileSync(archiveFile,"utf8")):[];
const saved=new Set(archived.map(p=>p.name));
for(const p of original) {
  const next=targetByName.get(p.name);
  if((!next || JSON.stringify(next)!==JSON.stringify(p))&&!saved.has(p.name)) {
    archived.push(p);saved.add(p.name);
  }
}
const decisions=archived.map(p=>({
  name:p.name,action:retired[p.name]?"deprecated":"revised; previous version archived",
  replacement:retired[p.name]?.[0]??p.name,
  reason:retired[p.name]?.[1]??revisions.get(p.name)?.reason??(dcCleanupNames.includes(p.name)?"Remove measured post-shaper DC with a 20 Hz highpass.":"Broaden the retained compressor's useful ratio range.")
}));
fs.writeFileSync(archiveFile,JSON.stringify(archived,null,2)+"\n");
fs.writeFileSync("resources/deprecated/factory-2026-10-22-manifest.json",JSON.stringify(decisions,null,2)+"\n");
fs.writeFileSync(file,JSON.stringify(curated,null,2)+"\n");
console.log(`Factory: ${original.length} -> ${curated.length}; ${archived.length} complete old entries archived.`);
