import { BOARD_GRID, snapToCellCenter } from "./grid";
import { chamferWaypoints } from "./layout/chamfer";
import type { Pt } from "./layout/types";

/** Dest is left and below source: compact wrap to the next row, not a same-row U-turn. */
export function isWrapJack(from: Pt, to: Pt): boolean {
  return to.x < from.x && to.y > from.y + BOARD_GRID;
}

export function wrapRailY(
  source: { y: number; h: number },
  target: { y: number },
): number {
  return snapToCellCenter((source.y + source.h + target.y) / 2);
}

/** East stub, drop to the rail between rows, west, dest stub. Not around the hull. */
export function wrapRailRoute(from: Pt, to: Pt, railY: number): Pt[] {
  const ax = from.x + BOARD_GRID;
  const bx = to.x - BOARD_GRID;
  return [
    from,
    { x: ax, y: from.y },
    { x: ax, y: railY },
    { x: bx, y: railY },
    { x: bx, y: to.y },
    to,
  ];
}

export function parseRoutePath(d: string): Pt[] {
  const pts: Pt[] = [];
  const re = /[ML]\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)/g;
  let m: RegExpExecArray | null = re.exec(d);
  while (m) {
    pts.push({ x: Number(m[1]), y: Number(m[2]) });
    m = re.exec(d);
  }
  return pts;
}

export function dragLinePath(fromX: number, fromY: number, toX: number, toY: number): string {
  return `M ${fromX} ${fromY} L ${toX} ${toY}`;
}

export function stubRoute(from: Pt, to: Pt): Pt[] {
  const ax = from.x + BOARD_GRID;
  const bx = to.x - BOARD_GRID;
  if (Math.abs(from.y - to.y) < 0.6) {
    return chamferWaypoints([from, { x: ax, y: from.y }, { x: bx, y: to.y }, to]);
  }
  return chamferWaypoints([
    from,
    { x: ax, y: from.y },
    { x: ax, y: to.y },
    { x: bx, y: to.y },
    to,
  ]);
}

function near(a: Pt, b: Pt): boolean {
  return Math.abs(a.x - b.x) <= BOARD_GRID && Math.abs(a.y - b.y) <= BOARD_GRID;
}

/** Stored PCB that does not meet the current jacks is empty space — drop it. */
export function routeFitsPorts(from: Pt, to: Pt, route: Pt[]): boolean {
  if (route.length < 2) {
    return false;
  }
  return near(route[0]!, from) && near(route[route.length - 1]!, to);
}

/** Paint the stored route only when both ends still sit on the jacks. Always source → dest. */
export function paintRoute(from: Pt, to: Pt, route: Pt[]): Pt[] {
  if (route.length < 2) {
    return stubRoute(from, to);
  }
  const last = route[route.length - 1]!;
  const forward = near(route[0]!, from) && near(last, to);
  const backward = near(route[0]!, to) && near(last, from);
  if (! forward && ! backward) {
    return stubRoute(from, to);
  }
  const pinned = (backward ? route.slice().reverse() : route.slice());
  pinned[0] = from;
  pinned[pinned.length - 1] = to;
  return pinned;
}
