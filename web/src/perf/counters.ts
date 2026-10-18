import { perfEnabled } from "./marks";

export type PerfCounters = {
  draws: number;
  programs: number;
  resizes: number;
  texAlloc: number;
};

const z: PerfCounters = { draws: 0, programs: 0, resizes: 0, texAlloc: 0 };
let cur: PerfCounters = { ...z };

export function resetPerfCounters(): void {
  cur = { ...z };
}

export function noteDraw(): void {
  if (perfEnabled()) {
    cur.draws += 1;
  }
}

export function noteProgram(): void {
  if (perfEnabled()) {
    cur.programs += 1;
  }
}

export function noteResize(): void {
  if (perfEnabled()) {
    cur.resizes += 1;
  }
}

export function noteTexAlloc(): void {
  if (perfEnabled()) {
    cur.texAlloc += 1;
  }
}

export function readPerfCounters(): PerfCounters {
  return { ...cur };
}

export function takePerfCounters(): PerfCounters {
  const out = { ...cur };
  cur = { ...z };
  return out;
}
