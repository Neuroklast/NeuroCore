import {expect,it} from "vitest";
import {hydrateBoard,portId,portLocal,portGlobal,graphOpEndpoints} from "./boardModel";
import {chipSpec} from "./chipSpec";
import type {AstNode} from "../bridge/ast";

it("keeps all connected compressor parameters compact across north and south banks",()=>{
 const keys=Object.keys(chipSpec("comp").ranges);
 const source:AstNode={id:"random1",type:"random",args:{},busName:"mod",trailingComment:""};
 const target:AstNode={id:"comp1",type:"comp",args:Object.fromEntries(keys.map(key=>[key,"random1"])),busName:"main",trailingComment:""};
 const graph=hydrateBoard({version:1,params:[],leadingComments:[],nodes:[source,target],edges:[]});
 const chip=graph.nodes.comp1!;
 expect(chip.w).toBeLessThanOrEqual(448);
 const ports=keys.map(key=>graph.ports[portId("comp1",key,false)]!);
 expect(ports.every(Boolean)).toBe(true);
 expect(ports.some(port=>port.face==="n")).toBe(true);
 expect(ports.some(port=>port.face==="s")).toBe(true);
 const points=ports.map(port=>portLocal(chip,port));
 expect(new Set(points.map(p=>`${p.x},${p.y}`)).size).toBe(points.length);
 expect(ports.every(port=>graphOpEndpoints(port).jackId===port.jackId)).toBe(true);
 expect(graph.connectionIssues).toEqual([]);
});

it("does not snap routed north/south contacts away from their painted local centre",()=>{
 const g=hydrateBoard({version:1,params:[],leadingComments:[],nodes:[{id:"comp1",type:"comp",args:{},busName:"main",trailingComment:""}],edges:[]});
 const node=g.nodes.comp1!;
 for(const port of Object.values(g.ports).filter(p=>p.nodeId===node.id)) {
  const local=portLocal(node,port);
  const point=portGlobal(node,port);
  expect(point).toEqual({x:node.x+local.x,y:node.y+local.y});
 }
});
