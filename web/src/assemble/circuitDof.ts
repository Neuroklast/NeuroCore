import { motionAllows, type MotionPref } from "../theme/motionPolicy";

export type DofEdge = { id: string; source: string; target: string };

export type FocusPlane = {
  active: boolean;
  nodes: Set<string>;
  edges: Set<string>;
};

export function circuitDofAllowed(motion: MotionPref, prefersReduced: boolean): boolean {
  return motionAllows("dof", motion, prefersReduced);
}

/** Selected chain ∪ hover path (node + incident edges + neighbors). */
export function focusPlane(input: {
  selectedNodeIds: readonly string[];
  selectedEdgeIds: readonly string[];
  hoverNodeId?: string | null;
  edges: readonly DofEdge[];
}): FocusPlane {
  const nodes = new Set<string>();
  const edgeSet = new Set<string>();
  const byId = new Map(input.edges.map((e) => [e.id, e]));

  for (const id of input.selectedNodeIds) {
    nodes.add(id);
  }
  for (const id of input.selectedEdgeIds) {
    edgeSet.add(id);
    const e = byId.get(id);
    if (e) {
      nodes.add(e.source);
      nodes.add(e.target);
    }
  }
  for (const e of input.edges) {
    if (nodes.has(e.source) && nodes.has(e.target)) {
      edgeSet.add(e.id);
    }
  }

  const hover = input.hoverNodeId;
  if (hover) {
    nodes.add(hover);
    for (const e of input.edges) {
      if (e.source === hover || e.target === hover) {
        edgeSet.add(e.id);
        nodes.add(e.source);
        nodes.add(e.target);
      }
    }
  }

  return {
    active: nodes.size > 0 || edgeSet.size > 0,
    nodes,
    edges: edgeSet,
  };
}

export function focusAttr(
  allowed: boolean,
  plane: FocusPlane,
  id: string,
  kind: "node" | "edge" = "node",
): "sharp" | "soft" | "off" {
  if (! allowed || ! plane.active) {
    return "off";
  }
  const sharp = kind === "edge" ? plane.edges.has(id) : plane.nodes.has(id);
  return sharp ? "sharp" : "soft";
}
