import { expect, it } from "vitest";
import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { compact } from "./runLayout";
import type { LayoutNode } from "./types";
for (const count of [6, 22, 102, 202])
  it(`keeps ${count} components on a compact two-dimensional PCB`, async () => {
    const nodes: LayoutNode[] = Array.from({ length: count }, (_, i) => ({
      id: i === 0 ? "IN" : i === count - 1 ? "OUT" : `chip${i}`,
      w: i === 0 || i === count - 1 ? 160 : 320,
      h: 256,
      ins: [{ id: "in", y: 128 }],
      outs: [{ id: "out", y: 128 }],
    }));
    const edges = nodes.slice(1).map((n, i) => ({
      id: `e${i}`,
      source: nodes[i]!.id,
      target: n.id,
      fromJack: "out",
      toJack: "in",
    }));
    const r = await compact(nodes, edges, { w: 1200, h: 580 });
    const boxes = Object.values(r.nodes),
      w = Math.max(...boxes.map((n) => n.x + n.w)),
      h = Math.max(...boxes.map((n) => n.y + n.h));
    expect(Object.keys(r.nodes)).toHaveLength(count);
    expect(w / h).toBeGreaterThan(1);
    expect(w / h).toBeLessThan(3.2);
    expect(new Set(boxes.map((n) => n.y)).size).toBeGreaterThan(1);
    expect(Object.keys(r.edgePaths)).toHaveLength(edges.length);
  }, 30000);
it("always mounts the complete board without region replacement", () => {
  const source = readFileSync(
    new URL("../BoardView.tsx", import.meta.url),
    "utf8",
  );
  expect(source).not.toContain("scopeGraph");
  expect(source).not.toContain("!overview");
});

import { parseDslSketch } from "../../presets/parseDslSketch";
import { findFactory } from "../../presets/factoryCatalog";
import { graphToLayout, hydrateBoard } from "../boardModel";
import { fitCamera, screenFromWorld } from "../boardCamera";
import { pathLength } from "./runLayout";
import { countCorners } from "../audioStep";
import { parseRoutePath } from "../boardPath";
function segmentHitsBox(
  a: { x: number; y: number },
  b: { x: number; y: number },
  r: { x: number; y: number; w: number; h: number },
) {
  let lo = 0,
    hi = 1;
  for (const [v, d, min, max] of [
    [a.x, b.x - a.x, r.x + 0.1, r.x + r.w - 0.1],
    [a.y, b.y - a.y, r.y + 0.1, r.y + r.h - 0.1],
  ]) {
    if (Math.abs(d!) < 1e-9) {
      if (v! < min! || v! > max!) return false;
    } else {
      const p = (min! - v!) / d!,
        q = (max! - v!) / d!;
      lo = Math.max(lo, Math.min(p, q));
      hi = Math.min(hi, Math.max(p, q));
    }
  }
  return lo < hi;
}
for (const name of [
  "Wide Motion",
  "Bass Regen Growl",
  "Multiband Punch Enhancer",
  "Drone Low Bed",
])
  it(`PCB routes avoid every unrelated chip in ${name}`, async () => {
    const { doc } = parseDslSketch(findFactory(name)!.script);
    const graph = graphToLayout(hydrateBoard(doc));
    const r = await compact(graph.nodes, graph.edges, { w: 1200, h: 560 });
    expect(Object.keys(r.edgePaths)).toHaveLength(graph.edges.length);
    const routes = graph.edges.map((e) => ({
      id: e.id,
      points: parseRoutePath(r.edgePaths[e.id]!),
    }));
    const cam = fitCamera(
      Object.values(r.nodes),
      { w: 1200, h: 560 },
      routes.flatMap((r) => r.points),
    );
    for (const route of routes)
      for (const p of route.points) {
        const q = screenFromWorld(cam, p.x, p.y);
        expect(q.x).toBeGreaterThanOrEqual(0);
        expect(q.x).toBeLessThanOrEqual(1200);
        expect(q.y).toBeGreaterThanOrEqual(56);
        expect(q.y).toBeLessThanOrEqual(560);
      }
    if (process.env.NK_LAYOUT_EVIDENCE) {
      mkdirSync(process.env.NK_LAYOUT_EVIDENCE, { recursive: true });
      writeFileSync(
        `${process.env.NK_LAYOUT_EVIDENCE}/${name.toLowerCase().replaceAll(" ", "-")}.json`,
        JSON.stringify(
          {
            name,
            nodes: r.nodes,
            camera: cam,
            routes: routes.map((r) => ({
              ...r,
              length: pathLength(r.points),
              corners: countCorners(r.points),
            })),
          },
          null,
          2,
        ),
      );
    }
    for (const edge of graph.edges) {
      const points = parseRoutePath(r.edgePaths[edge.id]!);
      for (const [id, box] of Object.entries(r.nodes)) {
        if (id === edge.source || id === edge.target) continue;
        expect(
          points.slice(1).some((b, i) => segmentHitsBox(points[i]!, b, box)),
          `${edge.id} crosses ${id}`,
        ).toBe(false);
      }
    }
  }, 30000);

it('aligns the connected jacks rather than the first declared socket',async()=>{
 const nodes:LayoutNode[]=[
  {id:'IN',w:160,h:160,ins:[],outs:[{id:'out',y:64},{id:'sc',y:96}]},
  {id:'filter',w:352,h:256,ins:[{id:'unused',y:64},{id:'in',y:128}],outs:[{id:'out',y:128}]},
  {id:'OUT',w:160,h:160,ins:[{id:'in',y:80}],outs:[]},
 ];
 const edges=[{id:'a',source:'IN',target:'filter',fromJack:'sc',toJack:'in'},{id:'b',source:'filter',target:'OUT',fromJack:'out',toJack:'in'}];
 const r=await compact(nodes,edges,{w:1400,h:560});
 expect(r.nodes.IN!.y+96).toBe(r.nodes.filter!.y+128);
 expect(countCorners(parseRoutePath(r.edgePaths.a!))).toBe(0);
});
