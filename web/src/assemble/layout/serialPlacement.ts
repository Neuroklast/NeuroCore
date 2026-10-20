import { CHIP_AIR_X, CHIP_AIR_Y, snapSize, snapToGrid } from "../grid";
import type { LayoutEdge, LayoutNode, LayoutResult, LayoutView } from "./types";

/** Balanced reading-order PCB rows with an explicit inter-row routing channel. */
export function placeSerialChain(
  nodes: LayoutNode[],
  edges: LayoutEdge[],
  view: LayoutView = { w: 960, h: 420 },
): LayoutResult["nodes"] | null {
  if (nodes.length < 2 || edges.length !== nodes.length - 1) return null;
  if (nodes.some((n) => n.rail && !["main", "out"].includes(n.rail)))
    return null;
  const input = nodes.find((n) => n.id.toUpperCase() === "IN");
  const output = nodes.find((n) => n.id.toUpperCase() === "OUT");
  if (!input || !output) return null;
  const byId = new Map(nodes.map((n) => [n.id, n]));
  const next = new Map<string, LayoutEdge>();
  const incoming = new Set<string>();
  for (const edge of edges) {
    if (
      !byId.has(edge.source) ||
      !byId.has(edge.target) ||
      next.has(edge.source) ||
      incoming.has(edge.target)
    )
      return null;
    next.set(edge.source, edge);
    incoming.add(edge.target);
  }
  if (incoming.has(input.id) || next.has(output.id)) return null;
  const ordered: LayoutNode[] = [];
  let node = input;
  const seen = new Set<string>();
  while (!seen.has(node.id)) {
    seen.add(node.id);
    ordered.push(node);
    const edge = next.get(node.id);
    if (!edge) break;
    const target = byId.get(edge.target)!;
    if (
      !node.outs.find((p) => p.id === edge.fromJack) ||
      !target.ins.find((p) => p.id === edge.toJack)
    )
      return null;
    node = target;
  }
  if (node.id !== output.id || ordered.length !== nodes.length) return null;
  // Compare whole-board compositions, including the routing channels. The final
  // row is right aligned so OUT terminates the lower-right corner of the PCB.
  let best: LayoutResult["nodes"] = {},
    bestScore = 0;
  const maxColumns =
    nodes.length < 4 ? nodes.length : Math.ceil(nodes.length / 2);
  for (let columns = 2; columns <= maxColumns; columns++) {
    const rows: LayoutNode[][] = [];
    for (let i = 0; i < ordered.length; i += columns)
      rows.push(ordered.slice(i, i + columns));
    const rowWidths = rows.map(
      (row) =>
        row.reduce((w, n) => w + snapSize(n.w), 0) +
        (row.length - 1) * CHIP_AIR_X,
    );
    const width = Math.max(...rowWidths);
    const placed: LayoutResult["nodes"] = {};
    let y = CHIP_AIR_Y;
    for (const [index, row] of rows.entries()) {
      let x =
        CHIP_AIR_Y +
        (index === rows.length - 1 ? width - rowWidths[index]! : 0);
      const offsets=[0];
      for(let i=1;i<row.length;i++) {
        const previous=row[i-1]!,current=row[i]!,edge=next.get(previous.id)!;
        offsets.push(offsets[i-1]!+previous.outs.find(p=>p.id===edge.fromJack)!.y-current.ins.find(p=>p.id===edge.toJack)!.y);
      }
      const lift=-Math.min(...offsets);
      let bottom = y;
      for (const [i,n] of row.entries()) {
        const ny = snapToGrid(y+offsets[i]!+lift);
        placed[n.id] = { x, y: ny, w: snapSize(n.w), h: snapSize(n.h) };
        bottom = Math.max(bottom, ny + snapSize(n.h));
        x += snapSize(n.w) + CHIP_AIR_X;
      }
      y = bottom + CHIP_AIR_X * 2;
    }
    const height = Math.max(...Object.values(placed).map((p) => p.y + p.h));
    const score = Math.min(
      view.w / (width + 96),
      Math.max(160, view.h - 56) / (height + 96),
    );
    if (score > bestScore) {
      bestScore = score;
      best = placed;
    }
  }
  return best;
}
