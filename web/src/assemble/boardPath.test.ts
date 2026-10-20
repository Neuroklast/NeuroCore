import { describe, expect, it } from "vitest";
import { shortestPath } from "./boardPath";

const graph = (edges: Array<[string, string]>) => {
  const e: Record<string, { sourceNodeId: string; targetNodeId: string; route: Array<{ x: number; y: number }> }> = {};
  let i = 0;
  for (const [s, t] of edges) {
    e[`e${i++}`] = { sourceNodeId: s, targetNodeId: t, route: [] };
  }
  return { nodes: {}, edges: e };
};

describe("shortestPath (gerichtete Graph-Pfadsuche)", () => {
  it("gibt den direkten Weg zurueck", () => {
    const g = graph([["A", "B"], ["B", "C"]]);
    expect(shortestPath(g, "A", "C")).toEqual(["A", "B", "C"]);
  });

  it("gibt [] zurueck, wenn kein Weg existiert", () => {
    const g = graph([["A", "B"], ["C", "D"]]);
    expect(shortestPath(g, "A", "D")).toEqual([]);
  });

  it("folgt der Richtung (keine Rueckwaerts-Kante)", () => {
    const g = graph([["A", "B"]]);
    expect(shortestPath(g, "B", "A")).toEqual([]);
  });
  it("start gleich ziel liefert das einzelne Element", () => {
    const g = graph([["A", "B"]]);
    expect(shortestPath(g, "A", "A")).toEqual(["A"]);
  });

  it("waehlt bei parallelen Wegen den kurzesten (stabile Tie-Break)", () => {
    const g = graph([
      ["A", "B"], ["B", "C"], ["A", "C"],
    ]);
    expect(shortestPath(g, "A", "C")).toEqual(["A", "C"]);
  });

  it("ignoriert Knoten ohne ausgehende Kante", () => {
    const g = graph([["A", "B"], ["C", "A"]]);
    expect(shortestPath(g, "C", "B")).toEqual(["C", "A", "B"]);
  });
});
