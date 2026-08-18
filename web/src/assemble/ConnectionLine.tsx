import { useStore, type ConnectionLineComponentProps } from "@xyflow/react";
import { TUBE } from "./tubeModel";
import { tubePath, type Obstacle } from "./tubePath";

export function ConnectionLine({
  fromX,
  fromY,
  toX,
  toY,
  fromNode,
  toNode,
}: ConnectionLineComponentProps) {
  const obstacles = useStore((s) => {
    const list: Obstacle[] = [];
    s.nodeLookup.forEach((n) => {
      const io = n.type === "io";
      list.push({
        id: n.id,
        x: n.internals.positionAbsolute.x,
        y: n.internals.positionAbsolute.y,
        w: n.measured.width ?? n.width ?? (io ? 96 : 220),
        h: n.measured.height ?? n.height ?? (io ? 56 : 80),
      });
    });
    return list;
  });
  const path = tubePath(fromX, fromY, toX, toY, {
    obstacles,
    sourceId: fromNode?.id,
    targetId: toNode?.id,
  }).d;
  return (
    <g>
      <path d={path} fill="none" stroke="#2a0606" strokeWidth={TUBE.audioOuter} strokeLinecap="butt" strokeLinejoin="miter" />
      <path d={path} fill="none" stroke="#050505" strokeWidth={TUBE.audioGlass} strokeLinecap="butt" strokeLinejoin="miter" />
      <path d={path} fill="none" stroke="#ff003c" strokeWidth={TUBE.audioBore} strokeLinecap="butt" strokeLinejoin="miter" opacity={0.55} />
    </g>
  );
}
