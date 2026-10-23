import { expect, it } from "vitest";
import { factoryRows } from "../presets/factoryCatalog";
import { parseDslSketch } from "../presets/parseDslSketch";
import { hydrateBoard } from "./boardModel";

it("every factory graph resolves all modulation connection endpoints", () => {
 const failures = factoryRows().flatMap(p => {
  const graph = hydrateBoard(parseDslSketch(p.script).doc);
  return (graph.connectionIssues ?? []).filter(issue=>issue.edge.kind === "mod").map(issue=>({preset:p.name,...issue}));
 });
 expect(failures).toEqual([]);
});
