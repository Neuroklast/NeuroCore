import { BOARD_GRID, BOARD_HALF, BOARD_PAD, CHIP_AIR_X, CHIP_AIR_Y, snapSize, snapToCellCenter, snapToGrid } from "../grid";
import type { LayoutEdge, LayoutNode } from "./types";

export const COMPACT_GAP = CHIP_AIR_X;
/** Halo + rail + halo. finishHalo solidifies one cell around each chip, so CHIP_AIR_Y is not a cable. */
export const WRAP_AIR = BOARD_PAD + BOARD_GRID + BOARD_PAD;
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

/** How many modules fit in one L→R row before wrapping. At least 1. */
export function wrapFits(
  widths: readonly number[],
  viewW: number,
  gapX: number,
  pad: number,
): number {
  if (widths.length === 0) {
    return 1;
  }
  let x = pad;
  let n = 0;
  for (const w of widths) {
    const start = n === 0 ? pad : x + gapX;
    if (n > 0 && start + w + pad > viewW) {
      break;
    }
    x = start + w;
    n += 1;
  }
  return Math.max(1, n);
}

export function nodeRail(n: LayoutNode): string {
  const r = (n.rail || "").toLowerCase();
  if (n.id === "IN" || n.id === "in") {
    return "main";
  }
  if (n.id === "OUT" || n.id === "out" || r === "out") {
    return "out";
  }
  if (r === "mod") {
    return "mod";
  }
  if (r && r !== "main") {
    return r;
  }
  return "main";
}

function portLocalY(n: LayoutNode, jackId: string | undefined, east: boolean): number {
  const list = east ? n.outs : n.ins;
  const hit = jackId ? list.find((p) => p.id === jackId) : undefined;
  return snapToCellCenter(hit?.y ?? list[0]?.y ?? BOARD_HALF);
}

function connectingLocalY(
  id: string,
  prevId: string | undefined,
  byId: Map<string, LayoutNode>,
  edges: LayoutEdge[],
): number {
  const n = byId.get(id);
  if (! n) {
    return BOARD_HALF;
  }
  if (! prevId) {
    return portLocalY(n, "out", true);
  }
  const e = edges.find((ed) => ed.source === prevId && ed.target === id);
  return portLocalY(n, e?.toJack, false);
}

function placeOrder(
  order: string[],
  byId: Map<string, LayoutNode>,
  originY: number,
  pad: number,
  gapX: number,
  gapY: number,
  viewW: number,
  edges: LayoutEdge[],
): Record<string, { x: number; y: number; w: number; h: number }> {
  const widths = order.map((id) => snapSize(byId.get(id)?.w ?? BOARD_GRID));
  const colsN = wrapFits(widths, viewW, gapX, pad);
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
  let y = originY;
  for (let r = 0; r < rowH.length; r += 1) {
    rowY[r] = snapToGrid(y);
    y += (rowH[r] ?? 0) + gapY;
  }
  const out: Record<string, { x: number; y: number; w: number; h: number }> = {};
  const rows = new Map<number, typeof slots>();
  for (const s of slots) {
    const list = rows.get(s.row) ?? [];
    list.push(s);
    rows.set(s.row, list);
  }
  for (const [r, list] of rows) {
    const first = list[0]!;
    const firstNode = byId.get(first.id);
    const firstJack = r === 0
      ? connectingLocalY(first.id, undefined, byId, edges)
      : (firstNode ? portLocalY(firstNode, undefined, false) : BOARD_HALF);
    const rail = snapToCellCenter((rowY[r] ?? originY) + firstJack);
    const raw = list.map((s, i) => {
      const jack = i === 0
        ? firstJack
        : connectingLocalY(s.id, list[i - 1]!.id, byId, edges);
      return { s, y: snapToGrid(rail - jack) };
    });
    const minY = Math.min(...raw.map((p) => p.y));
    const lift = minY < pad ? snapToGrid(pad - minY) : 0;
    for (const p of raw) {
      out[p.s.id] = {
        x: colX[p.s.col] ?? pad,
        y: snapToGrid(p.y + lift),
        w: p.s.w,
        h: p.s.h,
      };
    }
  }
  return out;
}

