import { describe, expect, it } from "vitest";
import type { AstEdge, AstNode } from "../bridge/ast";
import { visualAudioEdges, visualJacksFor } from "./visualEdges";

function node(id: string, type: string, args: Record<string, string> = {}): AstNode {
  return { id, type, busName: "main", args, trailingComment: "", jacks: [] };
}

describe("visualAudioEdges", () => {
  it("forks mid/side off encode instead of a fake out jack", () => {
    const nodes = [
      node("ms1", "ms", { mode: "encode" }),
      node("stage1", "stage", { channel: "mid", y: "x" }),
      node("stage2", "stage", { channel: "side", y: "x" }),
      node("ms2", "ms", { mode: "decode" }),
    ];
    const serial: AstEdge[] = [
      { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms1", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "stage1", to: "stage2", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "stage2", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
    ];
    const vis = visualAudioEdges(nodes, serial);
    expect(vis.some((e) => e.from === "ms1" && e.to === "stage1" && e.fromJack === "mid")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.to === "stage2" && e.fromJack === "side")).toBe(true);
    expect(vis.some((e) => e.from === "stage1" && e.to === "ms2" && e.toJack === "mid")).toBe(true);
    expect(vis.some((e) => e.from === "stage2" && e.to === "ms2" && e.toJack === "side")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.fromJack === "out")).toBe(false);
    const encJ = visualJacksFor(nodes[0]!, nodes);
    expect(encJ.some((j) => j.id === "mid" && j.output)).toBe(true);
    expect(encJ.some((j) => j.id === "out")).toBe(false);
  });

  it("runs an untagged chip between encode/decode on both rails", () => {
    const nodes = [
      node("ms1", "ms", { mode: "encode" }),
      node("reverb1", "reverb", { size: "0.5" }),
      node("ms2", "ms", { mode: "decode" }),
    ];
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms1", to: "reverb1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "reverb1", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms2", to: "OUT", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "mid" && e.toJack === "in")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "side" && e.toJack === "in")).toBe(true);
    expect(vis.some((e) => e.from === "reverb1" && e.to === "ms2" && e.fromJack === "out" && e.toJack === "mid")).toBe(true);
    expect(vis.some((e) => e.from === "reverb1" && e.to === "ms2" && e.fromJack === "out" && e.toJack === "side")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.fromJack === "out")).toBe(false);
    expect(vis.some((e) => e.to === "ms2" && e.toJack === "in")).toBe(false);
  });

  it("keeps mid implicit when a chip is patched onto side", () => {
    const nodes = [
      node("ms1", "ms", { mode: "encode" }),
      node("reverb1", "reverb", { size: "0.5", channel: "side" }),
      node("ms2", "ms", { mode: "decode" }),
    ];
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms1", to: "reverb1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "reverb1", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms2", to: "OUT", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "side")).toBe(true);
    expect(vis.some((e) => e.from === "reverb1" && e.to === "ms2" && e.toJack === "side")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.to === "ms2" && e.fromJack === "mid" && e.toJack === "mid")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "mid")).toBe(false);
  });

  it("still draws cables when encode or decode sits alone in the serial chain", () => {
    const loneEnc = visualAudioEdges(
      [node("ms1", "ms", { mode: "encode" }), node("stage1", "stage", { y: "x" })],
      [
        { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "ms1", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      ],
    );
    expect(loneEnc.some((e) => e.from === "ms1" && e.to === "stage1" && e.fromJack === "mid")).toBe(true);
    expect(loneEnc.some((e) => e.from === "ms1" && e.fromJack === "out")).toBe(false);

    const loneDec = visualAudioEdges(
      [node("stage1", "stage", { y: "x" }), node("ms2", "ms", { mode: "decode" })],
      [
        { from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
        { from: "stage1", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
      ],
    );
    expect(loneDec.some((e) => e.from === "stage1" && e.to === "ms2" && e.toJack === "mid")).toBe(true);
    expect(loneDec.some((e) => e.to === "ms2" && e.toJack === "in")).toBe(false);
  });

  it("draws xover mixes to OUT", () => {
    const nodes = [node("xover1", "xover", { f1: "200", f2: "2000" })];
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "xover1", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "xover1" && e.to === "OUT" && e.fromJack === "low")).toBe(true);
    expect(vis.some((e) => e.from === "xover1" && e.to === "OUT" && e.fromJack === "mid")).toBe(true);
    expect(vis.some((e) => e.from === "xover1" && e.to === "OUT" && e.fromJack === "high")).toBe(true);
  });
});
