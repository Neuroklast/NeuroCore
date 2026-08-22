import { Position, type Edge, type Node } from "./flowTypes";
import { audioStepPath, verticalRails, type Obstacle, type Pt } from "./audioStep";
import { chipBox, jackAnchor } from "./chipLayout";
import type { ChipData } from "./flowFromAst";
import { cableFace } from "./validateLink";
import { parseHandle } from "./handles";

function facePosition(jacks: ChipData["jacks"], handle: string | null | undefined, output: boolean): Position {
  const id = parseHandle(handle)?.id ?? (output ? "out" : "in");
  const jack = jacks.find((j) => j.id === id);
  const face = cableFace(jack?.kind ?? "audio");
  if (face === "bottom") {
    return Position.Bottom;
  }
  if (face === "top") {
    return Position.Top;
  }
  return output ? Position.Right : Position.Left;
}

export function routeBoard(
  nodes: Node<ChipData>[],
  edges: Edge[],
): Map<string, { d: string; points: Pt[]; centerX: number }> {
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const reservedXs: number[] = [];
  const reservedPaths: Pt[][] = [];
  const out = new Map<string, { d: string; points: Pt[]; centerX: number }>();
  const obstacles: Obstacle[] = nodes.map((n) => {
    const box = chipBox(n.data.type, n.data.jacks, false, n.data.args);
    return {
      id: n.id,
      x: n.position.x,
      y: n.position.y,
      w: n.width ?? box.w,
      h: n.height ?? box.h,
    };
  });
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
    const routed = audioStepPath(a.x, a.y, b.x, b.y, {
      sourcePosition: facePosition(src.data.jacks, e.sourceHandle, true),
      targetPosition: facePosition(dst.data.jacks, e.targetHandle, false),
      reservedXs,
      reservedPaths,
      obstacles,
      sourceId: src.id,
      targetId: dst.id,
    });
    reservedXs.push(routed.centerX, ...verticalRails(routed.points));
    reservedPaths.push(routed.points);
    out.set(e.id, routed);
  }
  return out;
}
