import { BOARD_GRID, BOARD_HALF, snapToCellCenter, snapToGrid } from "./grid";

/** Centre of a jack, relative to the node top-left. Never a global. */
export type PortLocal = { x: number; y: number };

const PITCH = BOARD_GRID * 2;

export function sidePortMinHeight(count: number): number {
  const n = Math.max(1, count);
  if (n <= 1) {
    return BOARD_HALF * 2;
  }
  return BOARD_HALF * 2 + (n - 1) * PITCH;
}

/**
 * 1..N jacks on the west (x=0) or east (x=w) face.
 * Centres only. Equal pitch. Inside the box. Cell midlines.
 */
export function sidePortLocals(
  count: number,
  box: { w: number; h: number },
  east: boolean,
): PortLocal[] {
  const n = Math.max(0, Math.floor(count));
  if (n === 0) {
    return [];
  }
  const x = east ? box.w : 0;
  const pitch = n >= 2 ? PITCH : 0;
  const mid = snapToCellCenter(box.h * 0.5);
  return Array.from({ length: n }, (_, i) => {
    const y = snapToCellCenter(mid + (i - (n - 1) / 2) * pitch);
    return { x, y };
  });
}

export function globalPort(
  node: { x: number; y: number },
  local: PortLocal,
): { x: number; y: number } {
  return {
    x: snapToGrid(node.x + local.x),
    y: snapToCellCenter(node.y + local.y),
  };
}

/** RF Handle: `top` is the jack centre. CSS translates -50%/-50%. */
export function portHandleStyle(local: PortLocal): { top: number } {
  return { top: local.y };
}
