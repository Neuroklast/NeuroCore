import { describe, expect, it } from "vitest";
import { motionAllows, motionCopy } from "./motionPolicy";

describe("motion policy", () => {
  it("never treats grain as a motion feature", () => {
    expect(motionAllows("boot", "full", false)).toBe(true);
    expect(motionAllows("lfoChase", "full", false)).toBe(true);
    expect(motionAllows("pipeWave", "full", false)).toBe(true);
    expect(motionAllows("chipReact", "full", false)).toBe(true);
    expect(motionAllows("crtScan", "full", false)).toBe(true);
  });

  it("reduced keeps scan and chip highlight only", () => {
    expect(motionAllows("crtScan", "reduced", false)).toBe(true);
    expect(motionAllows("chipReact", "reduced", false)).toBe(true);
    expect(motionAllows("bloom", "reduced", false)).toBe(true);
    expect(motionAllows("overlay", "reduced", false)).toBe(false);
    expect(motionAllows("lfoChase", "reduced", false)).toBe(false);
    expect(motionAllows("pipeWave", "reduced", false)).toBe(false);
    expect(motionAllows("boot", "reduced", false)).toBe(false);
  });

  it("off and OS reduced freeze everything", () => {
    expect(motionAllows("crtScan", "off", false)).toBe(false);
    expect(motionAllows("lfoChase", "full", true)).toBe(false);
    expect(motionCopy("full").toLowerCase()).not.toMatch(/grain|noise|crt/);
    expect(motionCopy("off").toLowerCase()).not.toMatch(/grain|noise|crt/);
  });
});
