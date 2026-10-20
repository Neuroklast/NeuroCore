import { BOARD_GRID, BOARD_HALF, BOARD_PAD, CHIP_AIR_X, CHIP_AIR_Y, snapSize, snapToCellCenter, snapToGrid } from "../grid";
import type { LayoutEdge, LayoutNode } from "./types";

/** Minimum neighbour air. Compact rows always use fat (×2) and wrap rather than squeeze. */
export const COMPACT_GAP = CHIP_AIR_X;

/** Fat air for every compact row. Thin 2-cell gutters land a forced-east stub on the next net. */
export function gapXForView(
  _widths: readonly number[],
  _viewW: number,
  _pad: number,
): number {
  return CHIP_AIR_X * 2;
}
/** Halo + two-cell wrap rail + halo. Wider IC plates wrap more; the rail stays a free cable. */
export const WRAP_AIR = BOARD_PAD + BOARD_GRID * 2 + BOARD_PAD;
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
  gapY: number,
  viewW: number,
  edges: LayoutEdge[],
): Record<string, { x: number; y: number; w: number; h: number }> {
  const widths = order.map((id) => snapSize(byId.get(id)?.w ?? BOARD_GRID));
  const gapX = gapXForView(widths, viewW, pad);
  // Each row owns its widths. A wide chip on a later row must not stretch IN's gutter.
  const rows: LayoutNode[][] = [];
  let row: LayoutNode[] = [];
  let right = pad;
  for (const id of order) {
    const node = byId.get(id);
    if (!node) continue;
    const start = row.length ? right + gapX : pad;
    if (row.length && start + snapSize(node.w) + pad > viewW) {
      rows.push(row);
      row = [];
    }
    right = (row.length ? right + gapX : pad) + snapSize(node.w);
    row.push(node);
  }
  if (row.length) rows.push(row);
  const out: Record<string, { x: number; y: number; w: number; h: number }> = {};
  let rowTop = originY;
  for (const [r, list] of rows.entries()) {
    const first = list[0]!;
    const firstJack = r === 0
      ? connectingLocalY(first.id, undefined, byId, edges)
      : portLocalY(first, undefined, false);
    const offsets = list.map((n, i) => firstJack - (i === 0 ? firstJack
      : connectingLocalY(n.id, list[i - 1]!.id, byId, edges)));
    const lift = -Math.min(...offsets);
    let x = pad;
    let bottom = rowTop;
    for (const [i, n] of list.entries()) {
      const y = snapToGrid(rowTop + offsets[i]! + lift);
      out[n.id] = { x: snapToGrid(x), y, w: snapSize(n.w), h: snapSize(n.h) };
      x += snapSize(n.w) + gapX;
      bottom = Math.max(bottom, y + snapSize(n.h));
    }
    rowTop = snapToGrid(bottom + gapY);
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
  // Large graphs grow in both dimensions, keeping the same continuous PCB.
  const area=nodes.reduce((sum,n)=>sum+(n.w+CHIP_AIR_X*2)*(n.h+WRAP_AIR),0);
  const aspect=Math.max(1.3,Math.min(2.2,view.w/Math.max(200,view.h-56)));
  view={...view,w:Math.max(view.w,snapToGrid(Math.sqrt(area*aspect)))};
  const gapY = WRAP_AIR;
  const pad = CHIP_AIR_Y;
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const outNode = nodes.find((n) => n.id === "OUT" || n.id === "out" || nodeRail(n) === "out");
  // Reserve the terminal and its cable gutter so OUT does not force fit-to-board
  // to shrink every chip after the body rows have already been packed.
  const firstPair = nodes.filter(n => n !== outNode).slice(0, 2);
  const pairWidth = firstPair.reduce((sum, n) => sum + snapSize(n.w), pad * 2)
    + Math.max(0, firstPair.length - 1) * CHIP_AIR_X * 2;
  const bodyViewW = Math.min(view.w, Math.max(pairWidth,
    view.w - (outNode ? snapSize(outNode.w) + CHIP_AIR_X * 2 : 0)));
  const namedRails = [...new Set(nodes.map(nodeRail).filter((r) => r !== "main" && r !== "out" && r !== "mod"))];
  const withOut = (
    packed: Record<string, { x: number; y: number; w: number; h: number }>,
  ) => {
    const body = separateChips(packed);
    if (! outNode) {
      return body;
    }
    const placed = parkOutTerminal({
      ...body,
      [outNode.id]: {
        x: pad,
        y: pad,
        w: snapSize(outNode.w),
        h: snapSize(outNode.h),
      },
    }, CHIP_AIR_X * 2);
    const incoming = edges.find(e => e.target === outNode.id && body[e.source]);
    if (incoming) {
      const source = byId.get(incoming.source)!;
      const terminal = placed[outNode.id]!;
      terminal.y = Math.max(terminal.y, snapToGrid(body[source.id]!.y
        + portLocalY(source, incoming.fromJack, true) - portLocalY(outNode, incoming.toJack, false)));
    }
    return placed;
  };
  if (namedRails.length === 0) {
    const start = nodes.find((n) => n.id === "IN")?.id ?? nodes[0]?.id ?? "";
    const order = serialIds(nodes, edges, start).filter((id) => id !== "OUT" && id !== "out");
    return withOut(placeOrder(
      order,
      byId,
      pad,
      pad,
      gapY,
      bodyViewW,
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
    const chunk = placeOrder(serialIds(members, subEdges, start), byId, y, pad, gapY, bodyViewW, subEdges);
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
