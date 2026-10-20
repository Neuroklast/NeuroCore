import { CHIP_AIR_X, CHIP_AIR_Y, snapSize, snapToGrid } from "../grid";
import type { LayoutEdge, LayoutNode, LayoutResult } from "./types";

/** A serial graph owns one directed rail, including its output terminal.
 * Branches, modulation, named buses and cycles belong to the general layout.
 * View width changes the camera, never the topology of a serial signal path.
 */
export function placeSerialChain(nodes: LayoutNode[], edges: LayoutEdge[]): LayoutResult["nodes"] | null {
  if (nodes.length < 2 || edges.length !== nodes.length - 1) return null;
  if (nodes.some(n => n.rail && !["main", "out"].includes(n.rail))) return null;
  const input = nodes.find(n => n.id.toUpperCase() === "IN");
  const output = nodes.find(n => n.id.toUpperCase() === "OUT");
  if (!input || !output) return null;
  const byId = new Map(nodes.map(n => [n.id, n]));
  const next = new Map<string, LayoutEdge>();
  const incoming = new Set<string>();
  for (const edge of edges) {
    if (!byId.has(edge.source) || !byId.has(edge.target) || next.has(edge.source) || incoming.has(edge.target)) return null;
    next.set(edge.source, edge);
    incoming.add(edge.target);
  }
  if (incoming.has(input.id) || next.has(output.id)) return null;
  const placed: LayoutResult["nodes"] = {};
  let node = input, x = CHIP_AIR_Y, y = CHIP_AIR_Y;
  while (!placed[node.id]) {
    placed[node.id] = { x, y, w: snapSize(node.w), h: snapSize(node.h) };
    const edge = next.get(node.id);
    if (!edge) break;
    const target = byId.get(edge.target)!;
    const sourceJack = node.outs.find(p => p.id === edge.fromJack);
    const targetJack = target.ins.find(p => p.id === edge.toJack);
    if (!sourceJack || !targetJack) return null;
    x += snapSize(node.w) + CHIP_AIR_X;
    y = snapToGrid(y + sourceJack.y - targetJack.y);
    node = target;
  }
  if (node.id !== output.id || Object.keys(placed).length !== nodes.length) return null;
  const lift = CHIP_AIR_Y - Math.min(...Object.values(placed).map(p => p.y));
  for (const p of Object.values(placed)) p.y += lift;
  return placed;
}
