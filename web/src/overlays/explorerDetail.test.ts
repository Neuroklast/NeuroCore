import { describe, expect, it } from "vitest";
import { explorerScriptBlocks } from "./explorerDetail";

describe("explorer detail blocks", () => {
  it("lists dsp and mod blocks from the preset script without inventing extras", () => {
    const script = "stage1: y = x\nfilter1: type = lowpass; cutoff = 800\nosc1: freq = 1; shape = sine\n";
    const info = explorerScriptBlocks(script);
    expect(info.dsp).toContain("Filter");
    expect(info.mod).toContain("LFO");
    expect(info.chain.join(" → ")).toMatch(/Filter/);
    expect(info.dsp).not.toContain("Reverb");
  });

  it("returns nothing when there is no script", () => {
    expect(explorerScriptBlocks("")).toEqual({ dsp: [], mod: [], chain: [] });
  });
});
