import { describe, expect, it } from "vitest";
import { keepLivePositions, mergeBoardNodes, nextSeenIds, shouldAutoArrange } from "./boardSync";

describe("board position ownership", () => {
  it("does not auto-arrange after a user drag or explicit Arrange", () => {
    const ids = ["IN", "stage1", "OUT"];
    expect(shouldAutoArrange({
      origin: "canvas",
      prevIds: ids,
      nextIds: ids,
      chipsHavePositions: true,
    })).toBe(false);
    expect(shouldAutoArrange({
      origin: "elk",
      prevIds: ids,
      nextIds: ids,
      chipsHavePositions: true,
    })).toBe(false);
    expect(keepLivePositions("canvas")).toBe(true);
    expect(keepLivePositions("elk")).toBe(true);
    expect(keepLivePositions("undo")).toBe(false);
    expect(keepLivePositions("host")).toBe(false);
  });

  it("auto-arranges first paint, a new graph, and every preset load", () => {
    expect(shouldAutoArrange({
      origin: "host",
      prevIds: [],
      nextIds: ["IN", "stage1", "OUT"],
      chipsHavePositions: true,
    })).toBe(true);
    expect(shouldAutoArrange({
      origin: "preset",
      prevIds: ["IN", "stage1", "OUT"],
      nextIds: ["IN", "drive1", "filter1", "OUT"],
      chipsHavePositions: true,
    })).toBe(true);
    expect(shouldAutoArrange({
      origin: "preset",
      prevIds: ["IN", "stage1", "filter1", "OUT"],
      nextIds: ["IN", "stage1", "filter1", "OUT"],
      chipsHavePositions: true,
    })).toBe(true);
    expect(shouldAutoArrange({
      origin: "host",
      prevIds: ["IN", "stage1", "OUT"],
      nextIds: ["IN", "stage1", "OUT"],
      chipsHavePositions: true,
    })).toBe(false);
  });

  it("keeps the live chip xy when a canvas AST echo arrives", () => {
    const previous = [
      { id: "stage1", position: { x: 400, y: 240 }, data: { label: "old" } },
    ];
    const incoming = [
      { id: "stage1", position: { x: 16, y: 16 }, data: { label: "DRIVE" } },
    ];
    const kept = mergeBoardNodes(previous, incoming, true);
    expect(kept[0]?.position).toEqual({ x: 400, y: 240 });
    expect(kept[0]?.data).toEqual({ label: "DRIVE" });
    const replaced = mergeBoardNodes(previous, incoming, false);
    expect(replaced[0]?.position).toEqual({ x: 16, y: 16 });
  });

  it("does not remember the board until arrange has landed", () => {
    const ids = ["IN", "filter1", "stage1", "OUT"];
    expect(nextSeenIds([], ids, true)).toEqual([]);
    expect(nextSeenIds([], ids, false)).toEqual(ids);
    expect(nextSeenIds(ids, ids, false)).toEqual(ids);
  });
});
