import type { Edge, Node } from "@xyflow/react";
import { chipBox, CHIP_GAP, IO_W } from "./chipLayout";
import { isModulatorNode, type ChipData } from "./flowFromAst";
import { TUBE_CLEAR, type Pt } from "./tubePath";

const GRID = 16;
/** Min clear between chip boxes (edge to edge). */
export const ARR_CHIP_GAP = CHIP_GAP;
/** Min clear between a chip and a foreign tube centerline. */
export const ARR_TUBE_GAP = TUBE_CLEAR;
const COL_GAP = ARR_CHIP_GAP;
const ROW_GAP = Math.max(80, ARR_TUBE_GAP * 2 + 32);
const MARGIN = 16;
const IO_GAP = 64;

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
    // Also sample segment vs chip AABB by clamping the closest point on the chip.
    const cx = Math.max(chip.x, Math.min(chip.x + chip.w, (a.x + b.x) * 0.5));
    const cy = Math.max(chip.y, Math.min(chip.y + chip.h, (a.y + b.y) * 0.5));
    best = Math.min(best, distPointToSeg({ x: cx, y: cy }, a, b));
  }
  return best;
}

function snap(v: number): number {
  return Math.round(v / GRID) * GRID;
}

function railOf(n: Node<ChipData>): string {
  if (n.id === "IN" || n.id === "OUT" || n.data.type === "in" || n.data.type === "out") {
    return "";
  }
  if (isModulatorNode({ type: n.data.type, id: n.id })) {
    return "mod";
  }
  const ch = (n.data.args.channel || "").toLowerCase();
  if (ch === "mid" || ch === "m") return "mid";
  if (ch === "side" || ch === "s") return "side";
  if (ch === "left" || ch === "l") return "left";
  if (ch === "right" || ch === "r") return "right";
  const bus = (n.data.channel || "").toLowerCase();
  if (bus && bus !== "main") {
    return bus;
  }
  return "main";
}

function boxOf(n: Node<ChipData>): { w: number; h: number } {
  if (n.id === "IN" || n.id === "OUT" || n.data.type === "in" || n.data.type === "out") {
    return { w: IO_W, h: 56 };
  }
  return chipBox(n.data.type, n.data.jacks, false, n.data.args);
}

function serialOrder(ids: string[], edges: Edge[], start: string): string[] {
  const want = new Set(ids);
  const next = new Map<string, string[]>();
  for (const e of edges) {
    if (e.className === "temp") {
      continue;
    }
    const list = next.get(e.source) ?? [];
    list.push(e.target);
    next.set(e.source, list);
  }
  const out: string[] = [];
  const seen = new Set<string>();
  const walk = (id: string) => {
    if (seen.has(id)) {
      return;
    }
    seen.add(id);
    if (want.has(id)) {
      out.push(id);
    }
    for (const t of next.get(id) ?? []) {
      walk(t);
    }
  };
  walk(start);
  for (const id of ids) {
    if (! seen.has(id)) {
      walk(id);
    }
  }
  return out;
}

