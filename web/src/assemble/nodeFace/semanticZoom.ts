import { BOARD_MAX_SCALE, BOARD_MIN_SCALE } from "../boardModel";

/**
 * Information density vs camera scale. The camera matrix stays the single
 * transform; this only chooses what a chip paints. Port locals do not move.
 */
export const SEMANTIC_ZOOM = {
  min: BOARD_MIN_SCALE,
  overviewMax: 0.55,
  focusedMin: 0.85,
  max: BOARD_MAX_SCALE,
} as const;

export type SemanticZoomLevel = "overview" | "normal" | "focused";

/** What a closed chip paints. Jack locals and AABB stay put. */
export type ZoomPaint = {
  title: boolean;
  typecode: boolean;
  foot: boolean;
  portCaps: boolean;
  bindCaps: boolean;
  face: "reduced" | "full";
};

export function zoomPaint(level: SemanticZoomLevel, selected: boolean): ZoomPaint {
  if (level === "overview") {
    return {
      title: true,
      typecode: selected,
      foot: selected,
      portCaps: false,
      bindCaps: false,
      face: "reduced",
    };
  }
  return {
    title: true,
    typecode: true,
    foot: true,
    portCaps: false,
    bindCaps: true,
    face: "full",
  };
}

export function semanticZoomLevel(scale: number): SemanticZoomLevel {
  if (! Number.isFinite(scale) || scale < SEMANTIC_ZOOM.overviewMax) {
    return "overview";
  }
  if (scale >= SEMANTIC_ZOOM.focusedMin) {
    return "focused";
  }
  return "normal";
}

/** Chip AABB and jack centres are independent of zoom level. */
export function zoomMayMovePorts(_level: SemanticZoomLevel): boolean {
  return false;
}

export function zoomMayResizeChip(_level: SemanticZoomLevel): boolean {
  return false;
}

/** One board attribute. Do not subscribe chips to the camera store. */
export function applyBoardZoom(
  pane: { dataset: DOMStringMap } | null,
  scale: number,
): void {
  if (! pane) {
    return;
  }
  pane.dataset.zoom = semanticZoomLevel(scale);
}
