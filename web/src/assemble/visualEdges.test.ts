import { describe, expect, it } from "vitest";
import type { AstEdge, AstNode } from "../bridge/ast";
import { isValidLink } from "./validateLink";
import { isMsEncode, visualAudioEdges, visualJacksFor } from "./visualEdges";

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
    expect(loneEnc.some((e) => e.from === "ms1" && e.to === "stage1" && e.fromJack === "side")).toBe(true);
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

  it("treats mode=split as encode and factory encode still draws as Split", () => {
    const enc = node("ms1", "ms", { mode: "encode" });
    const split = node("ms3", "ms", { mode: "split" });
    const join = node("ms4", "ms", { mode: "join" });
    expect(isMsEncode(enc)).toBe(true);
    expect(isMsEncode(split)).toBe(true);
    expect(isMsEncode(join)).toBe(false);
    const encJ = visualJacksFor(enc);
    expect(encJ.some((j) => j.id === "mid" && j.output)).toBe(true);
    expect(encJ.some((j) => j.id === "out")).toBe(false);
    expect(visualJacksFor(split).map((j) => j.id)).toEqual(["in", "mid", "side"]);
    expect(visualJacksFor(join).map((j) => j.id)).toEqual(["mid", "side", "out"]);
  });

  it("forks left/right off an L/R split and sits an untagged chip on both rails", () => {
    const nodes = [
      node("ms1", "ms", { mode: "split", family: "lr" }),
      node("reverb1", "reverb", { size: "0.5" }),
      node("ms2", "ms", { mode: "join", family: "lr" }),
    ];
    expect(visualJacksFor(nodes[0]!).map((j) => j.id)).toEqual(["in", "left", "right"]);
    expect(visualJacksFor(nodes[2]!).map((j) => j.id)).toEqual(["left", "right", "out"]);
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "ms1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms1", to: "reverb1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "reverb1", to: "ms2", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "ms2", to: "OUT", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "left" && e.toJack === "in")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.to === "reverb1" && e.fromJack === "right" && e.toJack === "in")).toBe(true);
    expect(vis.some((e) => e.from === "reverb1" && e.to === "ms2" && e.toJack === "left")).toBe(true);
    expect(vis.some((e) => e.from === "reverb1" && e.to === "ms2" && e.toJack === "right")).toBe(true);
    expect(vis.some((e) => e.from === "ms1" && e.fromJack === "out")).toBe(false);
    expect(vis.some((e) => e.to === "ms2" && e.toJack === "in")).toBe(false);
  });

  it("rejects Split L/R into Join MS and Split MS into Join L/R", () => {
    const audio = { kind: "audio" as const };
    expect(isValidLink({ ...audio, output: true, jack: "left" }, { ...audio, output: false, jack: "mid" })).toBe(false);
    expect(isValidLink({ ...audio, output: true, jack: "mid" }, { ...audio, output: false, jack: "left" })).toBe(false);
    expect(isValidLink({ ...audio, output: true, jack: "side" }, { ...audio, output: false, jack: "right" })).toBe(false);
    expect(isValidLink({ ...audio, output: true, jack: "mid" }, { ...audio, output: false, jack: "side" })).toBe(true);
    expect(isValidLink({ ...audio, output: true, jack: "left" }, { ...audio, output: false, jack: "right" })).toBe(true);
    expect(isValidLink({ ...audio, output: true, jack: "out" }, { ...audio, output: false, jack: "in" })).toBe(true);
  });

  it("Join Signal is inA/inB/out and mixes main with the named bus", () => {
    const nodes = [
      node("stage1", "stage", { y: "x" }),
      { ...node("dirt", "bus", { name: "dirt" }), busName: "" },
      { ...node("delay1", "delay", { time: "250" }), busName: "dirt" },
      node("join1", "join", { mix: "0.5" }),
    ];
    expect(visualJacksFor(nodes[3]!).map((j) => j.id)).toEqual(["inA", "inB", "out"]);
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "stage1", to: "join1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "IN", to: "delay1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "delay1", to: "join1", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "stage1" && e.to === "join1" && e.toJack === "inA")).toBe(true);
    expect(vis.some((e) => e.from === "delay1" && e.to === "join1" && e.toJack === "inB")).toBe(true);
    expect(vis.some((e) => e.to === "join1" && e.toJack === "in")).toBe(false);
    expect(vis.some((e) => e.from === "join1" && (e.to === "OUT" || e.to === "out"))).toBe(true);
  });

  it("wires a named bus chip so it is not an island", () => {
    const nodes = [
      node("xover1", "xover", { f1: "200", f2: "2000" }),
      { ...node("high", "bus", { name: "high" }), busName: "" },
      { ...node("comp1", "comp", { threshold: "-6" }), busName: "high" },
    ];
    const vis = visualAudioEdges(nodes, [
      { from: "IN", to: "xover1", kind: "audio", fromJack: "out", toJack: "in" },
    ]);
    expect(vis.some((e) => e.from === "xover1" && e.to === "high" && e.fromJack === "high")).toBe(true);
    expect(vis.some((e) => e.from === "high" && e.to === "comp1")).toBe(true);
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

  it("gives Send in/out plus a bottom ctrl jack, Multiband Split low/mid/high", () => {
    const send = visualJacksFor(node("send1", "send", { kanal: "both" }));
    expect(send.map((j) => j.id)).toEqual(["in", "out", "ctrl"]);
    expect(send.find((j) => j.id === "ctrl")?.output).toBe(true);
    expect(send.find((j) => j.id === "ctrl")?.kind).toBe("ctrl");

    expect(visualJacksFor(node("xover1", "xover", { f1: "200", f2: "2000" })).map((j) => j.id))
      .toEqual(["in", "low", "mid", "high"]);
    expect(visualJacksFor(node("msplit1", "msplit", { f1: "200", f2: "2000" })).map((j) => j.id))
      .toEqual(["in", "low", "mid", "high"]);
  });
});
