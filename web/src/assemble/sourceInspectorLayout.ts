/** One geometry source for the Circuit source inspector. Logical pixels on the 1280×860 stage. */
export const SOURCE_INSPECTOR = {
  width: 384,
  minWidth: 360,
  maxWidth: 420,
  minBoard: 520,
  bottomHeight: 220,
  bottomMin: 160,
  handle: 26,
} as const;

export type InspectorDock = "closed" | "side" | "bottom";

export function clampInspectorWidth(width: number, paneW: number): number {
  const max = Math.min(SOURCE_INSPECTOR.maxWidth, Math.max(0, paneW - SOURCE_INSPECTOR.minBoard));
  const min = Math.min(SOURCE_INSPECTOR.minWidth, max);
  const w = Number.isFinite(width) ? width : SOURCE_INSPECTOR.width;
  return Math.max(min, Math.min(max, w));
}

export function clampInspectorHeight(height: number, paneH: number): number {
  const max = Math.min(SOURCE_INSPECTOR.bottomHeight, Math.max(0, paneH));
  const min = Math.min(SOURCE_INSPECTOR.bottomMin, max);
  const h = Number.isFinite(height) ? height : SOURCE_INSPECTOR.bottomHeight;
  return Math.max(min, Math.min(max, h));
}

/** Side dock: pointer X in the pane, inspector on the right. */
export function inspectorWidthAt(paneW: number, pointerXInPane: number): number {
  return clampInspectorWidth(paneW - pointerXInPane, paneW);
}

/** Bottom dock: pointer Y in the pane, inspector on the south. */
export function inspectorHeightAt(paneH: number, pointerYInPane: number): number {
  return clampInspectorHeight(paneH - pointerYInPane, paneH);
}

export function inspectorDock(open: boolean, paneW: number, paneH: number): InspectorDock {
  if (! open) {
    return "closed";
  }
  if (! (paneW >= SOURCE_INSPECTOR.minWidth + SOURCE_INSPECTOR.minBoard) || paneH < 240) {
    return "bottom";
  }
  return "side";
}

export function inspectorBox(
  dock: InspectorDock,
  paneW: number,
  paneH: number,
  width: number = SOURCE_INSPECTOR.width,
  height: number = SOURCE_INSPECTOR.bottomHeight,
): { w: number; h: number } {
  if (dock === "closed") {
    return { w: 0, h: 0 };
  }
  if (dock === "bottom") {
    return { w: paneW, h: clampInspectorHeight(height, paneH) };
  }
  return { w: clampInspectorWidth(width, paneW), h: paneH };
}
