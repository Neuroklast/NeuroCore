import { describe, expect, it } from "vitest";
import { validateScript } from "./validateModel";

describe("validate script", () => {
  it("accepts a balanced formula and rejects empty or broken ones", () => {
    expect(validateScript("stage1: y = tanh(x * a)", []).ok).toBe(true);
    expect(validateScript("", []).ok).toBe(false);
    expect(validateScript("stage1: y = tanh(x * a", []).ok).toBe(false);
    const diag = validateScript("x", [{ line: 2, column: 1, message: "bad" }]);
    expect(diag.ok).toBe(false);
    expect(diag.issues.some((i) => i.message.includes("bad"))).toBe(true);
  });
});
