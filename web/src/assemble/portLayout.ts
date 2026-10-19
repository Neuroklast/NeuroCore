import { CHIP_PAD_Y, JACK_PITCH, sideJackBand, sideJackPitch, TITLE_H } from "./chipMetrics";
import { BOARD_GRID, BOARD_HALF, snapToCellCenter, snapToGrid } from "./grid";

/** Centre of a jack, relative to the node top-left. Never a global. */
export type PortLocal = { x: number; y: number };

export function sidePortMinHeight(count: number): number {
  const n = Math.max(1, count);
  if (n <= 1) {
    return BOARD_HALF * 2;
  }
  return TITLE_H + CHIP_PAD_Y + (n - 1) * JACK_PITCH;
}

/**
 * 1..N jacks on the west (x=0) or east (x=w) face.
 * One jack: chip midline. Two+: equal pitch around the body-band midline
 * (below the title, above the foot) so IN out/sc are not in the hazard or footer.
 * Every centre is a cell midline (16, 48, 80, …) so a 32-grid chip can share a rail.
 */
export function southPortLocals(
  count: number,
  box: { w: number; h: number },
): PortLocal[] {
  const n = Math.max(0, Math.floor(count));
  if (n === 0) {
    return [];
  }
  const pad = BOARD_HALF;
  if (n === 1) {
    return [{ x: snapToCellCenter(box.w * 0.5), y: box.h }];
  }
  const span = Math.max(BOARD_GRID, box.w - pad * 2);
  return Array.from({ length: n }, (_, i) => ({
    x: snapToCellCenter(pad + ((i + 0.5) / n) * span),
    y: box.h,
  }));
}

export function sidePortLocals(
  count: number,
  box: { w: number; h: number },
  east: boolean,
  role: "io" | "chip" = "chip",
): PortLocal[] {
  const n = Math.max(0, Math.floor(count));
  if (n === 0) {
    return [];
  }
  const x = east ? box.w : 0;
  if (n === 1) {
    return [{ x, y: snapToCellCenter(box.h * 0.5) }];
  }
  const band = sideJackBand(box.h, role);
  const pitch = sideJackPitch(n, band.innerH - 24);
  const firstMin = Math.ceil((band.top + 12 - BOARD_HALF) / BOARD_GRID) * BOARD_GRID + BOARD_HALF;
  const lastMax = Math.floor((band.top + band.innerH - 12 - BOARD_HALF) / BOARD_GRID) * BOARD_GRID + BOARD_HALF;
  const start = Math.max(firstMin, Math.min(lastMax - (n - 1) * pitch,
    snapToCellCenter(band.mid - (n - 1) * pitch / 2)));
  return Array.from({ length: n }, (_, i) => ({ x, y: start + i * pitch }));
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