/** Column-major wrap: short chains stay one row; longer ones stack like the board in Chrome. */
export function arrangeCompact(
  nodes: Node<ChipData>[],
  edges: Edge[],
  view: BoardView = { w: 1200, h: 420 },
): Record<string, { x: number; y: number }> {
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const inn = nodes.find((n) => n.id === "IN" || n.data.type === "in");
  const outn = nodes.find((n) => n.id === "OUT" || n.data.type === "out");
  const chips = nodes.filter((n) => n !== inn && n !== outn);
  const pos: Record<string, { x: number; y: number }> = {};

  const rails: string[] = ["main"];
  for (const pref of ["mid", "side", "left", "right", "low", "high"]) {
    if (chips.some((n) => railOf(n) === pref) && ! rails.includes(pref)) {
      rails.push(pref);
    }
  }
  for (const n of chips) {
    const r = railOf(n);
    if (r && r !== "mod" && ! rails.includes(r)) {
      rails.push(r);
    }
  }
  if (chips.some((n) => railOf(n) === "mod")) {
    rails.push("mod");
  }

  const maxChipW = chips.reduce((m, n) => Math.max(m, boxOf(n).w), 236);
  const maxChipH = chips.reduce((m, n) => Math.max(m, boxOf(n).h), 80);
  const colPitch = snap(maxChipW + COL_GAP);
  const rowPitch = snap(maxChipH + ROW_GAP);
  const ioW = inn ? boxOf(inn).w : IO_W;
  const ioH = inn ? boxOf(inn).h : 56;

  const mainIds = chips.filter((n) => railOf(n) === "main").map((n) => n.id);
  const nMain = Math.max(mainIds.length, 1);
  const avail = Math.max(colPitch, view.w - MARGIN * 2 - ioW - IO_GAP - IO_W - IO_GAP);
  const fitCols = Math.max(1, Math.floor((avail + COL_GAP) / colPitch));
  let rows = 1;
  let cols = nMain;
  if (nMain > 3) {
    rows = nMain > 8 ? 3 : 2;
    cols = Math.ceil(nMain / rows);
  } else if (nMain > fitCols) {
    cols = fitCols;
    rows = Math.ceil(nMain / cols);
  }

  const originX = snap(MARGIN + ioW + IO_GAP);
  let bandY = MARGIN;

  const placeBand = (ids: string[], startId: string) => {
    const order = serialOrder(ids, edges, startId);
    const n = order.length;
    if (n === 0) {
      return 0;
    }
    const bandRows = ids === mainIds || startId === "IN" ? rows : Math.min(rows, Math.max(1, n > 3 ? 2 : 1));
    const bandCols = Math.max(1, Math.ceil(n / bandRows));
    for (let i = 0; i < n; i += 1) {
      const col = Math.floor(i / bandRows);
      const row = i % bandRows;
      pos[order[i]!] = {
        x: snap(originX + col * colPitch),
        y: snap(bandY + row * rowPitch),
      };
    }
    return bandCols;
  };

  placeBand(mainIds, inn?.id ?? "IN");

  const firstMain = pos[serialOrder(mainIds, edges, inn?.id ?? "IN")[0] ?? ""];
  if (inn) {
    const y = firstMain ? firstMain.y + Math.round((maxChipH - ioH) / 2) : bandY;
    pos[inn.id] = { x: snap(MARGIN), y: snap(Math.max(MARGIN, y)) };
  }

  const mainOrder = serialOrder(mainIds, edges, inn?.id ?? "IN");
  const lastMainId = mainOrder[mainOrder.length - 1];
  const lastMain = lastMainId ? pos[lastMainId] : undefined;
  const mainCols = Math.max(1, cols);
  if (outn) {
    const x = lastMain
      ? lastMain.x + (byId.get(lastMainId!) ? boxOf(byId.get(lastMainId!)!).w : maxChipW) + IO_GAP
      : originX + mainCols * colPitch;
    const y = lastMain ? lastMain.y + Math.round((maxChipH - 56) / 2) : bandY;
    pos[outn.id] = { x: snap(x), y: snap(y) };
  }

  const mainHeight = rows * rowPitch;
  bandY = snap(bandY + mainHeight + (rails.length > 1 ? ROW_GAP : 0));

  for (const rail of rails) {
    if (rail === "main") {
      continue;
    }
    const ids = chips.filter((n) => railOf(n) === rail).map((n) => n.id);
    if (ids.length === 0) {
      continue;
    }
    const start = rail === "mod" ? ids[0]! : (inn?.id ?? "IN");
    placeBand(ids, start);
    const usedRows = ids.length > 3 ? Math.min(rows, 2) : 1;
    bandY = snap(bandY + usedRows * rowPitch + ROW_GAP);
  }

  for (const n of nodes) {
    if (! pos[n.id]) {
      pos[n.id] = { x: snap(n.position.x || MARGIN), y: snap(n.position.y || bandY) };
    }
  }
  return pos;
}

export async function arrangeElk(
  nodes: Node<ChipData>[],
  edges: Edge[],
  view: BoardView = { w: 1200, h: 420 },
): Promise<Record<string, { x: number; y: number }>> {
  return arrangeCompact(nodes, edges, view);
}
