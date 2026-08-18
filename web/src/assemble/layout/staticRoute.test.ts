import { describe, expect, it } from "vitest";
import { staticRoute } from "../StaticGridEdge";
import { applyLayoutResult } from "./layoutClient";

describe("StaticGridEdge route", () => {
  it("paints only the stored SVG d string", () => {
    expect(staticRoute({ route: "M0 0L32 0" })).toBe("M0 0L32 0");
    expect(staticRoute({})).toBe("");
    expect(staticRoute(undefined)).toBe("");
  });

  it("writes worker paths onto edge data and optional node origins", () => {
    const nodes = [{ id: "a", position: { x: 0, y: 0 } }];
    const edges = [{ id: "e0", data: { kind: "audio", route: "" } }];
    const moved = applyLayoutResult(
      { nodes: { a: { x: 64, y: 32, w: 128, h: 64 } }, edgePaths: { e0: "M0 16L64 16" } },
      nodes,
      edges,
      true,
    );
    expect(moved.nodes[0]!.position).toEqual({ x: 64, y: 32 });
    expect((moved.edges[0]!.data as { route: string }).route).toBe("M0 16L64 16");
    const kept = applyLayoutResult(
      { nodes: { a: { x: 96, y: 0, w: 128, h: 64 } }, edgePaths: { e0: "M1 1L2 2" } },
      nodes,
      edges,
      false,
    );
    expect(kept.nodes[0]!.position).toEqual({ x: 0, y: 0 });
    expect((kept.edges[0]!.data as { route: string }).route).toBe("M1 1L2 2");
  });
});
