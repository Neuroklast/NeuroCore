import {writeFileSync} from "node:fs";
import {expect,it} from "vitest";
import {hydrateBoard,graphToLayout,applyPlaced,portGlobal} from "../boardModel";
import {parseDslSketch} from "../../presets/parseDslSketch";
import {parseRoutePath} from "../boardPath";
import {compact,reroute} from "./runLayout";

it("keeps every route and contact valid at 10,25,50,100 processing chips",async()=>{
 const metrics=[];
 for(const count of [10,25,50,100]) {
  const script=["stepseq1: steps = 64; rate = 2","random1: rate = 4","modmix1: inA = stepseq1; inB = random1; mix = 0.5",
   "filter1: cutoff = 1000 + modmix1 * 500; resonance = 0.7",
   ...Array.from({length:count-5},(_,i)=>`stage${i+1}: y = x * (0.8 + midi_gate * 0.2)`),
   "comp1: threshold = -18; source = sidechain"].join("\n");
  const t0=performance.now();let graph=hydrateBoard(parseDslSketch(script).doc);
  const hydrateMs=performance.now()-t0;
  expect(Object.values(graph.nodes).filter(n=>n.role==="chip")).toHaveLength(count);
  expect(graph.connectionIssues).toEqual([]);
  const payload=graphToLayout(graph),t1=performance.now();
  const first=await compact(payload.nodes,payload.edges,{w:1440,h:640});
  const compactMs=performance.now()-t1;
  graph=applyPlaced(graph,first.nodes,Object.fromEntries(Object.entries(first.edgePaths).map(([id,path])=>[id,parseRoutePath(path)])));
  const next=graphToLayout(graph),t2=performance.now();
  const second=await reroute(next.nodes,next.edges);
  const rerouteMs=performance.now()-t2;
  expect(Object.keys(second.edgePaths).sort()).toEqual(Object.keys(graph.edges).sort());
  for(const edge of Object.values(graph.edges)) {
   const route=parseRoutePath(second.edgePaths[edge.id]!);
   expect(route[0],edge.id).toEqual(portGlobal(graph.nodes[edge.sourceNodeId]!,graph.ports[edge.sourcePortId]!));
   expect(route[route.length-1],edge.id).toEqual(portGlobal(graph.nodes[edge.targetNodeId]!,graph.ports[edge.targetPortId]!));
  }
  const nodes=Object.entries(second.nodes);
  const overlaps=nodes.flatMap(([id,a],index)=>nodes.slice(index+1).filter(([,b])=>a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y).map(([other])=>`${id}/${other}`));
  expect(overlaps).toEqual([]);
  metrics.push({chips:count,edges:Object.keys(graph.edges).length,hydrateMs,compactMs,rerouteMs,graphBytes:JSON.stringify(graph).length});
 }
 if(process.env.NK_LAYOUT_METRICS)writeFileSync(process.env.NK_LAYOUT_METRICS,JSON.stringify(metrics,null,2));
},120000);
