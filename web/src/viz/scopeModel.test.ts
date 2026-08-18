import { describe, expect, it } from "vitest";
import {
  deltaSamples,
  fieldTitle,
  gonioPoint,
  nextProcessMode,
  processModeIndex,
  sampleAtPx,
  SCOPE_COLOR,
  SCOPE_MENU,
  barFillPercent,
  demoLoudness,
  scopeTitle,
  tracesFor,
} from "./scopeModel";

describe("scope deck model", () => {
  it("builds IN, OUT, BOTH and delta traces in distinct colours", () => {
    const inn = [0.2, 0.4];
    const out = [0.5, 0.1];
    expect(tracesFor("in", false, inn, out, 2).map((t) => t.id)).toEqual(["in"]);
    expect(tracesFor("out", false, inn, out, 2).map((t) => t.id)).toEqual(["out"]);
    const both = tracesFor("both", true, inn, out, 2);
    expect(both.map((t) => t.id)).toEqual(["in", "out", "delta"]);
    expect(both[0]?.color).toBe(SCOPE_COLOR.in);
    expect(both[1]?.color).toBe(SCOPE_COLOR.out);
    expect(both[2]?.color).toBe(SCOPE_COLOR.delta);
    expect(deltaSamples(out, inn, 2)[0]).toBeCloseTo(0.3);
    expect(sampleAtPx([0, 1], 2, 0, 100)).toBeCloseTo(0);
    expect(sampleAtPx([0, 1], 2, 99, 100)).toBeCloseTo(1);
    expect(sampleAtPx([0, 1], 2, 49.5, 100)).toBeCloseTo(0.5);
  });

  it("keeps native titles and the old context-menu surface", () => {
    expect(scopeTitle("in", false)).toBe("IN // PRE");
    expect(scopeTitle("both", true)).toContain("Δ");
    expect(fieldTitle("out")).toBe("OUT FIELD");
    expect(SCOPE_MENU.x.map((x) => x.id)).toEqual(["samples", "time", "freq"]);
    expect(SCOPE_MENU.y.map((y) => y.id)).toEqual(["linear", "db"]);
    expect(SCOPE_MENU.flags.map((f) => f.id)).toEqual(["grid", "invertY", "delta"]);
    expect(SCOPE_MENU.source.map((s) => s.id)).toEqual(["in", "out", "both"]);
  });

  it("maps L/R to a mid/side goniometer, not raw x/y", () => {
    const mono = gonioPoint(0.8, 0.8);
    expect(mono.x).toBeCloseTo(0);
    expect(mono.y).toBeCloseTo(-0.8);
    const side = gonioPoint(0.6, -0.6);
    expect(side.x).toBeCloseTo(0.6);
    expect(side.y).toBeCloseTo(0);
  });

  it("LU bar height follows live rms, not a parked constant", () => {
    expect(barFillPercent(0.6)).toBeGreaterThan(barFillPercent(0.05));
    expect(barFillPercent(0)).toBe(2);
    const a = demoLoudness(0);
    const b = demoLoudness(30);
    expect(Math.abs(a.inPeak - b.inPeak) + Math.abs(a.outRms - b.outRms)).toBeGreaterThan(0.05);
  });

  it("STUDIO chip cycles LIVE, not Settings", () => {
    expect(nextProcessMode("STUDIO")).toBe("LIVE");
    expect(nextProcessMode("LIVE")).toBe("STUDIO");
    expect(nextProcessMode("SAFE")).toBe("LIVE");
    expect(processModeIndex("LIVE")).toBe(1);
    expect(processModeIndex("STUDIO")).toBe(0);
  });
});
