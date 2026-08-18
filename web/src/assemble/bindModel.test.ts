import { describe, expect, it } from "vitest";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { jackTopPx } from "./chipLayout";
import {
  activateKnobPatch,
  bindHit,
  bindProfile,
  commitBind,
  resolveBindKey,
} from "./bindModel";
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

  it("pulls default range and unit from chipSpec for a numeric bind", () => {
    const profile = bindProfile("filter", "cutoff");
    expect(profile.min).toBe(20);
    expect(profile.max).toBe(20000);
    expect(profile.unit).toBe("Hz");
    expect(profile.enums).toBeUndefined();
    expect(profile.name.toLowerCase()).toContain("cutoff");
    const patch = activateKnobPatch(profile);
    expect(patch.active).toBe(true);
    expect(patch.min).toBe(20);
    expect(patch.max).toBe(20000);
    expect(patch.unit).toBe("Hz");
    expect(patch.value).toBeGreaterThan(0);
    expect(patch.value).toBeLessThan(1);
  });

  it("enum bind profiles expose N options for detents", () => {
    const profile = bindProfile("filter", "type");
    expect(profile.enums).toEqual(["lowpass", "highpass", "bandpass"]);
    expect(profile.min).toBe(0);
    expect(profile.max).toBe(1);
    expect(activateKnobPatch(profile).enums).toEqual(profile.enums);
  });

  it("commitBind activates an inactive knob and writes the letter", () => {
    useHostStore.setState({
      knobs: [
        { id: "d", name: "", value: 0, active: false, min: 0, max: 1, isNote: false },
        { id: "a", name: "Rate", value: 1, active: true, min: 0.05, max: 6, isNote: false },
      ],
    });
    useAstStore.setState({
      origin: "canvas",
      ast: {
        version: 1,
        leadingComments: [],
        params: [],
        nodes: [{
          id: "filter1",
          type: "filter",
          busName: "main",
          args: { type: "lowpass", cutoff: "1000", resonance: "0.4", channel: "both" },
          trailingComment: "",
        }],
      },
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
    });

    commitBind("filter1", "cutoff", "d");

    const knob = useHostStore.getState().knobs.find((k) => k.id === "d");
    expect(knob?.active).toBe(true);
    expect(knob?.min).toBe(20);
    expect(knob?.max).toBe(20000);
    expect(knob?.unit).toBe("Hz");
    expect(knob?.name.toLowerCase()).toContain("cutoff");
    const node = useAstStore.getState().ast?.nodes.find((n) => n.id === "filter1");
    expect(node?.args.cutoff).toBe("d");
  });
});
