import { BOARD_GRID, CHIP_AIR_X, CHIP_AIR_Y, snapSize, snapToGrid } from "../grid";
import type { LayoutEdge, LayoutNode } from "./types";

export const COMPACT_GAP = CHIP_AIR_X;
export type BoardView = { w: number; h: number };

export function serialIds(
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  start: string,
): string[] {
  const want = new Set(nodes.map((n) => n.id));
  const next = new Map<string, string[]>();
  for (const e of edges) {
    const list = next.get(e.source) ?? [];
    list.push(e.target);
    next.set(e.source, list);
  }
  const out: string[] = [];
  const seen = new Set<string>();
  const walk = (id: string) => {
    if (seen.has(id) || ! want.has(id)) {
      return;
    }
    seen.add(id);
    out.push(id);
    for (const t of next.get(id) ?? []) {
      walk(t);
    }
  };
  walk(start);
  for (const n of nodes) {
    if (! seen.has(n.id)) {
      walk(n.id);
    }
  }
  return out;
}

/** How many stacked rows a compact board uses. Short chains stay one row. */
export function compactRowTarget(count: number): number {
  if (count <= 3) {
    return 1;
  }
  if (count <= 8) {
    return 2;
  }
  return 3;
}

function pairGap(
  a: { x: number; y: number; w: number; h: number },
  b: { x: number; y: number; w: number; h: number },
): { ovX: number; ovY: number; gap: number } {
  const ovX = Math.min(a.x + a.w, b.x + b.w) - Math.max(a.x, b.x);
  const ovY = Math.min(a.y + a.h, b.y + b.h) - Math.max(a.y, b.y);
  if (ovX > 0 && ovY > 0) {
    return { ovX, ovY, gap: 0 };
  }
  if (ovX > 0) {
    return { ovX, ovY, gap: Math.max(a.y, b.y) - Math.min(a.y + a.h, b.y + b.h) };
  }
  if (ovY > 0) {
    return { ovX, ovY, gap: Math.max(a.x, b.x) - Math.min(a.x + a.w, b.x + b.w) };
  }
  return { ovX, ovY, gap: BOARD_GRID };
}

/** Push chips: two grids beside each other, one grid above/below. */
export function separateChips(
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
  airX = CHIP_AIR_X,
  airY = CHIP_AIR_Y,
): Record<string, { x: number; y: number; w: number; h: number }> {
  const out: Record<string, { x: number; y: number; w: number; h: number }> = {};
  for (const [id, p] of Object.entries(placed)) {
    out[id] = { ...p };
  }
  const ids = Object.keys(out);
  for (let pass = 0; pass < 24; pass += 1) {
    let moved = false;
    for (let i = 0; i < ids.length; i += 1) {
      for (let j = i + 1; j < ids.length; j += 1) {
        const a = out[ids[i]!]!;
        const b = out[ids[j]!]!;
        const g = pairGap(a, b);
        if (g.ovX > 0) {
          if (g.gap >= airY) {
            continue;
          }
          const top = a.y <= b.y ? a : b;
          const bot = top === a ? b : a;
          const need = snapToGrid(top.y + top.h + airY);
          if (bot.y < need) {
            bot.y = need;
            moved = true;
          }
        } else if (g.ovY > 0) {
          if (g.gap >= airX) {
            continue;
          }
          const left = a.x <= b.x ? a : b;
          const right = left === a ? b : a;
          const need = snapToGrid(left.x + left.w + airX);
          if (right.x < need) {
            right.x = need;
            moved = true;
          }
        }
      }
    }
    if (! moved) {
      break;
    }
  }
  return out;
}

export function packRows(
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: BoardView = { w: 960, h: 420 },
): Record<string, { x: number; y: number; w: number; h: number }> {
  const gapX = CHIP_AIR_X;
  const gapY = CHIP_AIR_Y;
  const pad = gapY;
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const start = nodes.find((n) => n.id === "IN")?.id ?? nodes[0]?.id ?? "";
  const order = serialIds(nodes, edges, start);
  const colsN = Math.max(1, Math.ceil(order.length / compactRowTarget(order.length)));
  const slots: Array<{ id: string; row: number; col: number; w: number; h: number }> = [];
  for (let i = 0; i < order.length; i += 1) {
    const n = byId.get(order[i]!);
    if (! n) {
      continue;
    }
    slots.push({
      id: n.id,
      row: Math.floor(i / colsN),
      col: i % colsN,
      w: snapSize(n.w),
      h: snapSize(n.h),
    });
  }
  const colW: number[] = [];
  const rowH: number[] = [];
  for (const s of slots) {
    colW[s.col] = Math.max(colW[s.col] ?? 0, s.w);
    rowH[s.row] = Math.max(rowH[s.row] ?? 0, s.h);
  }
  const colX: number[] = [];
  let x = pad;
  for (let c = 0; c < colW.length; c += 1) {
    colX[c] = snapToGrid(x);
    x += (colW[c] ?? 0) + gapX;
  }
  const rowY: number[] = [];
  let y = pad;
  for (let r = 0; r < rowH.length; r += 1) {
    rowY[r] = snapToGrid(y);
    y += (rowH[r] ?? 0) + gapY;
  }
  void view;
  const jackLocal = (id: string): number => {
    const n = byId.get(id);
    return n?.outs[0]?.y ?? n?.ins[0]?.y ?? BOARD_GRID + BOARD_GRID * 0.5;
  };
  const out: Record<string, { x: number; y: number; w: number; h: number }> = {};
  const rows = new Map<number, typeof slots>();
  for (const s of slots) {
    const list = rows.get(s.row) ?? [];
    list.push(s);
    rows.set(s.row, list);
  }
  for (const [r, list] of rows) {
    const rail = jackLocal(list[0]!.id);
    let minY = Infinity;
    const placed = list.map((s) => {
      const y = snapToGrid((rowY[r] ?? pad) + (rail - jackLocal(s.id)));
      minY = Math.min(minY, y);
      return { s, y };
    });
    const lift = minY < pad ? pad - minY : 0;
    for (const p of placed) {
      out[p.s.id] = {
        x: colX[p.s.col] ?? pad,
        y: snapToGrid(p.y + lift),
        w: p.s.w,
        h: p.s.h,
      };
    }
  }
  return separateChips(out);
}

export function rowCount(
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
): number {
  const ys = [...new Set(Object.values(placed).map((p) => p.y))];
  return ys.length;
}
