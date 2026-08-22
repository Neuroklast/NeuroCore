import type { Origin } from "../bridge/ast";

/** One floor for `<ReactFlow>` and `fitView`. Different values make fitView a no-op. */
export const BOARD_MIN_ZOOM = 0.4;

export function keepLivePositions(origin: Origin): boolean {
  return origin === "canvas" || origin === "elk";
}

/** User owns pan/zoom. fitView only when auto-arrange actually ran (preset / first graph). */
export function shouldFitView(_origin: Origin, autoArrange: boolean): boolean {
  return autoArrange;
}

export function fitViewOpts(motion: string, reduced: boolean) {
  return {
    padding: 0.1,
    duration: motion === "off" || reduced ? 0 : 280,
    minZoom: BOARD_MIN_ZOOM,
    maxZoom: 1,
  };
}

/** Hidden Circuit tab reports 0×0. fitView then is a no-op. */
export function paneCanFit(width: number): boolean {
  return width > 0;
}

/** Do not fit the previous graph: wait until the laid-out ids are on the board. */
export function boardIdsMatch(live: readonly string[], expected: readonly string[]): boolean {
  if (expected.length === 0) {
    return live.length > 0;
  }
  if (live.length !== expected.length) {
    return false;
  }
  const a = [...live].sort();
  const b = [...expected].sort();
  return a.every((id, i) => id === b[i]);
}

/** Positions must commit before fitView, or the camera stays on the last graph. */
export function scheduleFitView(run: () => void): void {
  requestAnimationFrame(() => {
    requestAnimationFrame(run);
  });
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
