import type { AstDocument } from "../bridge/ast";
import { assignRanks, hydrateBoard, type BoardNode } from "../assemble/boardModel";
import { isEnvNode, isLfoNode } from "../assemble/flowFromAst";

/** Navigator groups. Same graph as Circuit; not a second patch document. */
export type PatchStructureGroup = "input" | "audio" | "parallel" | "mod" | "output";

export type PatchStructureNode = {
  id: string;
  type: string;
  group: PatchStructureGroup;
};

function groupOf(n: BoardNode): PatchStructureGroup {
  if (n.type === "in" || n.id === "IN") {
    return "input";
  }
  if (n.type === "out" || n.id === "OUT") {
    return "output";
  }
  if (isLfoNode(n) || isEnvNode(n)) {
    return "mod";
  }
  const bus = (n.busName || "").toLowerCase();
  if (bus && bus !== "main") {
    return "parallel";
  }
  return "audio";
}

/** Derive the Terminal structure list from the same hydrated board as Circuit. */
export function patchStructure(ast: AstDocument): PatchStructureNode[] {
  const graph = hydrateBoard(ast);
  const rank = assignRanks(graph);
  return Object.values(graph.nodes)
    .sort((a, b) => (rank[a.id] ?? 0) - (rank[b.id] ?? 0) || a.id.localeCompare(b.id))
    .map((n) => ({ id: n.id, type: n.type, group: groupOf(n) }));
}
