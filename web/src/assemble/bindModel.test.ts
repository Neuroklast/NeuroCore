import { describe, expect, it } from "vitest";
import { jackTopPx } from "./chipLayout";
import { bindHit, resolveBindKey } from "./bindModel";
import { applyKnobBind } from "./handles";

describe("jack align and knob bind", () => {
  it("moves jack Y when the chip height changes", () => {
    expect(jackTopPx(0, 1, 80)).toBe(40);
    expect(jackTopPx(0, 1, 160)).toBe(80);
    expect(jackTopPx(0, 2, 120)).toBeLessThan(jackTopPx(1, 2, 120));
  });

  it("reads a drop target from data-bind attributes", () => {
    const row = { dataset: { bindNode: "filter1", bindKey: "cutoff" } } as unknown as HTMLElement;
    const hit = bindHit({ closest: () => row } as unknown as Element);
    expect(hit).toEqual({ node: "filter1", key: "cutoff" });
    expect(resolveBindKey({ node: "filter1", key: "" }, { cutoff: "800", q: "f" })).toBe("cutoff");
    expect(applyKnobBind({ cutoff: "800" }, "cutoff", "b").cutoff).toBe("b");
  });
});
