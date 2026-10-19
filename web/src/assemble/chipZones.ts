import { chipPadInset, FOOT_H, PARAMETER_H, PIN_LEAD, TECHNICAL_H, TITLE_H } from "./chipMetrics";

export type ChipZoneId = "title" | "technical" | "westRail" | "eastRail" | "face" | "parameters" | "foot";
export type ChipZone = { id: ChipZoneId; x: number; y: number; w: number; h: number };

/** Axis-aligned boxes that share an edge do not overlap. */
export function rectsOverlap(a: ChipZone, b: ChipZone): boolean {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

/** Paint, controls and routing consume the same exclusive bands. */
export function chipZones(role: "io" | "chip", box: { w: number; h: number }): ChipZone[] {
  const pad = chipPadInset(role);
  const parameterH = role === "io" ? 0 : PARAMETER_H;
  const w = Math.max(0, box.w - pad.left - pad.right);
  const faceH = Math.max(0, box.h - pad.top - pad.bottom);
  return [
    { id: "title", x: pad.left, y: PIN_LEAD, w, h: TITLE_H },
    { id: "technical", x: pad.left, y: PIN_LEAD + TITLE_H, w, h: TECHNICAL_H },
    { id: "westRail", x: 0, y: pad.top, w: pad.left, h: faceH },
    { id: "eastRail", x: box.w - pad.right, y: pad.top, w: pad.right, h: faceH },
    { id: "face", x: pad.left, y: pad.top, w, h: faceH },
    { id: "parameters", x: pad.left, y: box.h - PIN_LEAD - FOOT_H - parameterH, w, h: parameterH },
    { id: "foot", x: pad.left, y: box.h - PIN_LEAD - FOOT_H, w, h: FOOT_H },
  ];
}
