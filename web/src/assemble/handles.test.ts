import { describe, expect, it } from "vitest";
import { applyKnobBind, bindableArgKeys, handleId, parseHandle, tokenInExpr } from "./handles";

describe("jack handles", () => {
  it("keeps mod:0 ids intact so the edge can attach", () => {
    expect(handleId("mod:0", true)).toBe("src::mod:0");
    expect(parseHandle("src::mod:0")).toEqual({ id: "mod:0", output: true });
    expect(parseHandle("dst::lfo1")).toEqual({ id: "lfo1", output: false });
    expect(parseHandle("out:out")).toEqual({ id: "out", output: true });
  });

  it("finds an osc name inside a cutoff formula", () => {
    expect(tokenInExpr("c + lfo1 * b", "lfo1")).toBe(true);
    expect(tokenInExpr("lfo10 * b", "lfo1")).toBe(false);
    expect(tokenInExpr("b", "lfo1")).toBe(false);
  });

  it("binds a knob letter onto a numeric or letter arg", () => {
    expect(bindableArgKeys({ cutoff: "b", type: "lp", y: "x" })).toEqual(["cutoff", "type"]);
    expect(bindableArgKeys({ cutoff: "c + lfo1 * b", q: "f" })).toEqual(["cutoff", "q"]);
    expect(applyKnobBind({ cutoff: "800" }, "cutoff", "e").cutoff).toBe("e");
  });
});
