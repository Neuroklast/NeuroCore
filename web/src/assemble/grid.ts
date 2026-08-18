export const BOARD_GRID = 16;

export function snapToGrid(n: number): number {
  if (! Number.isFinite(n)) {
    return BOARD_GRID;
  }
  return Math.round(n / BOARD_GRID) * BOARD_GRID;
}

export function snapPoint(p: { x: number; y: number }): { x: number; y: number } {
  return { x: snapToGrid(p.x), y: snapToGrid(p.y) };
}