/** OUT is the unique last terminal: east of the hull, on the last row. Never a wrap slot. */
export function parkOutTerminal(
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
  gapX = CHIP_AIR_X,
): Record<string, { x: number; y: number; w: number; h: number }> {
  const outId = placed.OUT ? "OUT" : placed.out ? "out" : "";
  if (! outId) {
    return placed;
  }
  const out = placed[outId]!;
  const others = Object.entries(placed)
    .filter(([id]) => id !== outId)
    .map(([, p]) => p);
  if (others.length === 0) {
    return placed;
  }
  const maxRight = Math.max(...others.map((p) => p.x + p.w));
  const maxY = Math.max(...others.map((p) => p.y));
  return {
    ...placed,
    [outId]: {
      ...out,
      x: snapToGrid(maxRight + gapX),
      y: snapToGrid(maxY),
    },
  };
}

export function packRows(
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: BoardView = { w: 960, h: 420 },
): Record<string, { x: number; y: number; w: number; h: number }> {
  const gapX = CHIP_AIR_X;
  const gapY = WRAP_AIR;
  const pad = CHIP_AIR_Y;
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const outNode = nodes.find((n) => n.id === "OUT" || n.id === "out" || nodeRail(n) === "out");
  const namedRails = [...new Set(nodes.map(nodeRail).filter((r) => r !== "main" && r !== "out" && r !== "mod"))];
  const withOut = (
    packed: Record<string, { x: number; y: number; w: number; h: number }>,
  ) => {
    const body = separateChips(packed);
    if (! outNode) {
      return body;
    }
    return parkOutTerminal({
      ...body,
      [outNode.id]: {
        x: pad,
        y: pad,
        w: snapSize(outNode.w),
        h: snapSize(outNode.h),
      },
    }, gapX);
  };
  if (namedRails.length === 0) {
    const start = nodes.find((n) => n.id === "IN")?.id ?? nodes[0]?.id ?? "";
    const order = serialIds(nodes, edges, start).filter((id) => id !== "OUT" && id !== "out");
    return withOut(placeOrder(
      order,
      byId,
      pad,
      pad,
      gapX,
      gapY,
      view.w,
      edges,
    ));
  }

  const groups = new Map<string, LayoutNode[]>();
  for (const n of nodes) {
    const r = nodeRail(n);
    if (r === "out") {
      continue;
    }
    const list = groups.get(r) ?? [];
    list.push(n);
    groups.set(r, list);
  }
  const railOrder = ["main", ...namedRails, ...(groups.has("mod") ? ["mod"] : [])];
  const placed: Record<string, { x: number; y: number; w: number; h: number }> = {};
  let y = pad;
  let maxRight = pad;
  for (const rail of railOrder) {
    const members = groups.get(rail);
    if (! members || members.length === 0) {
      continue;
    }
    const start = rail === "main"
      ? (members.find((n) => n.id === "IN")?.id ?? members[0]!.id)
      : (members.find((n) => n.id === rail)?.id ?? members[0]!.id);
    const subEdges = edges.filter((e) => members.some((n) => n.id === e.source) && members.some((n) => n.id === e.target));
    const chunk = placeOrder(serialIds(members, subEdges, start), byId, y, pad, gapX, gapY, view.w, subEdges);
    for (const [id, p] of Object.entries(chunk)) {
      placed[id] = p;
      maxRight = Math.max(maxRight, p.x + p.w);
      y = Math.max(y, p.y + p.h + gapY);
    }
  }
  return withOut(placed);
}

export function rowCount(
  placed: Record<string, { x: number; y: number; w: number; h: number }>,
): number {
  const ys = [...new Set(Object.values(placed).map((p) => p.y))];
  return ys.length;
}
