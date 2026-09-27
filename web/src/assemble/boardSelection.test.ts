import { expect, it } from "vitest";
import { nodesInSelection, routeMidpoint } from "./boardSelection";
import type { BoardNode } from "./boardModel";

it("selects every intersecting node regardless of drag direction", () => {
  const nodes = [
    { id: "a", x: 10, y: 10, w: 40, h: 40 },
    { id: "b", x: 60, y: 10, w: 40, h: 40 },
    { id: "c", x: 200, y: 10, w: 40, h: 40 },
  ] as BoardNode[];
  expect(nodesInSelection(nodes, { x0: 105, y0: 60, x1: 5, y1: 0 })).toEqual(["a", "b"]);
});

it("places an insertion control halfway along a bent route", () => {
  expect(routeMidpoint([{ x: 0, y: 0 }, { x: 100, y: 0 }, { x: 100, y: 200 }])).toEqual({ x: 100, y: 50 });
});
