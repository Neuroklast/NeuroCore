import { describe, expect, it } from "vitest";
import { motionAllows } from "../theme/motionPolicy";
import {
  circuitDofAllowed,
  focusAttr,
  focusPlane,
} from "./circuitDof";

const edges = [
  { id: "e-in-a", source: "IN", target: "a" },
  { id: "e-a-b", source: "a", target: "b" },
  { id: "e-b-out", source: "b", target: "OUT" },
];

describe("circuit depth of field", () => {
  it("gates blur behind motionPolicy dof (off / prefers-reduced → no blur)", () => {
    expect(motionAllows("dof", "full", false)).toBe(true);
    expect(motionAllows("dof", "reduced", false)).toBe(false);
    expect(motionAllows("dof", "off", false)).toBe(false);
    expect(motionAllows("dof", "full", true)).toBe(false);
    expect(circuitDofAllowed("full", false)).toBe(true);
    expect(circuitDofAllowed("off", false)).toBe(false);
    expect(circuitDofAllowed("full", true)).toBe(false);
  });

  it("selected chain is sharp; other chips/edges are soft", () => {
    const plane = focusPlane({
      selectedNodeIds: ["a", "b"],
      selectedEdgeIds: ["e-a-b"],
      hoverNodeId: null,
      edges,
    });
    expect(plane.active).toBe(true);
    expect([...plane.nodes].sort()).toEqual(["a", "b"]);
    expect([...plane.edges].sort()).toEqual(["e-a-b"]);
    expect(focusAttr(true, plane, "a")).toBe("sharp");
    expect(focusAttr(true, plane, "IN")).toBe("soft");
    expect(focusAttr(true, plane, "e-a-b", "edge")).toBe("sharp");
    expect(focusAttr(true, plane, "e-in-a", "edge")).toBe("soft");
  });

  it("hover path expands to incident edges and neighbor chips", () => {
    const plane = focusPlane({
      selectedNodeIds: [],
      selectedEdgeIds: [],
      hoverNodeId: "a",
      edges,
    });
    expect(plane.active).toBe(true);
    expect(plane.nodes.has("a")).toBe(true);
    expect(plane.nodes.has("IN")).toBe(true);
    expect(plane.nodes.has("b")).toBe(true);
    expect(plane.nodes.has("OUT")).toBe(false);
    expect(plane.edges.has("e-in-a")).toBe(true);
    expect(plane.edges.has("e-a-b")).toBe(true);
    expect(plane.edges.has("e-b-out")).toBe(false);
  });

  it("no plane or dof denied → data-focus off (no blur)", () => {
    const empty = focusPlane({
      selectedNodeIds: [],
      selectedEdgeIds: [],
      hoverNodeId: null,
      edges,
    });
    expect(empty.active).toBe(false);
    expect(focusAttr(true, empty, "a")).toBe("off");
    expect(focusAttr(false, {
      active: true,
      nodes: new Set(["a"]),
      edges: new Set(),
    }, "a")).toBe("off");
  });
});
