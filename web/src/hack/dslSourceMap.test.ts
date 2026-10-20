import { describe, expect, it } from "vitest";
import {
  boardIdForSource,
  busFoldRanges,
  canonicalNodeId,
  mapNodeRanges,
  nodeAtPosition,
  paramAtPosition,
  rangeForNode,
  rangeForParam,
  sourceForNode,
} from "./dslSourceMap";

const SCRIPT = [
  "# Airy Clean",
  "param a = Drive [0.5, 2.8]",
  "stage1: y = tube(x, a)",
  "# filter1: cutoff = 80",
  "filter1: cutoff = 1000",
  "  ; q = 0.7",
  "out: mix = 1",
].join("\n");

const MULTILINE = [
  "stage1: y = tube(",
  "  x, a)",
  "filter1: cutoff = 1000",
].join("\n");

describe("dsl source map uses the tokenizer", () => {
  it("maps id : heads and skips param, comments, and comment-only fake heads", () => {
    const ranges = mapNodeRanges(SCRIPT);
    expect(ranges.map((r) => r.nodeId)).toEqual(["stage1", "filter1", "OUT"]);
    expect(rangeForNode(SCRIPT, "IN")).toBeNull();
    expect(rangeForNode(SCRIPT, "a")).toBeNull();
    expect(nodeAtPosition(SCRIPT, 1, 1)).toBeNull();
    expect(nodeAtPosition(SCRIPT, 2, 1)).toBeNull();
    expect(nodeAtPosition(SCRIPT, 4, 3)).toBeNull();
  });

  it("extracts the source span for a node", () => {
    expect(sourceForNode(SCRIPT, "filter1")).toContain("cutoff = 1000");
    expect(sourceForNode(SCRIPT, "filter1")).toContain("; q = 0.7");
    expect(sourceForNode(SCRIPT, "IN")).toBe("");
  });

  it("finds a chip from a caret on its line and canonicalises out → OUT", () => {
    expect(nodeAtPosition(SCRIPT, 3, 1)).toBe("stage1");
    expect(nodeAtPosition(SCRIPT, 3, 20)).toBe("stage1");
    expect(nodeAtPosition(SCRIPT, 5, 1)).toBe("filter1");
    expect(nodeAtPosition(SCRIPT, 6, 5)).toBe("filter1");
    expect(nodeAtPosition(SCRIPT, 7, 1)).toBe("OUT");
    expect(rangeForNode(SCRIPT, "out")?.startLine).toBe(7);
    expect(rangeForNode(SCRIPT, "OUT")?.startLine).toBe(7);
    expect(canonicalNodeId("out")).toBe("OUT");
    expect(canonicalNodeId("in")).toBe("IN");
  });

  it("keeps a multiline formula on the opening node, not a new id", () => {
    expect(nodeAtPosition(MULTILINE, 2, 3)).toBe("stage1");
    expect(rangeForNode(MULTILINE, "stage1")?.endLine).toBe(2);
    expect(nodeAtPosition(MULTILINE, 3, 1)).toBe("filter1");
  });

  it("matches board ids for send_bus aliases without inventing IN", () => {
    expect(boardIdForSource("OUT", ["IN", "stage1", "OUT"])).toBe("OUT");
    expect(boardIdForSource("out", ["IN", "stage1", "OUT"])).toBe("OUT");
    expect(boardIdForSource("send", ["IN", "send_main", "OUT"])).toBe("send_main");
    expect(boardIdForSource("IN", ["IN", "stage1"])).toBe("IN");
    expect(boardIdForSource("missing", ["IN", "stage1"])).toBeNull();
  });

  it("maps LFO, bus, and OUT without inventing IN", () => {
    const s = ["osc1: freq = 1", "bus dirt:", "delay1: time = 250", "out: mix = 1"].join("\n");
    expect(nodeAtPosition(s, 1, 1)).toBe("osc1");
    expect(nodeAtPosition(s, 2, 5)).toBe("dirt");
    expect(nodeAtPosition(s, 4, 1)).toBe("OUT");
    expect(paramAtPosition(s, 1, 10)?.param).toBe("freq");
    expect(rangeForNode(s, "IN")).toBeNull();
  });

  it("does not prefix-guess a DSL id onto an unrelated board chip", () => {
    expect(boardIdForSource("drive", ["IN", "drive_main", "OUT"])).toBeNull();
    expect(boardIdForSource("in", ["in_left", "stage1"])).toBeNull();
  });

  it("names the param under the caret and skips comments", () => {
    expect(paramAtPosition(SCRIPT, 5, 12)).toEqual({ nodeId: "filter1", param: "cutoff" });
    expect(paramAtPosition(SCRIPT, 6, 8)).toEqual({ nodeId: "filter1", param: "q" });
    expect(paramAtPosition(SCRIPT, 1, 1)).toBeNull();
    expect(paramAtPosition(SCRIPT, 4, 3)).toBeNull();
    expect(rangeForParam(SCRIPT, "filter1", "cutoff")?.startLine).toBe(5);
    expect(rangeForParam(SCRIPT, "filter1", "q")?.startLine).toBe(6);
  });
});

describe("busFoldRanges", () => {
  const BUS_SCRIPT = [
    "param a = Drive [0.5, 2.8]",
    "bus low:",
    "  env1: type = peak; attack = 0.005",
    "  stage1: y = x * (1 + env1 * a * 0.1)",
    "bus mid:",
    "  env2: type = peak; attack = 0.003",
    "  stage2: y = x * (1 + env2 * b * 0.12)",
    "out: low = 1; mid = 1",
  ].join("\n");

  it("faltet jeden Bus-Block bis zum naechsten Bus oder OUT", () => {
    const ranges = busFoldRanges(BUS_SCRIPT);
    expect(ranges).toHaveLength(2);
    expect(ranges[0]).toEqual({
      nodeId: "bus-fold",
      startLine: 2,
      startColumn: 1,
      endLine: 4,
      endColumn: 39,
    });
    expect(ranges[1]).toEqual({
      nodeId: "bus-fold",
      startLine: 5,
      startColumn: 1,
      endLine: 7,
      endColumn: 40,
    });
  });

  it("ignoriert Kommentarzeilen, die wie Bus koennen", () => {
    const script = [
      "# bus low:",
      "bus real:",
      "  stage1: y = x",
    ].join("\n");
    const ranges = busFoldRanges(script);
    expect(ranges).toHaveLength(1);
    expect(ranges[0].startLine).toBe(2);
  });

  it("liefert [] ohne Bus", () => {
    expect(busFoldRanges("stage1: y = x\nout: mix = 1\n")).toEqual([]);
  });
});
