/** Original design dimensions retained for default-size visuals and legacy fitting. */
export const DESIGN_W = 1280;
export const DESIGN_H = 860;
export const DESIGN_GRID = 16;

/** Real-size, responsive editor begins at 900×600; smaller previews get a safety scale. */
export const RESPONSIVE_MIN_W = 900;
export const RESPONSIVE_MIN_H = 600;
export type ResponsiveMode = "wide" | "compact" | "tight" | "fallback";
export type ResponsiveLayout = {
  mode: ResponsiveMode;
  scale: number;
  width: number | "100%";
  height: number | "100%";
  x: number;
  y: number;
};
export function layoutForViewport(width: number, height: number): ResponsiveLayout {
  if (!hasUsableViewport(width, height)) {
    return { mode: "wide", scale: 1, width: "100%", height: "100%", x: 0, y: 0 };
  }
  if (width < RESPONSIVE_MIN_W || height < RESPONSIVE_MIN_H) {
    const scale = Math.min(width / RESPONSIVE_MIN_W, height / RESPONSIVE_MIN_H);
    const x = Math.max(0, (width - RESPONSIVE_MIN_W * scale) / 2);
    const y = Math.max(0, (height - RESPONSIVE_MIN_H * scale) / 2);
    return { mode: "fallback", scale, width: RESPONSIVE_MIN_W, height: RESPONSIVE_MIN_H, x, y };
  }
  const mode = width < 1000 || height < 690 ? "tight"
    : width < 1220 || height < 800 ? "compact" : "wide";
  return { mode, scale: 1, width: "100%", height: "100%", x: 0, y: 0 };
}


export function snapUiFitToGrid(fit: number): number {
  if (! (fit > 0) || ! Number.isFinite(fit)) {
    return 1;
  }
  return Math.max(0.5, Math.floor(fit * DESIGN_GRID) / DESIGN_GRID);
}

export function fitForWindow(width: number, height: number): number {
  if (! hasUsableViewport(width, height)) {
    return 1;
  }
  const raw = Math.min(width / DESIGN_W, height / DESIGN_H);
  if (! (raw > 0) || ! Number.isFinite(raw)) {
    return 1;
  }
  return raw;
}

export function fitOrigin(width: number, height: number, fit: number): { x: number; y: number } {
  const x = Math.max(0, (width - DESIGN_W * fit) / 2);
  const y = Math.max(0, (height - DESIGN_H * fit) / 2);
  return { x, y };
}

/** WebView2 often reports 0×0 until the first host resize. */
export function hasUsableViewport(width: number, height: number): boolean {
  return width >= 8 && height >= 8;
}

export type PaneMetrics = {
  getBoundingClientRect: () => { left: number; top: number; width: number; height: number };
  clientWidth: number;
  clientHeight: number;
};

/** Browser client pixels → CSS pixels inside an axis-aligned scaled pane. */
export function clientToPane(
  pane: PaneMetrics,
  clientX: number,
  clientY: number,
): { x: number; y: number } {
  const r = pane.getBoundingClientRect();
  const sx = r.width > 0 ? pane.clientWidth / r.width : 1;
  const sy = r.height > 0 ? pane.clientHeight / r.height : 1;
  return {
    x: (clientX - r.left) * sx,
    y: (clientY - r.top) * sy,
  };
}

export const ADD_PICKER_BOX = { w: 260, h: 280 };
export const CHIP_MENU_BOX = { w: 188, h: 240 };

export function placeOverlayPane(
  pane: { clientWidth: number; clientHeight: number },
  anchorX: number,
  anchorY: number,
  box: { w: number; h: number },
): { left: number; top: number } {
  let left = anchorX;
  let top = anchorY;
  if (left + box.w > pane.clientWidth) {
    left = Math.max(0, anchorX - box.w);
  }
  if (top + box.h > pane.clientHeight) {
    top = Math.max(0, anchorY - box.h);
  }
  left = Math.min(left, Math.max(0, pane.clientWidth - box.w));
  top = Math.min(top, Math.max(0, pane.clientHeight - box.h));
  return { left, top };
}

export function placeOverlay(
  pane: PaneMetrics,
  clientX: number,
  clientY: number,
  box: { w: number; h: number },
): { left: number; top: number } {
  const a = clientToPane(pane, clientX, clientY);
  return placeOverlayPane(pane, a.x, a.y, box);
}

export function menuPos(
  clientX: number,
  clientY: number,
  el: { getBoundingClientRect: () => { left: number; top: number; width: number; height: number }; offsetWidth: number; offsetHeight: number },
  boxW = 188,
  boxH = 176,
): { left: number; top: number } {
  const p = clientToPane(
    { getBoundingClientRect: () => el.getBoundingClientRect(), clientWidth: el.offsetWidth, clientHeight: el.offsetHeight },
    clientX,
    clientY,
  );
  const x = Math.min(Math.max(0, p.x), Math.max(0, el.offsetWidth - boxW));
  const y = Math.min(Math.max(0, p.y), Math.max(0, el.offsetHeight - boxH));
  return { left: x, top: y };
}
