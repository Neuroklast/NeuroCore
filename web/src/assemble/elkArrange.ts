import type { Edge, Node } from "./flowTypes";
import { BOARD_PAD } from "./grid";
import type { ChipData } from "./flowFromAst";
import { requestLayout } from "./layout/layoutClient";
import type { Pt } from "./tubePath";

/** ELK arrange vertical gap (nodeNode). */
export const ARR_CHIP_GAP = 64;
/** ELK arrange horizontal gap between layers. */
export const ARR_LAYER_GAP = 128;
/** Min clear between a chip and a foreign tube centerline. */
export const ARR_TUBE_GAP = BOARD_PAD;

export type BoardView = { w: number; h: number };
export type ChipRect = { id?: string; x: number; y: number; w: number; h: number };

/** Edge-to-edge gap; 0 if the boxes overlap or touch. */
export function chipChipGap(a: ChipRect, b: ChipRect): number {
  const ox = Math.min(a.x + a.w, b.x + b.w) - Math.max(a.x, b.x);
  const oy = Math.min(a.y + a.h, b.y + b.h) - Math.max(a.y, b.y);
  if (ox > 0 && oy > 0) {
    return 0;
  }
  if (ox > 0) {
    return Math.max(a.y, b.y) - Math.min(a.y + a.h, b.y + b.h);
  }
  if (oy > 0) {
    return Math.max(a.x, b.x) - Math.min(a.x + a.w, b.x + b.w);
  }
  const dx = Math.max(a.x, b.x) - Math.min(a.x + a.w, b.x + b.w);
  const dy = Math.max(a.y, b.y) - Math.min(a.y + a.h, b.y + b.h);
  return Math.hypot(Math.max(0, dx), Math.max(0, dy));
}

function distPointToSeg(p: Pt, a: Pt, b: Pt): number {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const len2 = dx * dx + dy * dy;
  if (len2 < 1e-9) {
    return Math.hypot(p.x - a.x, p.y - a.y);
  }
  const t = Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
  return Math.hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

/** Shortest distance from chip outline to a polyline (tube centerline). */
export function minGapToPolyline(chip: ChipRect, pts: Pt[]): number {
  if (pts.length < 2) {
    return Infinity;
  }
  const corners: Pt[] = [
    { x: chip.x, y: chip.y },
    { x: chip.x + chip.w, y: chip.y },
    { x: chip.x + chip.w, y: chip.y + chip.h },
    { x: chip.x, y: chip.y + chip.h },
  ];
  const mids: Pt[] = [
    { x: chip.x + chip.w * 0.5, y: chip.y },
    { x: chip.x + chip.w, y: chip.y + chip.h * 0.5 },
    { x: chip.x + chip.w * 0.5, y: chip.y + chip.h },
    { x: chip.x, y: chip.y + chip.h * 0.5 },
  ];
  let best = Infinity;
  for (let i = 1; i < pts.length; i += 1) {
    const a = pts[i - 1]!;
    const b = pts[i]!;
    for (const p of [...corners, ...mids]) {
      best = Math.min(best, distPointToSeg(p, a, b));
    }
    const cx = Math.max(chip.x, Math.min(chip.x + chip.w, (a.x + b.x) * 0.5));
    const cy = Math.max(chip.y, Math.min(chip.y + chip.h, (a.y + b.y) * 0.5));
    best = Math.min(best, distPointToSeg({ x: cx, y: cy }, a, b));
  }
  return best;
}

export async function arrangeElk(
  nodes: Node<ChipData>[],
  edges: Edge[],
  _view: BoardView = { w: 1200, h: 420 },
): Promise<Record<string, { x: number; y: number }>> {
  const laid = await requestLayout("ARRANGE", nodes, edges);
  const pos: Record<string, { x: number; y: number }> = {};
  for (const [id, p] of Object.entries(laid.nodes)) {
    pos[id] = { x: p.x, y: p.y };
  }
  return pos;
}
