import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import { chipOverlay } from "../presets/irSlots";
import { explainDrive, formatParamRange, stageCards, stageHeadline, stageRole } from "./stagesModel";

const ast: AstDocument = {
  version: 1,
  leadingComments: [],
  params: [
    { alias: "a", name: "Drive", min: 0.5, max: 2.8, isNote: false, noteWholes: [], noteLabels: [] },
    { alias: "b", name: "Bright", min: 0, max: 7, isNote: false, noteWholes: [], noteLabels: [] },
  ],
  nodes: [
    {
      id: "filter1",
      type: "filter",
      busName: "main",
      args: { type: "highpass", cutoff: "45", resonance: "0.22" },
      trailingComment: "highpass",
    },
    {
      id: "stage1",
      type: "stage",
      busName: "main",
      args: { y: "tube(x, a * 0.5)" },
      trailingComment: "tube saturation",
    },
    {
      id: "osc1",
      type: "osc",
      busName: "mod",
      args: { shape: "sine", freq: "a" },
      trailingComment: "",
    },
    { id: "dirt", type: "bus", busName: "", args: {}, trailingComment: "" },
  ],
};

describe("stages overlay model", () => {
  it("explains each stage in plain language and skips bus headers", () => {
    expect(explainDrive("tube(x, a)")).toBe("Tube saturation");
    expect(stageRole("filter")).toMatch(/frequencies/i);
    expect(stageHeadline(ast.nodes[0]!)).toMatch(/High-pass/i);
    const cards = stageCards(ast);
    expect(cards.map((c) => c.id)).toEqual(["filter1", "stage1", "osc1"]);
    expect(cards[0]?.role.length).toBeGreaterThan(20);
    expect(cards[1]?.headline).toContain("tube(x, a * 0.5)");
    expect(cards[1]?.knobs).toEqual([{ id: "a", name: "Drive" }]);
    expect(cards[2]?.label).toBe("LFO");
    expect(formatParamRange(0.10000000149011612, 0.5)).toBe("[0.10 … 0.50]");
  });

  it("an IR stage is a cab slot, not a generic inspect-only card", () => {
    const ir = {
      id: "ir1",
      type: "ir",
      busName: "main",
      args: { mix: "0.3", gain: "0" },
      trailingComment: "",
    };
    expect(stageRole("ir")).toMatch(/cabinet|impulse/i);
    expect(stageHeadline(ir)).toMatch(/Cab mix/i);
    expect(chipOverlay(ir.id, ir.type).overlay).toBe("ir");
  });
});
