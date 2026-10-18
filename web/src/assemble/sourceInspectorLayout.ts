/** One geometry source for the Circuit source inspector. Logical pixels on the 1280×860 stage. */
export const SOURCE_INSPECTOR = {
  width: 384,
  minWidth: 360,
  maxWidth: 420,
  minBoard: 520,
  bottomHeight: 220,
  bottomMin: 160,
} as const;

export type InspectorDock = "closed" | "side" | "bottom";

export function clampInspectorWidth(width: number, paneW: number): number {
  const max = Math.min(SOURCE_INSPECTOR.maxWidth, Math.max(0, paneW - SOURCE_INSPECTOR.minBoard));
  const min = Math.min(SOURCE_INSPECTOR.minWidth, max);
  const w = Number.isFinite(width) ? width : SOURCE_INSPECTOR.width;
  return Math.max(min, Math.min(max, w));
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
  width = SOURCE_INSPECTOR.width,
): { w: number; h: number } {
  if (dock === "closed") {
    return { w: 0, h: 0 };
  }
  if (dock === "bottom") {
    const h = Math.min(SOURCE_INSPECTOR.bottomHeight, Math.max(SOURCE_INSPECTOR.bottomMin, Math.round(paneH * 0.34)));
    return { w: paneW, h };
  }
  return { w: clampInspectorWidth(width, paneW), h: paneH };
}
