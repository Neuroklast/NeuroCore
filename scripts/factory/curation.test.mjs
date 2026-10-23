import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
const presets=JSON.parse(fs.readFileSync('resources/factory_presets.json','utf8'));
test('factory includes distinct demonstrations of all shipping control sources',()=>{
 for(const type of ['mseg','stepseq','adsr','samplehold','random','envtrigger','modmath','modmix'])
  assert.ok(presets.some(p=>new RegExp('^'+type+'\\d+:','m').test(p.script)), `missing ${type}`);
});
test('the old additive-LFO wow and DC doubler are replaced by actual time/pitch processing',()=>{
 for(const name of ['Tape Wobble','Vocal Double']) {
  const p=presets.find(p=>p.name===name);assert.ok(p);
  assert.match(p.script,/delay\d+:|pitch\d+:/);
  assert.doesNotMatch(p.script,/stage\d+:.*\+\s*(?:osc\d+|a\s*\*\s*0\.05)/);
 }
});

import {createHash} from 'node:crypto';
import {curateFactory,retired} from './curation-2026-10-22.mjs';
const inventory=JSON.parse(fs.readFileSync('docs/presets/2026-10-22-inventory.json','utf8'));
const archived=JSON.parse(fs.readFileSync('resources/deprecated/factory-2026-10-22.json','utf8'));
const canonical=v=>Array.isArray(v)?'['+v.map(canonical).join(', ')+']':
 v&&typeof v==='object'?'{'+Object.keys(v).sort().map(k=>JSON.stringify(k)+': '+canonical(v[k])).join(', ')+'}':JSON.stringify(v);
const hash=v=>createHash('sha256').update(canonical(v)).digest('hex');
test('Mesa High Gain stays exactly at its protected baseline',()=>{
 const original=inventory.presets.find(p=>p.name==='Mesa High Gain');
 assert.equal(hash(presets.find(p=>p.name===original.name)),original.originalSha256);
});
test('every removed preset has an exact complete original in deprecated',()=>{
 for(const [name] of Object.entries(retired)){
  assert.ok(!presets.some(p=>p.name===name),name);
  const copy=archived.find(p=>p.name===name);
  assert.ok(copy, name);
  assert.equal(hash(copy),inventory.presets.find(p=>p.name===name).originalSha256,name);
 }
});
test('curation is idempotent and retains every unretired name',()=>{
 assert.deepEqual(curateFactory(presets),presets);
 for(const p of inventory.presets)assert.ok(presets.some(x=>x.name===p.name)||archived.some(x=>x.name===p.name),p.name);
});
