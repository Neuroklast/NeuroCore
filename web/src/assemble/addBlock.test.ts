import { describe, expect, it } from "vitest";
import { ADDABLE_BLOCKS, ADD_CATEGORIES, blocksInCategory, nextBlockId, scriptAfterAdd, scriptAfterRemove, scriptAfterSetArg } from "./addBlock";

describe("circuit add/remove", () => {
  it("lists addable chips and inserts a legal line before out", () => {
    expect(ADDABLE_BLOCKS.some((b) => b.type === "filter")).toBe(true);
    const next = scriptAfterAdd("stage1: y = x\nout: main = 1\n", "filter");
    expect(next).toContain("filter1: type = lowpass");
    expect(next.indexOf("filter1")).toBeLessThan(next.indexOf("out:"));
    expect(nextBlockId("stage", ["stage1"])).toBe("stage2");
  });

  it("groups Add items by category and inserts a custom formula block", () => {
    expect([...ADD_CATEGORIES]).toEqual(expect.arrayContaining(["Dynamics", "Tone", "Custom"]));
    expect(blocksInCategory("Custom").map((b) => b.type)).toEqual(["custom"]);
    const next = scriptAfterAdd("filter1: type = lowpass; cutoff = 800\n", "custom");
    expect(next).toMatch(/custom1:\s*y = x/);
    const edited = scriptAfterSetArg(next, "custom1", "in2", "0");
    expect(edited).toContain("in2 = 0");
    expect(edited).toContain("y = x");
  });

  it("drops a named block from the script", () => {
    const gone = scriptAfterRemove("stage1: y = x\nfilter1: type = lowpass; cutoff = 800\n", "filter1");
    expect(gone).not.toMatch(/filter1/);
    expect(gone).toContain("stage1");
  });
});
