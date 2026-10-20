import { describe, expect, it } from "vitest";
import {
  navigationTree,
  boundaryLinks,
  scopeGraph,
  boardGridStep,
  cableEnvelope,
  fitReadableRegion,
} from "./boardNavigation";
import type { BoardGraph } from "./boardModel";
function chain(count: number): BoardGraph {
  const nodes = Object.fromEntries(
    Array.from({ length: count }, (_, i) => [
      String(i),
      {
        id: String(i),
        type: "filter",
        role: "chip" as const,
        x: i * 384,
        y: 0,
        w: 320,
        h: 256,
        args: {},
        label: `Filter ${i}`,
        channel: "main",
        locked: false,
      },
    ]),
  );
  const edges = Object.fromEntries(
    Array.from({ length: count - 1 }, (_, i) => [
      String(i),
      {
        id: String(i),
        sourceNodeId: String(i),
        targetNodeId: String(i + 1),
        sourcePortId: `${i}o`,
        targetPortId: `${i + 1}i`,
        kind: "audio" as const,
        route: [],
      },
    ]),
  );
  return { nodes, edges, ports: {} };
}
describe("large Circuit navigation contract", () => {
  it.each([20, 100, 200])(
    "bounds every overview and retains every real chip for %i nodes",
    (count) => {
      const graph = chain(count),
        before = JSON.stringify(graph),
        tree = navigationTree(graph);
      const leaves: string[] = [];
      function visit(g: ReturnType<typeof navigationTree>) {
        expect(g.children.length).toBeLessThanOrEqual(6);
        if (g.children.length) g.children.forEach(visit);
        else {
          expect(g.ids.length).toBeLessThanOrEqual(3);
          leaves.push(...g.ids);
        }
      }
      visit(tree);
      expect(new Set(leaves).size).toBe(count);
      expect(JSON.stringify(graph)).toBe(before);
    },
  );
  it("exposes real cross-boundary links without inventing graph ports", () => {
    const graph = chain(10),
      scoped = scopeGraph(graph, ["3", "4", "5"]);
    expect(Object.keys(scoped.nodes)).toEqual(["3", "4", "5"]);
    expect(Object.keys(scoped.edges)).toEqual(["3", "4"]);
    expect(boundaryLinks(graph, ["3", "4", "5"]).map((e) => e.id)).toEqual([
      "2",
      "5",
    ]);
  });
  it("keeps the stationary dot grid bounded at extreme zoom", () => {
    for (const scale of [0.01, 0.1, 0.5, 1])
      expect(boardGridStep(scale) * scale).toBeGreaterThanOrEqual(16);
  });
  it("reserves navigation space and keeps effective detail text above 8 px", () => {
    const g = chain(3),
      camera = fitReadableRegion(
        Object.values(g.nodes),
        { w: 1280, h: 540 },
        0.75,
      );
    expect(15 * camera.scale * 0.75).toBeGreaterThanOrEqual(8);
    const center = (g.nodes["0"]!.x + g.nodes["2"]!.x + 320) / 2;
    expect(Math.abs(center * camera.scale + camera.tx - 640)).toBeLessThan(1);
  });
  it("shows sustained energy without fabricated signal or clipping", () => {
    expect(cableEnvelope(0, 0)).toEqual({ body: 0, contact: 0, clip: false });
    expect(cableEnvelope(0.2, 0.5).body).toBeGreaterThan(0);
    expect(cableEnvelope(0.2, 0.5).clip).toBe(false);
    expect(cableEnvelope(0.2, 1.01).clip).toBe(true);
    expect(cableEnvelope(Number.NaN, Number.NaN)).toEqual({
      body: 0,
      contact: 0,
      clip: false,
    });
  });
});
