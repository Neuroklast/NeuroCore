import { expect, it } from "vitest";
import { hydrateBoard, graphToLayout } from "../boardModel";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { parseRoutePath } from "../boardPath";
import { compact } from "./runLayout";

it("routes host sidechain and serial audio without shared conductor segments", async () => {
 const failures: string[] = [];
 for (const length of [2,4,8,12]) {
  const script = Array.from({length},(_,i)=>`comp${i+1}: threshold = -18; source = sidechain`).join("\n");
  const graph = graphToLayout(hydrateBoard(parseDslSketch(script).doc));
  const layout = await compact(graph.nodes,graph.edges,{w:1440,h:640});
  const spans = new Map<string,{lo:number;hi:number;id:string}[]>();
  for (const [id,path] of Object.entries(layout.edgePaths)) {
   const pts = parseRoutePath(path);
   for (let i=1;i<pts.length;++i) {
    const a=pts[i-1]!, b=pts[i]!;
    if (a.x !== b.x && a.y !== b.y) continue;
    const vertical=a.x===b.x;
    const key=vertical?`x${a.x}`:`y${a.y}`;
    const lo=Math.min(vertical?a.y:a.x,vertical?b.y:b.x), hi=Math.max(vertical?a.y:a.x,vertical?b.y:b.x);
    const list=spans.get(key)??[];
    for(const other of list) if(other.id !== id && Math.min(other.hi,hi)>Math.max(other.lo,lo)) failures.push(`${length}: ${id} / ${other.id}`);
    list.push({lo,hi,id});spans.set(key,list);
   }
  }
 }
 expect(failures).toEqual([]);
},30000);

it("rejects missing routing ports instead of routing to the first contact", async () => {
 const graph = graphToLayout(hydrateBoard(parseDslSketch("stage1: y = x\n").doc));
 const edges = graph.edges.map((edge,i)=>i===0?{...edge,toJack:"missing-contact"}:edge);
 await expect(compact(graph.nodes,edges)).rejects.toThrow("missing-contact");
});
