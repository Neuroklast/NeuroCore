import { describe, expect, it } from "vitest";
import {
  boardIdForSource,
  canonicalNodeId,
  mapNodeRanges,
  nodeAtPosition,
  rangeForNode,
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
});
