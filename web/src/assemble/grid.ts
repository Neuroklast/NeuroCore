/** One board cell. Chips, jacks, and snap use this. */
export const BOARD_GRID = 32;
/** 4×4 cells: faint square border. */
export const BOARD_BLOCK = BOARD_GRID * 4;
/** Center point of a cell. */
export const BOARD_DOT = 1.25;
/** Audio tube and jack thickness. Stays half a cell. */
export const BOARD_TRACE = 16;
/** Air between two 16 px tubes on neighbouring cell midlines. */
export const BOARD_GAP = BOARD_GRID - BOARD_TRACE;
/** Parallel center-to-center. One cell. */
export const BOARD_RAIL = BOARD_GRID;
/** Centerline stays one full cell off every chip box. */
export const BOARD_PAD = BOARD_GRID;
/** Neighbours in a row: two cells so both 32 px jack stubs fit without a maze. */
export const CHIP_AIR_X = BOARD_GRID * 2;
/** Neighbours in a column: one cell. */
export const CHIP_AIR_Y = BOARD_GRID;
/** Midline of a cell. Cables and jacks sit here so parallels are one cell apart. */
export const BOARD_HALF = BOARD_GRID * 0.5;

export type GridDot = { x: number; y: number };
export type GridRect = { x: number; y: number; w: number; h: number };

export function snapToGrid(n: number): number {
  if (! Number.isFinite(n)) {
    return BOARD_GRID;
  }
  return Math.round(n / BOARD_GRID) * BOARD_GRID;
}

/** Grow to the next cell so content never shrinks off the grid. */
export function snapSize(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return BOARD_GRID;
  }
  return Math.max(BOARD_GRID, Math.ceil(n / BOARD_GRID) * BOARD_GRID);
}

export function snapPoint(p: { x: number; y: number }): { x: number; y: number } {
  return { x: snapToGrid(p.x), y: snapToGrid(p.y) };
}

/** Snap a coordinate onto a cell midline (16, 48, 80, …). */
export function snapToCellCenter(n: number): number {
  if (! Number.isFinite(n)) {
    return BOARD_HALF;
  }
  return Math.round((n - BOARD_HALF) / BOARD_GRID) * BOARD_GRID + BOARD_HALF;
}

export function onGrid(n: number, cell = BOARD_GRID): boolean {
  return Math.abs(n - Math.round(n / cell) * cell) < 1e-6;
}

export function onCellCenter(n: number): boolean {
  return Math.abs(n - snapToCellCenter(n)) < 1e-6;
}

export function cellDot(cell = BOARD_GRID): GridDot {
  return { x: cell * 0.5, y: cell * 0.5 };
}

export function blockBorder(cell = BOARD_GRID): GridRect {
  return { x: 0, y: 0, w: cell * 4, h: cell * 4 };
}

/** Paint contract the background must follow. */
export function boardGridPaint(): {
  cell: number;
  block: number;
  crosses: boolean;
  trace: number;
  rail: number;
  pad: number;
  dot: number;
} {
  return {
    cell: BOARD_GRID,
    block: BOARD_BLOCK,
    crosses: false,
    trace: BOARD_TRACE,
    rail: BOARD_RAIL,
    pad: BOARD_PAD,
    dot: BOARD_DOT,
  };
}
