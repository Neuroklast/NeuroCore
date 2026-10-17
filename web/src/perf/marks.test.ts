import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { afterEach, describe, expect, it } from "vitest";
import {
  perfBegin,
  perfEnabled,
  perfEnd,
  perfFlagFromSearch,
  perfMeasures,
  setPerfEnabled,
} from "./marks";

afterEach(() => {
  setPerfEnabled(false);
  performance.clearMarks();
  performance.clearMeasures();
});

describe("perf marks", () => {
  it("emits no nk measures when off", () => {
    setPerfEnabled(false);
    perfBegin("ball");
    perfEnd("ball");
    expect(perfEnabled()).toBe(false);
    expect(perfMeasures()).toHaveLength(0);
  });

  it("emits one measure per begin/end when on", () => {
    setPerfEnabled(true);
    perfBegin("cables");
    perfEnd("cables");
    const rows = perfMeasures("cables");
    expect(rows).toHaveLength(1);
    expect(rows[0]?.name).toBe("nk:cables");
  });

  it("hot paths call begin/end and App syncs the flag", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const ball = readFileSync(path.join(here, "../viz/BallGl.tsx"), "utf8");
    const cables = readFileSync(path.join(here, "../assemble/CableCanvas.tsx"), "utf8");
    const bands = readFileSync(path.join(here, "../viz/vizSnap.ts"), "utf8");
    const app = readFileSync(path.join(here, "../app/App.tsx"), "utf8");
    expect(ball).toContain('perfBegin("ball")');
    expect(ball).toContain('perfEnd("ball")');
    expect(cables).toContain('perfBegin("cables")');
    expect(bands).toContain('perfBegin("bands")');
    expect(app).toContain("syncPerfFlag");
  });

  it("reads ?nkperf=1 over storage, and storage when the query is absent", () => {
    expect(perfFlagFromSearch("?nkperf=1")).toBe(true);
    expect(perfFlagFromSearch("?nkperf=0", { getItem: () => "1" })).toBe(false);
    expect(perfFlagFromSearch("", { getItem: () => "1" })).toBe(true);
    expect(perfFlagFromSearch("")).toBe(false);
  });
});
