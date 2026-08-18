import type { AstJack } from "../bridge/ast";

export const PROXIMITY_PX = 150;

export type PointId = { id: string; x: number; y: number };

export function findClosestChip(
  dragged: PointId,
  others: PointId[],
  maxDist = PROXIMITY_PX,
): (PointId & { distance: number }) | null {
  let best: (PointId & { distance: number }) | null = null;
  for (const n of others) {
    if (n.id === dragged.id) {
      continue;
    }
    const distance = Math.hypot(n.x - dragged.x, n.y - dragged.y);
    if (distance < maxDist && (best == null || distance < best.distance)) {
      best = { ...n, distance };
    }
  }
  return best;
}

/** Left chip is always the source (horizontal L→R). */
export function directedProximity(
  a: { id: string; x: number },
  b: { id: string; x: number },
): { source: string; target: string } {
  return a.x <= b.x
    ? { source: a.id, target: b.id }
    : { source: b.id, target: a.id };
}

export function primaryJackId(
  jacks: Array<Pick<AstJack, "id" | "output" | "kind">>,
  output: boolean,
): string {
  const preferred = output ? "out" : "in";
  const hit = jacks.find((j) => j.output === output && j.kind !== "knob" && j.id === preferred);
  if (hit) {
    return hit.id;
  }
  const any = jacks.find((j) => j.output === output && j.kind !== "knob");
  return any?.id ?? preferred;
}

export type ZoomTier = "label" | "letters" | "detail";

export function zoomTier(zoom: number): ZoomTier {
  if (zoom < 0.75) {
    return "label";
  }
  if (zoom < 1.15) {
    return "letters";
  }
  return "detail";
}

export function shouldPulse(cables: string, motion: string, reduced: boolean): boolean {
  if (reduced || motion === "off") {
    return false;
  }
  return cables === "wave";
}

export function alreadyLinked(
  edges: Array<{ source: string; target: string; sourceHandle?: string | null; targetHandle?: string | null; className?: string }>,
  source: string,
  target: string,
  sourceHandle?: string | null,
  targetHandle?: string | null,
): boolean {
  return edges.some((e) => {
    if (e.className === "temp" || e.source !== source || e.target !== target) {
      return false;
    }
    if (sourceHandle && targetHandle) {
      return e.sourceHandle === sourceHandle && e.targetHandle === targetHandle;
    }
    return true;
  });
}
