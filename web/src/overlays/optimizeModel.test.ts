import { describe, expect, it } from "vitest";
import {
  optimizeEmptyMessage,
  optimizeScript,
  optimizeShowsApply,
} from "./optimizeModel";

describe("optimize script", () => {
  it("rewrites identities and leaves unknown formulas", () => {
    const a = optimizeScript("stage1: y = x * 1");
    expect(a.changes).toBe(1);
    expect(a.script).toContain("y = x");
    expect(optimizeShowsApply(a)).toBe(true);
    expect(optimizeEmptyMessage(a)).toBe("1 safe rewrite(s). Review and Apply.");

    const b = optimizeScript("stage1: y = tanh(x * a)");
    expect(b.changes).toBe(0);
    expect(b.script).toContain("tanh");
    expect(optimizeShowsApply(b)).toBe(false);
    expect(optimizeEmptyMessage(b)).toBe("Script already optimal");
  });

  it("never replaces hard clipping with soft replacements", () => {
    for (const expr of [
      "clamp(x,-1,1)",
      "min(1,max(-1,x))",
      "hardclip(x,1)",
      "x/(1+abs(x))",
    ]) {
      const report = optimizeScript(`stage1: y = ${expr}`);
      expect(report.changes).toBe(0);
      expect(report.script).toContain(expr);
      expect(optimizeShowsApply(report)).toBe(false);
    }
  });
});
