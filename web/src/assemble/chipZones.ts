import { chipPadInset, PIN_LEAD, TITLE_H } from "./chipMetrics";

export type ChipZoneId = "title" | "westRail" | "eastRail" | "face" | "southRail";

export type ChipZone = {
  id: ChipZoneId;
  x: number;
  y: number;
  w: number;
  h: number;
};

/** Axis-aligned boxes that share an edge do not overlap. */
export function rectsOverlap(a: ChipZone, b: ChipZone): boolean {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

/**
 * Closed-chip paint zones. Title owns mute/solo. Side rails own audio
 * port labels. Face owns the NodeFace plot. South owns param captions.
 */
export function chipZones(role: "io" | "chip", box: { w: number; h: number }): ChipZone[] {
  const pad = chipPadInset(role);
  const title: ChipZone = {
    id: "title",
    x: pad.left,
    y: PIN_LEAD,
    w: Math.max(0, box.w - pad.left - pad.right),
    h: TITLE_H,
  };
  const westRail: ChipZone = {
    id: "westRail",
    x: 0,
    y: pad.top,
    w: pad.left,
    h: Math.max(0, box.h - pad.top - pad.bottom),
  };
  const eastRail: ChipZone = {
    id: "eastRail",
    x: box.w - pad.right,
    y: pad.top,
    w: pad.right,
    h: westRail.h,
  };
  const southRail: ChipZone = {
    id: "southRail",
    x: pad.left,
    y: box.h - pad.bottom,
    w: Math.max(0, box.w - pad.left - pad.right),
    h: pad.bottom,
  };
  const face: ChipZone = {
    id: "face",
    x: pad.left,
    y: pad.top,
    w: Math.max(0, box.w - pad.left - pad.right),
    h: Math.max(0, box.h - pad.top - pad.bottom),
  };
  return [title, westRail, eastRail, face, southRail];
}
