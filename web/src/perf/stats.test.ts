import { afterEach, describe, expect, it } from "vitest";
import {
  frameDrops,
  presentPercentiles,
  recordGpuMs,
  recordPresentMs,
  recordWorkMs,
  resetPerfStats,
  snapshotPerfStats,
  workPercentiles,
} from "./stats";
import { createGpuTimer } from "./gpuTimer";
import {
  noteDraw,
  noteProgram,
  noteResize,
  noteTexAlloc,
  readPerfCounters,
  resetPerfCounters,
  takePerfCounters,
} from "./counters";
import { setPerfEnabled } from "./marks";

afterEach(() => {
  resetPerfStats();
  resetPerfCounters();
  setPerfEnabled(false);
});

describe("perf stats", () => {
  it("reports work and present percentiles separately", () => {
    for (let i = 1; i <= 100; i += 1) {
      recordWorkMs(i * 0.1);
      recordPresentMs(16.6);
    }
    recordPresentMs(40, 16.67);
    const work = workPercentiles();
    const present = presentPercentiles();
    expect(work.n).toBe(100);
    expect(work.p50).toBeGreaterThan(4);
    expect(work.p50).toBeLessThan(6);
    expect(work.p99).toBeGreaterThan(work.p50);
    expect(present.n).toBe(101);
    expect(present.p50).toBeCloseTo(16.6, 5);
    expect(frameDrops()).toBeGreaterThanOrEqual(1);
    const snap = snapshotPerfStats();
    expect(snap.work.n).toBe(work.n);
    expect(snap.drops).toBe(frameDrops());
  });

  it("ignores non-finite samples", () => {
    recordWorkMs(Number.NaN);
    recordWorkMs(-1);
    recordGpuMs(2);
    expect(workPercentiles().n).toBe(0);
    expect(snapshotPerfStats().gpu.n).toBe(1);
  });
});

describe("perf counters", () => {
  it("stay zero when nkperf is off", () => {
    noteDraw();
    noteProgram();
    expect(readPerfCounters().draws).toBe(0);
  });

  it("count and reset when on", () => {
    setPerfEnabled(true);
    noteDraw();
    noteDraw();
    noteProgram();
    noteResize();
    noteTexAlloc();
    expect(readPerfCounters()).toEqual({ draws: 2, programs: 1, resizes: 1, texAlloc: 1 });
    expect(takePerfCounters().draws).toBe(2);
    expect(readPerfCounters().draws).toBe(0);
  });
});

describe("gpu timer", () => {
  it("returns null when the disjoint timer extension is missing", () => {
    const gl = {
      getExtension: () => null,
    } as unknown as WebGL2RenderingContext;
    expect(createGpuTimer(gl)).toBeNull();
  });
});
