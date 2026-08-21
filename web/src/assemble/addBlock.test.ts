import { describe, expect, it } from "vitest";
import {
  ADDABLE_BLOCKS,
  ADD_CATEGORIES,
  blocksInCategory,
  nextBlockId,
  scriptAfterAdd,
  scriptAfterRemove,
  scriptAfterRename,
  scriptAfterSetArg,
} from "./addBlock";
import { chipSpec } from "./chipSpec";

describe("circuit add/remove", () => {
  it("lists addable chips and inserts a legal line before out", () => {
    expect(ADDABLE_BLOCKS.some((b) => b.type === "filter")).toBe(true);
    const next = scriptAfterAdd("stage1: y = x\nout: main = 1\n", "filter");
    expect(next).toContain("filter1: type = lowpass");
    expect(next).toContain("bus __park:");
    expect(next.indexOf("bus __park:")).toBeLessThan(next.indexOf("filter1:"));
    expect(next.indexOf("stage1:")).toBeLessThan(next.indexOf("bus __park:"));
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

  it("offers Split/Join Mid-Side and L/R and emits canonical msN mode", () => {
    const routing = ADDABLE_BLOCKS.filter((b) => b.category === "Routing");
    const labels = routing.map((b) => b.label);
    expect(labels).toEqual(expect.arrayContaining([
      "Split Mid/Side",
      "Join Mid/Side",
      "Split L/R",
      "Join L/R",
    ]));
    expect(labels.some((l) => l === "MS Enc" || l === "MS Dec")).toBe(false);

    const byLabel = (label: string) => routing.find((b) => b.label === label);
    expect(byLabel("Split Mid/Side")?.type).toBe("ms");
    expect(byLabel("Split Mid/Side")?.args).toMatch(/mode\s*=\s*split\b/);
    expect(byLabel("Join Mid/Side")?.args).toMatch(/mode\s*=\s*join\b/);
    expect(byLabel("Split L/R")?.args).toMatch(/mode\s*=\s*split\b/);
    expect(byLabel("Split L/R")?.args).toMatch(/family\s*=\s*lr\b/);
    expect(byLabel("Join L/R")?.args).toMatch(/mode\s*=\s*join\b/);
    expect(byLabel("Join L/R")?.args).toMatch(/family\s*=\s*lr\b/);

    const split = scriptAfterAdd("stage1: y = x\n", "ms", "mode = split");
    expect(split).toMatch(/ms1:\s*mode = split/);
    const joinLr = scriptAfterAdd(split, "ms", "mode = join; family = lr");
    expect(joinLr).toMatch(/ms2:\s*mode = join;\s*family = lr/);
  });

  it("offers Bus and Join Signal and emits bus dirt: plus mix", () => {
    const routing = ADDABLE_BLOCKS.filter((b) => b.category === "Routing");
    const byLabel = (label: string) => routing.find((b) => b.label === label);
    expect(byLabel("Bus")?.type).toBe("bus");
    expect(byLabel("Bus")?.args).toMatch(/name\s*=\s*dirt\b/);
    expect(byLabel("Join Signal")?.type).toBe("join");
    expect(byLabel("Join Signal")?.args).toMatch(/mix\s*=\s*0\.5\b/);

    const withBus = scriptAfterAdd("stage1: y = x\n", "bus", "name = dirt");
    expect(withBus).toMatch(/^bus dirt:\s*$/m);
    expect(withBus).not.toMatch(/\bbus1\s*:/);
    const withDelay = scriptAfterAdd(withBus, "delay");
    expect(withDelay.indexOf("bus dirt:")).toBeLessThan(withDelay.indexOf("delay1:"));
    const withJoin = scriptAfterAdd(withDelay, "join", "mix = 0.5");
    expect(withJoin).toMatch(/join1:\s*mix = 0\.5/);
    expect(withJoin).not.toMatch(/^\s*out\s*:/m);
  });

  it("offers Multiband Split, Send, Width, Octaver with catalog defaults", () => {
    const byLabel = (label: string) => ADDABLE_BLOCKS.find((b) => b.label === label);
    expect(byLabel("Xover")).toBeUndefined();
    expect(byLabel("Multiband Split")?.type).toBe("xover");
    expect(byLabel("Multiband Split")?.args).toMatch(/f1\s*=\s*200/);
    expect(byLabel("Multiband Split")?.args).toMatch(/f2\s*=\s*2000/);

    expect(byLabel("Send")?.type).toBe("send");
    expect(byLabel("Send")?.args).toMatch(/kanal\s*=\s*both/);
    expect(chipSpec("send").enums.kanal).toEqual(["both", "left", "right", "mid", "side", "env"]);

    expect(byLabel("Width")?.type).toBe("widen");
    expect(byLabel("Width")?.args).toMatch(/width\s*=/);
    expect(byLabel("Width")?.args).toMatch(/delay\s*=/);
    expect(byLabel("Width")?.args).toMatch(/bass\s*=/);
    expect(chipSpec("width").paramJacks).toEqual(["width", "delay", "bass"]);

    expect(byLabel("Octaver")?.type).toBe("octaver");
    expect(chipSpec("octaver").paramJacks).toEqual(["sub", "up", "mix", "tone", "thresh"]);
  });

  it("offers Cabinet IR and emits ir1: mix / gain", () => {
    const cab = ADDABLE_BLOCKS.find((b) => b.label === "Cabinet IR");
    expect(cab?.type).toBe("ir");
    expect(cab?.category).toBe("Time");
    expect(cab?.args).toMatch(/mix\s*=\s*0\.3/);
    expect(cab?.args).toMatch(/gain\s*=\s*0/);
    const next = scriptAfterAdd("stage1: y = x\n", "ir");
    expect(next).toMatch(/ir1:\s*mix = 0\.3/);
    expect(next).toContain("gain = 0");
    expect(nextBlockId("ir", ["ir1"])).toBe("ir2");
  });

  it("rewrites a custom chip id across the script on rename", () => {
    const src = "custom1: y = x * a\nfilter1: type = lowpass; cutoff = custom1\nout: main = 1\n";
    const next = scriptAfterRename(src, "custom1", "dirt");
    expect(next).toMatch(/^dirt:\s*y = x \* a/m);
    expect(next).toContain("cutoff = dirt");
    expect(next).not.toMatch(/\bcustom1\b/);
    expect(scriptAfterRename(src, "custom1", "custom1")).toBe(src);
    expect(scriptAfterRename(src, "custom1", "")).toBe(src);
  });
});
