/** Native design canvas. Window resize scales this; layout does not reflow. */
export const DESIGN_W = 1280;
export const DESIGN_H = 860;
export const DESIGN_GRID = 16;

export function snapUiFitToGrid(fit: number): number {
  if (! (fit > 0) || ! Number.isFinite(fit)) {
    return 1;
  }
  return Math.max(0.5, Math.floor(fit * DESIGN_GRID) / DESIGN_GRID);
}

export function fitForWindow(width: number, height: number): number {
  const raw = Math.min(width / DESIGN_W, height / DESIGN_H);
  return snapUiFitToGrid(raw);
}

export function fitOrigin(width: number, height: number, fit: number): { x: number; y: number } {
  return {
    x: Math.max(0, (width - DESIGN_W * fit) * 0.5),
    y: Math.max(0, (height - DESIGN_H * fit) * 0.5),
  };
}

/** WebView2 often reports 0×0 until the first host resize. */
export function hasUsableViewport(width: number, height: number): boolean {
  return width >= 8 && height >= 8;
}

export function menuPos(
  clientX: number,
  clientY: number,
  el: { getBoundingClientRect: () => { left: number; top: number; width: number; height: number }; offsetWidth: number; offsetHeight: number },
  boxW = 188,
  boxH = 176,
): { left: number; top: number } {
  const r = el.getBoundingClientRect();
  const sx = r.width > 0 ? el.offsetWidth / r.width : 1;
  const sy = r.height > 0 ? el.offsetHeight / r.height : 1;
  const x = Math.min(Math.max(0, (clientX - r.left) * sx), Math.max(0, el.offsetWidth - boxW));
  const y = Math.min(Math.max(0, (clientY - r.top) * sy), Math.max(0, el.offsetHeight - boxH));
  return { left: x, top: y };
}
