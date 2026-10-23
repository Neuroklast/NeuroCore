import {expect,it} from 'vitest';
import {entryById} from './blockCatalog';
import {hydrateBoard,portId} from './boardModel';
import {visualJacksFor} from './visualEdges';
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
it("ModMix paints a control out jack from catalog modOuts", () => {
  const jacks = visualJacksFor({
    id: "modmix1",
    type: "modmix",
    busName: "",
    args: { levela: "1", levelb: "1" },
    trailingComment: "",
  });
  expect(jacks.some((j) => j.output && j.id === "out" && j.kind === "mod")).toBe(true);
});

it("cutoff = random1 is a cable onto the cutoff jack", () => {
  const graph = hydrateBoard({
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [
      { id: "random1", type: "random", args: { min: "0", max: "1" }, busName: "", trailingComment: "" },
      { id: "filter1", type: "filter", args: { cutoff: "random1" }, busName: "", trailingComment: "" },
    ],
    edges: [],
  });
  const hit = Object.values(graph.edges).find(
    (e) => e.sourceNodeId === "random1" && e.targetPortId === portId("filter1", "cutoff", false),
  );
  expect(hit).toBeTruthy();
  expect(graph.ports[portId("random1", "out", true)]).toBeTruthy();
});

it("y = x * random1 still draws a cable onto the stage", () => {
  const graph = hydrateBoard({
    version: 1,
    leadingComments: [],
    params: [],
    nodes: [
      { id: "random1", type: "random", args: { min: "0", max: "1" }, busName: "", trailingComment: "" },
      { id: "stage1", type: "stage", args: { y: "x * random1" }, busName: "", trailingComment: "" },
    ],
    edges: [],
  });
  const hit = Object.values(graph.edges).find(
    (e) => e.sourceNodeId === "random1" && e.targetNodeId === "stage1",
  );
  expect(hit).toBeTruthy();
  expect(graph.ports[hit!.targetPortId]?.kind).toBe("mod");
});

it.each(['mseg','stepseq','adsr','samplehold','random','modmath','modmix'])('offline preset sketch keeps %s outside the audio rail',(type)=>{
 const doc=parseDslSketch(`${type}1: depth = 1\nstage1: y = x * ${type}1\n`).doc!;
 expect(doc.edges.filter(e=>e.kind==='audio').some(e=>e.from===`${type}1`||e.to===`${type}1`)).toBe(false);
 expect(doc.nodes.find(n=>n.id===`${type}1`)!.jacks?.some(j=>j.output&&j.kind==='mod')).toBe(true);
});


it("keeps every parameter dependency from one modulation source", () => {
 const graph = hydrateBoard({version:1, leadingComments:[], params:[], nodes:[
  {id:"random1",type:"random",args:{min:"0",max:"1"},busName:"",trailingComment:""},
  {id:"filter1",type:"filter",args:{cutoff:"random1",resonance:"1 + random1"},busName:"",trailingComment:""}
 ],edges:[]});
 const targets = Object.values(graph.edges).filter(e => e.sourceNodeId === "random1").map(e => e.targetPortId);
 expect(targets.sort()).toEqual([portId("filter1","cutoff",false),portId("filter1","resonance",false)].sort());
});
