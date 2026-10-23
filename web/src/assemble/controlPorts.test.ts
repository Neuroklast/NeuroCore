import {expect,it} from 'vitest';
import {entryById} from './blockCatalog';
import {hydrateBoard,portId} from './boardModel';
import {canLink} from './boardConnect';
it('ModMix catalog exposes control inputs and no audio inputs',()=>{
 const entry=entryById('modmix')!;
 expect(entry.audioIns).toEqual([]);
 expect(entry.modIns).toEqual(['inA','inB']);
});
it.each(['mseg','random','stepseq','adsr','samplehold','modmath','modmix'])('Circuit routes %s control to both A/B contacts and rejects audio',(type)=>{
 const graph=hydrateBoard({version:1,leadingComments:[],params:[],nodes:[
  {id:'source1',type,args:{min:'0',max:'1'},busName:'',trailingComment:''},
  {id:'modmix1',type:'modmix',args:{levela:'1',levelb:'1'},busName:'',trailingComment:''}
 ],edges:[]});
 const source=graph.ports[portId('source1',type === 'modmath' ? 'mod' : 'out',true)]!;
 for(const input of ['inA','inB']){
  const target=graph.ports[portId('modmix1',input,false)]!;
  expect(target?.kind).toBe('mod');
  expect(canLink(source,target)).toBe(true);
  expect(canLink(graph.ports[portId('IN','out',true)]!,target)).toBe(false);
 }
});

import {parseDslSketch} from '../presets/parseDslSketch';
it.each(['mseg','stepseq','adsr','samplehold','random','modmath','modmix'])('offline preset sketch keeps %s outside the audio rail',(type)=>{
 const doc=parseDslSketch(`${type}1: depth = 1\nstage1: y = x * ${type}1\n`).doc!;
 expect(doc.edges.filter(e=>e.kind==='audio').some(e=>e.from===`${type}1`||e.to===`${type}1`)).toBe(false);
 expect(doc.nodes.find(n=>n.id===`${type}1`)!.jacks?.some(j=>j.output&&j.kind==='mod')).toBe(true);
});
