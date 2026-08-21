import type { Origin } from "../bridge/ast";

export function keepLivePositions(origin: Origin): boolean {
  return origin === "canvas" || origin === "elk";
}

/** User owns pan/zoom. fitView only when auto-arrange actually ran (preset / first graph). */
export function shouldFitView(_origin: Origin, autoArrange: boolean): boolean {
  return autoArrange;
}

export function shouldAutoArrange(opts: {
  origin: Origin;
  prevIds: readonly string[];
  nextIds: readonly string[];
  chipsHavePositions: boolean;
}): boolean {
  if (opts.origin === "canvas" || opts.origin === "elk") {
    return false;
  }
  if (opts.origin === "preset") {
    return true;
  }
  if (opts.prevIds.length === 0) {
    return true;
  }
  const prev = [...opts.prevIds].sort().join("\0");
  const next = [...opts.nextIds].sort().join("\0");
  if (prev === next) {
    return false;
  }
  return opts.origin === "host" || opts.origin === "bridge";
}

/** Strict Mode re-runs the paint effect. Do not remember ids while arrange is still pending. */
export function nextSeenIds(
  prev: readonly string[],
  next: readonly string[],
  autoPending: boolean,
): string[] {
  return autoPending ? [...prev] : [...next];
}

export function mergeBoardNodes<T extends { id: string; position: { x: number; y: number } }>(
  previous: readonly T[],
  incoming: readonly T[],
  keepLive: boolean,
): T[] {
  if (! keepLive) {
    return incoming.slice();
  }
  const prev = new Map(previous.map((n) => [n.id, n.position]));
  return incoming.map((n) => {
    const p = prev.get(n.id);
    return p ? { ...n, position: p } : n;
  });
}
