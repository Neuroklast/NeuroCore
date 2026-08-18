import { getStraightPath, type ConnectionLineComponentProps } from "@xyflow/react";

export function dragLinePath(fromX: number, fromY: number, toX: number, toY: number): string {
  const [d] = getStraightPath({
    sourceX: fromX,
    sourceY: fromY,
    targetX: toX,
    targetY: toY,
  });
  return d;
}

/** Drag is a free line. PCB routing runs after drop only. */
export function ConnectionLine({
  fromX,
  fromY,
  toX,
  toY,
}: ConnectionLineComponentProps) {
  const d = dragLinePath(fromX, fromY, toX, toY);
  return (
    <path
      d={d}
      fill="none"
      stroke="#ff003c"
      strokeWidth={2}
      strokeLinecap="butt"
      opacity={0.7}
    />
  );
}
