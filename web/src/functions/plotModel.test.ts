import { describe, expect, it } from "vitest";
import { buildTraces, categoryForName, evalExpr, kindForName, resolvePlotExpression } from "./plotModel";

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
});
