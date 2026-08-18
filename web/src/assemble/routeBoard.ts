import type { Edge, Node } from "@xyflow/react";
import { chipBox, jackAnchor } from "./chipLayout";
import type { ChipData } from "./flowFromAst";
import { tubePath, type Obstacle, type Pt } from "./tubePath";

export function routeBoard(
  nodes: Node<ChipData>[],
  edges: Edge[],
): Map<string, { d: string; points: Pt[] }> {
  const obs: Obstacle[] = nodes.map((n) => {
    const box = chipBox(n.data.type, n.data.jacks, false, n.data.args);
    return {
      id: n.id,
      x: n.position.x,
      y: n.position.y,
      w: n.width ?? box.w,
      h: n.height ?? box.h,
    };
  });
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const reserved: Pt[][] = [];
  const out = new Map<string, { d: string; points: Pt[] }>();
  const ordered = [...edges].sort((a, b) => a.id.localeCompare(b.id));
  for (const e of ordered) {
    if (e.className === "temp") {
      continue;
    }
    const src = byId.get(e.source);
    const dst = byId.get(e.target);
    if (! src || ! dst) {
      continue;
    }
    const sb = chipBox(src.data.type, src.data.jacks, false, src.data.args);
    const db = chipBox(dst.data.type, dst.data.jacks, false, dst.data.args);
    const a = jackAnchor(
      src.position,
      src.data.type,
      src.data.jacks,
      e.sourceHandle,
      true,
      src.height ?? sb.h,
      src.width ?? sb.w,
    );
    const b = jackAnchor(
      dst.position,
      dst.data.type,
      dst.data.jacks,
      e.targetHandle,
      false,
      dst.height ?? db.h,
      dst.width ?? db.w,
    );
    const routed = tubePath(a.x, a.y, b.x, b.y, {
      obstacles: obs,
      sourceId: src.id,
      targetId: dst.id,
      reserved,
    });
    reserved.push(routed.points);
    out.set(e.id, { d: routed.d, points: routed.points });
  }
  return out;
}
