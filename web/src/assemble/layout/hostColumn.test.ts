import {expect,it} from "vitest";
import {hydrateBoard,graphToLayout} from "../boardModel";
import {parseDslSketch} from "../../presets/parseDslSketch";
import {compact} from "./runLayout";
it("reserves a compact left terminal column instead of extending it below the entire patch",async()=>{
 const g=graphToLayout(hydrateBoard(parseDslSketch("stage1: y = x\n").doc));
 const result=await compact(g.nodes,g.edges,{w:1280,h:520});
 const {IN,MIDI,SC}=result.nodes;
 expect(MIDI!.h).toBeLessThanOrEqual(96);expect(SC!.h).toBeLessThanOrEqual(96);
 expect(SC!.y+SC!.h-IN!.y).toBeLessThanOrEqual(448);
 expect(MIDI!.x).toBe(IN!.x);expect(SC!.x).toBe(IN!.x);
 for(const [id,node] of Object.entries(result.nodes)) {
  if(["IN","MIDI","SC"].includes(id))continue;
  expect(node.x).toBeGreaterThanOrEqual(IN!.x+IN!.w+64);
 }
});
