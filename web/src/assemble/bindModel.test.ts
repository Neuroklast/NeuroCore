import { describe, expect, it } from "vitest";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { jackTopPx } from "./chipLayout";
import { snapToCellCenter } from "./grid";
import {
  activateKnobPatch,
  bindHit,
  bindProfile,
  commitBind,
  defaultBindKey,
  resolveBindKey,
} from "./bindModel";
import { bindCellAt, bindGhostHot, bindPadCells, bindPadCols, boundLetters } from "./BindRail";
import { applyKnobBind } from "./handles";

describe("jack align and knob bind", () => {
  it("keeps a single side jack on the RF midline of that chip", () => {
    expect(jackTopPx(0, 1, 80)).toBe(snapToCellCenter(40));
    expect(jackTopPx(0, 1, 160)).toBe(snapToCellCenter(80));
    expect(jackTopPx(0, 2, 120)).toBeLessThan(jackTopPx(1, 2, 120));
  });

  it("reads a drop target from data-bind attributes", () => {
    const row = { dataset: { bindNode: "filter1", bindKey: "cutoff" } } as unknown as HTMLElement;
    const hit = bindHit({ closest: () => row } as unknown as Element);
    expect(hit).toEqual({ node: "filter1", key: "cutoff" });
    expect(resolveBindKey({ node: "filter1", key: "" }, { cutoff: "800", q: "f" }, "filter")).toBe("cutoff");
    expect(defaultBindKey("stage")).toBe("y");
    expect(defaultBindKey("filter")).toBe("cutoff");
    expect(applyKnobBind({ cutoff: "800" }, "cutoff", "b").cutoff).toBe("b");
    expect(boundLetters("b")).toEqual(["b"]);
    expect(boundLetters("a * 0.5")).toEqual(["a"]);
    expect(boundLetters("800")).toEqual([]);
  });

  it("packs bind pads inside the chip and hits the cell under the cursor", () => {
    expect(bindGhostHot()).toEqual({ x: 0, y: 0 });
    const box = { w: 256, h: 192 };
    expect(bindPadCols(1)).toBe(1);
    expect(bindPadCols(3)).toBe(2);
    expect(bindPadCols(4)).toBe(2);
    expect(bindPadCols(5)).toBe(3);
    const cells = bindPadCells(3, box);
    expect(cells).toHaveLength(3);
    expect(new Set(cells.map((c) => Math.round(c.x))).size).toBe(2);
    expect(new Set(cells.map((c) => Math.round(c.y))).size).toBe(2);
    for (const c of cells) {
      expect(c.x).toBeGreaterThanOrEqual(0);
      expect(c.y).toBeGreaterThanOrEqual(0);
      expect(c.x + c.w).toBeLessThanOrEqual(box.w);
      expect(c.y + c.h).toBeLessThanOrEqual(box.h);
      expect(c.w).toBeGreaterThanOrEqual(26);
      expect(c.h).toBeGreaterThanOrEqual(26);
    }
    const mid = cells[0]!;
    expect(bindCellAt({ x: mid.x + mid.w * 0.5, y: mid.y + mid.h * 0.5 }, cells)).toBe(0);
    expect(bindCellAt({ x: -1, y: -1 }, cells)).toBe(-1);
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
    expect(profile.enums).toEqual(["lowpass", "highpass", "bandpass", "allpass"]);
    expect(profile.min).toBe(0);
    expect(profile.max).toBe(1);
    expect(activateKnobPatch(profile).enums).toEqual(profile.enums);
  });

  it("commitBind on an enum key snaps the knob to N options", () => {
    useHostStore.setState({
      knobs: [
        { id: "b", name: "", value: 0, active: false, min: 0, max: 1, isNote: false },
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
    commitBind("filter1", "type", "b");
    const knob = useHostStore.getState().knobs.find((k) => k.id === "b");
    expect(knob?.enums).toEqual(["lowpass", "highpass", "bandpass", "allpass"]);
    expect(knob?.enums).toHaveLength(4);
    expect(knob?.active).toBe(true);
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

    commitBind("filter1", defaultBindKey("filter"), "d");

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
