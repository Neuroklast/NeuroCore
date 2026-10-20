import { expect, it } from "vitest";
import { writeFileSync, mkdirSync } from "node:fs";
import { parseDslSketch } from "../../presets/parseDslSketch";
import { findFactory } from "../../presets/factoryCatalog";
import { hydrateBoard, graphToLayout } from "../boardModel";
import { parseRoutePath } from "../boardPath";
import { countCorners } from "../audioStep";
import { placeSerialChain } from "./serialPlacement";
import { compact, arrange, pathLength } from "./runLayout";

for (const width of [960, 1440]) {
  it(`composes every Drone Low Bed hop within reserved PCB row channels at ${width}px`, async () => {
    const { doc } = parseDslSketch(findFactory("Drone Low Bed")!.script);
    const graph = graphToLayout(hydrateBoard(doc));
    const result = await compact(graph.nodes, graph.edges, { w: width, h: 520 });
    const widths = ["octaver1", "filter1", "reverb1"].map(id => result.nodes[id]!.w);
    const routes = graph.edges.map(edge => {
      const points = parseRoutePath(result.edgePaths[edge.id]!);
      expect(points.length, edge.id).toBeGreaterThanOrEqual(2);
      const start = points[0]!, end = points.at(-1)!;
      const length = pathLength(points);
      const westward = points.slice(1).reduce((sum, p, i) => sum + Math.max(0, points[i]!.x - p.x), 0);
      const direct = Math.abs(end.x - start.x) + Math.abs(end.y - start.y);
      const rowChange=end.y>start.y+128;
      expect(westward,edge.id).toBeLessThanOrEqual(rowChange?960:0);
      expect(countCorners(points),edge.id).toBeLessThanOrEqual(rowChange?8:0);
      expect(length,edge.id).toBeLessThanOrEqual(direct+(rowChange?128:0));
      expect(length,edge.id).toBeLessThanOrEqual(1200);
      return { id: edge.id, source: edge.source, target: edge.target, length, westward, corners: countCorners(points), points };
    });
    expect(Math.max(...widths) / Math.min(...widths)).toBeLessThanOrEqual(1.25);
    const boxes = Object.values(result.nodes);
    let collisions = 0, crossings = 0;
    for (let i=0; i<boxes.length; i++) for (let j=i+1; j<boxes.length; j++) {
      const a=boxes[i]!, b=boxes[j]!;
      if (a.x < b.x+b.w && a.x+a.w > b.x && a.y < b.y+b.h && a.y+a.h > b.y) collisions++;
    }
    for (let i=0; i<routes.length; i++) for (let j=i+1; j<routes.length; j++) {
      const a=routes[i]!.points, b=routes[j]!.points;
      // Straight same-height routes must occupy disjoint intervals.
      if (a[0]!.y === b[0]!.y && Math.max(a[0]!.x,b[0]!.x) < Math.min(a.at(-1)!.x,b.at(-1)!.x)) crossings++;
    }
    let cableCollisions = 0;
    for (const route of routes) {
      for (const [id, box] of Object.entries(result.nodes)) {
        if (id === route.source || id === route.target) continue;
        for (let i=1; i<route.points.length; i++) {
          const a=route.points[i-1]!, b=route.points[i]!;
          if (a.y > box.y && a.y < box.y+box.h
            && Math.max(a.x,b.x) > box.x && Math.min(a.x,b.x) < box.x+box.w) cableCollisions++;
        }
      }
    }
    expect(cableCollisions).toBe(0);
    expect(collisions).toBe(0);
    expect(crossings).toBe(0);
    if (process.env.NK_LAYOUT_EVIDENCE) {
      mkdirSync(process.env.NK_LAYOUT_EVIDENCE,{recursive:true});
      writeFileSync(`${process.env.NK_LAYOUT_EVIDENCE}/metrics-${width}.json`, JSON.stringify({width,nodes:result.nodes,routes,collisions,cableCollisions,crossings,widthRatio:Math.max(...widths)/Math.min(...widths)},null,2));
    }
  });
}


it("uses the same PCB composition for Arrange and Compact, independent of input order", async () => {
  const { doc } = parseDslSketch(findFactory("Drone Low Bed")!.script);
  const graph = graphToLayout(hydrateBoard(doc));
  const a = await arrange([...graph.nodes].reverse(), [...graph.edges].reverse(), {w:960,h:520});
  const b = await compact(graph.nodes, graph.edges, {w:960,h:520});
  expect(a).toEqual(b);
});

it("leaves branches and modulation graphs to the general graph strategy", () => {
  for (const name of ["Pad Cloud Hall", "Bass Fractionator"]) {
    const { doc } = parseDslSketch(findFactory(name)!.script);
    const graph = graphToLayout(hydrateBoard(doc));
    expect(placeSerialChain(graph.nodes, graph.edges), name).toBeNull();
  }
});
