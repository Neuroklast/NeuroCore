import { describe, expect, it } from "vitest";
import {
  buildTraces,
  categoryForName,
  evalExpr,
  kindForName,
  previewSample,
  resolvePlotExpression,
  showsWavePreview,
} from "./plotModel";

describe("functions plot", () => {
  it("resolves sin to a closed sine demo", () => {
    expect(resolvePlotExpression("sin")).toContain("sin");
    expect(kindForName("sin")).toBe("transfer");
    expect(categoryForName("tanh")).toBe("Drive");
    expect(categoryForName("sin")).toBe("Core");
  });

  it("plots a moving sine IN and a finite OUT", () => {
    expect(evalExpr("sin(x * 3.14159)", 0)).toBeCloseTo(0, 5);
    const { inn, out, ok } = buildTraces(resolvePlotExpression("tanh"), 0.4);
    expect(ok).toBe(true);
    expect(inn.length).toBe(160);
    expect(Math.abs(inn[0])).toBeGreaterThan(0.01);
    expect(out.every((v) => Number.isFinite(v))).toBe(true);
  });

  it("shows IN/OUT only for transfer functions, not for blocks or OTT", () => {
    expect(showsWavePreview("tanh")).toBe(true);
    expect(showsWavePreview("fold")).toBe(true);
    expect(showsWavePreview("sin")).toBe(true);
    expect(showsWavePreview("ott")).toBe(false);
    expect(showsWavePreview("widen")).toBe(false);
    expect(showsWavePreview("octaver")).toBe(false);
    expect(showsWavePreview("delay")).toBe(false);
    expect(showsWavePreview("filter")).toBe(false);
    expect(showsWavePreview("reverb")).toBe(false);
  });

  it("can drive the preview with sine, square, saw or triangle", () => {
    expect(previewSample("sine", 0)).toBeCloseTo(0, 5);
    expect(previewSample("sine", Math.PI / 2)).toBeCloseTo(1, 5);
    expect(previewSample("square", 0.2)).toBe(1);
    expect(previewSample("square", Math.PI + 0.2)).toBe(-1);
    expect(previewSample("saw", 0)).toBeCloseTo(-1, 5);
    expect(previewSample("saw", Math.PI)).toBeCloseTo(0, 5);
    expect(previewSample("triangle", 0)).toBeCloseTo(-1, 5);
    expect(previewSample("triangle", Math.PI / 2)).toBeCloseTo(0, 5);

    const sq = buildTraces("x", 0, 32, "square");
    expect(sq.inn.every((v) => v === 1 || v === -1)).toBe(true);
    const saw = buildTraces("x", 0, 8, "saw");
    expect(saw.inn[1]!).toBeLessThan(saw.inn[5]!);
  });
});
