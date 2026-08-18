import type { AstJack } from "../bridge/ast";
import { handleId } from "./handles";

export const PROXIMITY_PX = 150;
export const PARK_BUS = "__park";

export type PointId = { id: string; x: number; y: number };

export type JackPoint = {
  nodeId: string;
  jackId: string;
  output: boolean;
  kind: string;
  x: number;
  y: number;
};

export type ProximityPair = {
  source: string;
  target: string;
  sourceHandle: string;
  targetHandle: string;
};

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

/** Nearest foreign jack inside the proximity radius (chip origin is not enough). */
export function findClosestJack(
  cursor: { x: number; y: number; nodeId: string },
  jacks: JackPoint[],
  maxDist = PROXIMITY_PX,
): (JackPoint & { distance: number }) | null {
  let best: (JackPoint & { distance: number }) | null = null;
  for (const j of jacks) {
    if (j.nodeId === cursor.nodeId || j.kind === "knob") {
      continue;
    }
    const distance = Math.hypot(j.x - cursor.x, j.y - cursor.y);
    if (distance < maxDist && (best == null || distance < best.distance)) {
      best = { ...j, distance };
    }
  }
  return best;
}

/** Output jack is always the source; input is the target. */
export function proximityPairFromJacks(a: JackPoint, b: JackPoint): ProximityPair | null {
  if (a.nodeId === b.nodeId || a.output === b.output) {
    return null;
  }
  const src = a.output ? a : b;
  const dst = a.output ? b : a;
  return {
    source: src.nodeId,
    target: dst.nodeId,
    sourceHandle: handleId(src.jackId, true),
    targetHandle: handleId(dst.jackId, false),
  };
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

export function dndNodeChrome(opts: { locked?: boolean; dragging?: boolean }): { className: string; cursor: string } {
  if (opts.locked) {
    return { className: "nk-chip-locked", cursor: "default" };
  }
  return {
    className: "nk-drag",
    cursor: opts.dragging ? "grabbing" : "grab",
  };
}

export function dndJackChrome(opts: { hot?: boolean } = {}): { className: string; cursor: string } {
  return {
    className: opts.hot ? "nk-jack-hot" : "nk-jack-shell",
    cursor: "crosshair",
  };
}

export function dndCableEndChrome(): { className: string; cursor: string } {
  return { className: "nk-cable-end", cursor: "pointer" };
}

function isOutId(id: string): boolean {
  return id === "OUT" || id === "out";
}

function isIoTerminal(id: string): boolean {
  return id === "IN" || id === "in" || isOutId(id);
}

/** Move one processing chip onto `bus __park:` (before `out:`). */
export function parkNodeInScript(script: string, nodeId: string): string {
  if (! nodeId || isIoTerminal(nodeId)) {
    return script;
  }
  const lines = script.replace(/\s+$/u, "").split("\n");
  const re = new RegExp(`^\\s*${nodeId}\\s*:`, "i");
  const idx = lines.findIndex((l) => re.test(l));
  if (idx < 0) {
    return script.endsWith("\n") ? script : `${script}\n`;
  }
  const [moved] = lines.splice(idx, 1);
  if (! moved) {
    return script;
  }
  const cleaned = lines.filter((l, i) => {
    if (! /^\s*bus\s+__park\s*:/i.test(l)) {
      return true;
    }
    const next = lines[i + 1];
    return Boolean(next && ! /^\s*bus\s+/i.test(next) && ! /^\s*out\s*:/i.test(next));
  });
  let outAt = cleaned.findIndex((l) => /^\s*out\s*:/i.test(l));
  if (outAt < 0) {
    outAt = cleaned.length;
  }
  let parkAt = cleaned.findIndex((l) => /^\s*bus\s+__park\s*:/i.test(l));
  if (parkAt < 0) {
    cleaned.splice(outAt, 0, `bus ${PARK_BUS}:`, moved);
  } else if (parkAt > outAt) {
    cleaned.splice(parkAt, 1);
    outAt = cleaned.findIndex((l) => /^\s*out\s*:/i.test(l));
    if (outAt < 0) {
      outAt = cleaned.length;
    }
    cleaned.splice(outAt, 0, `bus ${PARK_BUS}:`, moved);
  } else {
    cleaned.splice(parkAt + 1, 0, moved);
  }
  return `${cleaned.join("\n")}\n`;
}

/**
 * Delete cable A→B in the serial DSL by parking the unplugged chip.
 * OUT destination → park source; otherwise park target.
 */
export function scriptAfterDisconnect(script: string, from: string, to: string): string {
  const parkId = isOutId(to) ? from : to;
  if (isIoTerminal(parkId)) {
    return script.endsWith("\n") ? script : `${script}\n`;
  }
  return parkNodeInScript(script, parkId);
}
