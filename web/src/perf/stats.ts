/** Opt-in frame and work-time stats. Release builds stay idle unless nkperf is on. */

const RING = 512;

export type Percentiles = {
  n: number;
  mean: number;
  p50: number;
  p95: number;
  p99: number;
  max: number;
};

function empty(): Percentiles {
  return { n: 0, mean: 0, p50: 0, p95: 0, p99: 0, max: 0 };
}

class Ring {
  private readonly buf = new Float64Array(RING);
  private i = 0;
  private filled = 0;

  push(ms: number): void {
    if (! Number.isFinite(ms) || ms < 0) {
      return;
    }
    this.buf[this.i] = ms;
    this.i = (this.i + 1) % RING;
    if (this.filled < RING) {
      this.filled += 1;
    }
  }

  clear(): void {
    this.i = 0;
    this.filled = 0;
  }

  percentiles(): Percentiles {
    const n = this.filled;
    if (n < 1) {
      return empty();
    }
    const rows = Array.from(this.buf.subarray(0, n)).sort((a, b) => a - b);
    const at = (p: number) => rows[Math.min(n - 1, Math.max(0, Math.ceil(p * n) - 1))] ?? 0;
    let sum = 0;
    let max = 0;
    for (const v of rows) {
      sum += v;
      if (v > max) {
        max = v;
      }
    }
    return { n, mean: sum / n, p50: at(0.5), p95: at(0.95), p99: at(0.99), max };
  }
}

const work = new Ring();
const present = new Ring();
const gpu = new Ring();
let drops = 0;
let lastPresent = 0;

export function resetPerfStats(): void {
  work.clear();
  present.clear();
  gpu.clear();
  drops = 0;
  lastPresent = 0;
}

/** CPU work inside one vizClock turn (subscribers). */
export function recordWorkMs(ms: number): void {
  work.push(ms);
}

/**
 * Wall time between presented frames. A gap well above the fps budget counts as a drop.
 * This is smoothness, not work.
 */
export function recordPresentMs(ms: number, budgetMs = 16.67): void {
  present.push(ms);
  if (lastPresent > 0 && ms > budgetMs * 1.8) {
    drops += 1;
  }
  lastPresent = ms;
}

export function recordGpuMs(ms: number): void {
  gpu.push(ms);
}

export function workPercentiles(): Percentiles {
  return work.percentiles();
}

export function presentPercentiles(): Percentiles {
  return present.percentiles();
}

export function gpuPercentiles(): Percentiles {
  return gpu.percentiles();
}

export function frameDrops(): number {
  return drops;
}

export function snapshotPerfStats(): {
  work: Percentiles;
  present: Percentiles;
  gpu: Percentiles;
  drops: number;
} {
  return {
    work: work.percentiles(),
    present: present.percentiles(),
    gpu: gpu.percentiles(),
    drops,
  };
}
