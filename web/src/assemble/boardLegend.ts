import { boardBounds } from "./boardBounds";
import { chipSpec } from "./chipSpec";
import type { BoardNode } from "./boardModel";

/** Reserved lower silkscreen strip, outside both components and routed traces. */
export function boardLegend(nodes: BoardNode[], points: {x:number;y:number}[], preset = "") {
  const bounds = boardBounds(nodes, points);
  const counts = new Map<string, number>();
  for (const node of nodes) {
    if (node.role !== "chip") continue;
    const label = chipSpec(node.type, node.args).label.toUpperCase();
    counts.set(label, (counts.get(label) ?? 0) + 1);
  }
  return {
    x: bounds.x + 16, y: bounds.bottom + 8,
    w: Math.max(0, bounds.right - bounds.x - 32), h: 26,
    title: (preset.trim() || "UNTITLED").toUpperCase(),
    summary: [...counts].map(([label,n]) => `${label}${n > 1 ? ` ×${n}` : ""}`).join(" · "),
  };
}
