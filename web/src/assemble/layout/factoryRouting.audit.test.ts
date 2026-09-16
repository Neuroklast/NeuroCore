import { expect, it } from "vitest";
import { factoryRows } from "../../presets/factoryCatalog";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { hydrateBoard, graphToLayout } from "../boardModel";
import { parseRoutePath } from "../boardPath";
import { arrange, compact } from "./runLayout";
it("routes the factory graphs", async () => {
 const failures: string[] = [];
 for (const p of factoryRows()) for (const mode of ["arrange", "compact"]) {
   const { doc } = parseDslSketch(p.script);
   const graph = graphToLayout(hydrateBoard(doc));
   try {
     const result = await (mode === "arrange" ? arrange : compact)(graph.nodes, graph.edges, {w:1440,h:640});
     const lines = new Map<string, {lo:number;hi:number;edge:string}[]>();
     for (const [edge, d] of Object.entries(result.edgePaths)) {
       const pts = parseRoutePath(d);
       for (let i=1;i<pts.length;i++) {
         const a=pts[i-1]!,b=pts[i]!;
         const dx=b.x-a.x,dy=b.y-a.y;
         const vertical=Math.abs(dx)<0.01;
         const slope=vertical?0:dy/dx;
         const key=vertical?`v:${a.x}`:`${slope}:${a.y-slope*a.x}`;
         const lo=Math.min(vertical?a.y:a.x,vertical?b.y:b.x);
         const hi=Math.max(vertical?a.y:a.x,vertical?b.y:b.x);
         const spans=lines.get(key)??[];
         const clash=spans.find(s=>s.edge!==edge&&Math.min(s.hi,hi)-Math.max(s.lo,lo)>0.1);
         if(clash) throw new Error(`overlap ${edge} / ${clash.edge}`);
         spans.push({lo,hi,edge}); lines.set(key,spans);
       }
     }
   }
   catch (error) { failures.push(`${mode} ${p.name}: ${error}`); }
 }
 expect(failures).toEqual([]);
}, 120000);
