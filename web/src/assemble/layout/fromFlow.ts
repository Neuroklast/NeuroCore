import type { Edge, Node } from "../flowTypes";
import type { ChipData } from "../flowFromAst";
import { chipBox, jackAnchor } from "../chipLayout";
import { handleId, parseHandle } from "../handles";
import { snapSize } from "../grid";
import { cableFace } from "../validateLink";
import type { LayoutEdge, LayoutNode } from "./types";

export function flowToLayout(
  nodes: Node<ChipData>[],
  edges: Edge[],
): { nodes: LayoutNode[]; edges: LayoutEdge[] } {
  const ln: LayoutNode[] = nodes.map((n) => {
    const box = chipBox(n.data.type, n.data.jacks, false, n.data.args);
    const measured = (n as { measured?: { width?: number; height?: number } }).measured;
    const w = snapSize(measured?.width ?? n.width ?? box.w);
    const h = snapSize(measured?.height ?? n.height ?? box.h);
    const sideIn = n.data.jacks.filter((j) => ! j.output && cableFace(j.kind) === "side");
    const sideOut = n.data.jacks.filter((j) => j.output && cableFace(j.kind) === "side");
    return {
      id: n.id,
      x: n.position.x,
      y: n.position.y,
      w,
      h,
      ins: sideIn.map((j) => ({
        id: j.id,
        y: jackAnchor({ x: 0, y: 0 }, n.data.type, n.data.jacks, handleId(j.id, false), false, h, w).y,
      })),
      outs: sideOut.map((j) => ({
        id: j.id,
        y: jackAnchor({ x: 0, y: 0 }, n.data.type, n.data.jacks, handleId(j.id, true), true, h, w).y,
      })),
    };
  });
  const le: LayoutEdge[] = edges
    .filter((e) => e.className !== "temp")
    .map((e) => ({
      id: e.id,
      source: String(e.source),
      target: String(e.target),
      fromJack: parseHandle(e.sourceHandle)?.id ?? "out",
      toJack: parseHandle(e.targetHandle)?.id ?? "in",
    }));
  return { nodes: ln, edges: le };
}
