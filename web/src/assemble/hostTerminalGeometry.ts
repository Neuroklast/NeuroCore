import { CHIP_GAP } from "./chipLayout";
import { snapToGrid } from "./grid";

type Box = { x: number; y: number; w: number; h: number };

/** Shared placement for routing and rendering; host plates never expand their own hull. */
export function placeHostTerminals<T extends Box>(nodes: Record<string, T>): void {
  const inn = nodes.IN;
  if (!inn) return;
  const hullBottom = Math.max(...Object.entries(nodes)
    .filter(([id]) => id !== "MIDI" && id !== "SC")
    .map(([, node]) => node.y + node.h));
  let y = snapToGrid(hullBottom + CHIP_GAP);
  for (const id of ["MIDI", "SC"]) {
    const node = nodes[id];
    if (!node) continue;
    nodes[id] = { ...node, x: inn.x, y };
    y = snapToGrid(y + node.h + CHIP_GAP);
  }
}
