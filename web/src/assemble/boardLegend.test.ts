import { expect, it } from "vitest";
import { boardLegend } from "./boardLegend";
import type { BoardNode } from "./boardModel";
const nodes = [
  {id:"IN",type:"in",role:"io",x:0,y:0,w:160,h:160},
  {id:"filter1",type:"filter",role:"chip",x:256,y:0,w:384,h:256},
  {id:"stage1",type:"stage",role:"chip",x:768,y:0,w:384,h:256},
] as BoardNode[];
it("places real patch identification below every chip and routed cable", () => {
  const legend=boardLegend(nodes,[{x:200,y:480}],"Test patch");
  expect(legend.y).toBeGreaterThan(480);
  expect(legend.y+legend.h).toBeLessThanOrEqual(520);
  expect(legend.title).toBe("TEST PATCH");
  expect(legend.summary).toContain("FILTER");
  expect(legend.summary).toContain("DRIVE");
  expect(legend.summary).not.toContain("IN");
});
