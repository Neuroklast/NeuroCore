import { snapToGrid } from "./grid";
import { BOARD_MAX_SCALE, BOARD_MIN_SCALE, type BoardCamera, type BoardNode } from "./boardModel";

export function cameraMatrix(c: BoardCamera): string {
  return `matrix(${c.scale}, 0, 0, ${c.scale}, ${c.tx}, ${c.ty})`;
}

export function worldFromScreen(
  c: BoardCamera,
  sx: number,
  sy: number,
): { x: number; y: number } {
  return {
    x: (sx - c.tx) / c.scale,
    y: (sy - c.ty) / c.scale,
  };
}

export function fitCamera(
  nodes: Iterable<BoardNode>,
  view: { w: number; h: number },
): BoardCamera {
  const list = [...nodes];
  if (list.length === 0 || view.w < 8 || view.h < 8) {
    return { tx: 32, ty: 32, scale: 1 };
  }
  const pad = 48;
  const x0 = Math.min(...list.map((n) => n.x)) - pad;
  const y0 = Math.min(...list.map((n) => n.y)) - pad;
  const x1 = Math.max(...list.map((n) => n.x + n.w)) + pad;
  const y1 = Math.max(...list.map((n) => n.y + n.h)) + pad;
  const bw = Math.max(1, x1 - x0);
  const bh = Math.max(1, y1 - y0);
  const scale = Math.min(BOARD_MAX_SCALE, Math.max(BOARD_MIN_SCALE, Math.min(view.w / bw, view.h / bh)));
  return {
    scale,
    tx: snapToGrid((view.w - bw * scale) * 0.5 - x0 * scale),
    ty: snapToGrid((view.h - bh * scale) * 0.5 - y0 * scale),
  };
}
