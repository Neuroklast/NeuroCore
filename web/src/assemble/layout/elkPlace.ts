import ELK, { type ElkNode } from "elkjs/lib/elk.bundled.js";
import { BOARD_GRID, CHIP_AIR_X, snapSize, snapToGrid } from "../grid";
import { separateChips } from "./compactPack";
import type { LayoutMode, LayoutNode } from "./types";

const elk = new ELK();

export function elkOptions(mode: LayoutMode, throughEdges = 0): Record<string, string> {
  if (mode === "COMPACT") {
    return {
      "elk.algorithm": "layered",
      "elk.direction": "RIGHT",
      "elk.layered.nodePlacement.strategy": "NETWORK_SIMPLEX",
      "elk.spacing.nodeNode": "32",
      "elk.layered.spacing.nodeNodeBetweenLayers": String(CHIP_AIR_X),
      "elk.padding": "[top=32,left=32,bottom=32,right=32]",
    };
  }
  void throughEdges;
  return {
    "elk.algorithm": "layered",
    "elk.direction": "RIGHT",
    "elk.spacing.nodeNode": "64",
    "elk.layered.spacing.nodeNodeBetweenLayers": "128",
    "elk.padding": "[top=64,left=64,bottom=64,right=64]",
  };
}

export async function placeWithElk(
  nodes: LayoutNode[],
  edges: Array<{ id: string; source: string; target: string; fromJack: string; toJack: string }>,
  mode: LayoutMode,
): Promise<Record<string, { x: number; y: number; w: number; h: number }>> {
  const through = edges.length;
  const graph: ElkNode = {
    id: "root",
    layoutOptions: elkOptions(mode, through),
    children: nodes.map((n) => ({
      id: n.id,
      width: snapSize(n.w),
      height: snapSize(n.h),
      ports: [
        ...n.ins.map((p) => ({
          id: `${n.id}:in:${p.id}`,
          y: p.y,
          layoutOptions: { "org.eclipse.elk.port.side": "WEST" },
        })),
        ...n.outs.map((p) => ({
          id: `${n.id}:out:${p.id}`,
          y: p.y,
          layoutOptions: { "org.eclipse.elk.port.side": "EAST" },
        })),
      ],
    })),
    edges: edges.map((e) => ({
      id: e.id,
      sources: [`${e.source}:out:${e.fromJack}`],
      targets: [`${e.target}:in:${e.toJack}`],
    })),
  };
  const laid = await elk.layout(graph);
  const out: Record<string, { x: number; y: number; w: number; h: number }> = {};
  for (const ch of laid.children ?? []) {
    const src = nodes.find((n) => n.id === ch.id);
    out[ch.id] = {
      x: snapToGrid(ch.x ?? 0),
      y: snapToGrid(ch.y ?? 0),
      w: snapSize(ch.width ?? src?.w ?? BOARD_GRID),
      h: snapSize(ch.height ?? src?.h ?? BOARD_GRID),
    };
  }
  return separateChips(out);
}
