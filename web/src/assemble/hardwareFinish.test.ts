import { expect, it } from "vitest";
import { ioFaceSize, dspFaceSize, sideJackBand } from "./chipMetrics";
import { visualJacksFor } from "./visualEdges";
import { sidePortLocals } from "./portLayout";
import { overlayParamKeys, chipSpec } from "./chipSpec";
import { packRows } from "./layout/compactPack";

it("uses a compact connector housing", () => {
  expect(ioFaceSize(2).h).toBeLessThanOrEqual(dspFaceSize().h * .625);
});
it("keeps enum and routing fields off the modulation rail", () => {
  const jacks = visualJacksFor({id:"f",type:"filter",args:{type:"lowpass",channel:"stereo"},jacks:[],busName:"main",trailingComment:""});
  expect(jacks.some(j => j.id === "cutoff")).toBe(true);
  expect(jacks.some(j => j.id === "type" || j.id === "channel")).toBe(false);
  expect(overlayParamKeys(chipSpec("filter"), {type:"lowpass",channel:"stereo"})).toContain("type");
});
it("packs each row using its own actual widths", () => {
  const nodes = [128,224,416,224].map((w,i)=>({id:i===0?"IN":`n${i}`,w,h:256,ins:[{id:"in",y:144}],outs:[{id:"out",y:144}]}));
  const edges=nodes.slice(1).map((n,i)=>({id:`e${i}`,source:nodes[i]!.id,target:n.id,fromJack:"out",toJack:"in"}));
  const p=packRows(nodes,edges,{w:960,h:800});
  expect(p.n1!.x-p.IN!.x-p.IN!.w).toBe(128);
  expect(p.n3!.x-p.n2!.x-p.n2!.w).toBe(128);
  expect(p.n3!.x+p.n3!.w).toBeLessThanOrEqual(928);
});

it("contains the complete IO pin hit areas in its signal band", () => {
  for (const count of [2,3,4]) {
    const box = ioFaceSize(count);
    const band = sideJackBand(box.h, "io");
    for (const p of sidePortLocals(count, box, true, "io")) {
      expect(p.y - 12).toBeGreaterThanOrEqual(band.top);
      expect(p.y + 12).toBeLessThanOrEqual(band.top + band.innerH);
    }
  }
});

it("reserves terminal room and aligns a compact OUT to the final processor", () => {
  const nodes = [160, 352, 224, 288, 160].map((w, i) => ({
    id: i === 0 ? "IN" : i === 4 ? "OUT" : `n${i}`,
    w, h: i === 0 || i === 4 ? 160 : 256,
    ins: [{ id: "in", y: i === 4 ? 80 : 144 }],
    outs: [{ id: "out", y: i === 0 ? 80 : 144 }],
  }));
  const edges = nodes.slice(1).map((n, i) => ({
    id: `e${i}`, source: nodes[i]!.id, target: n.id, fromJack: "out", toJack: "in",
  }));
  const p = packRows(nodes, edges, { w: 1200, h: 800 });
  expect(p.n2!.y).toBeGreaterThan(p.n1!.y);
  expect(p.OUT!.x - p.n3!.x - p.n3!.w).toBe(128);
  expect(p.OUT!.y + 80).toBe(p.n3!.y + 144);
  expect(p.OUT!.x + p.OUT!.w).toBeLessThanOrEqual(1200);
});
