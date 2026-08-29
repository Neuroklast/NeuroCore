import { describe, expect, it } from "vitest";
import { motionAllows, motionCopy } from "./motionPolicy";

describe("motion policy", () => {
  it("never treats grain as a motion feature", () => {
    expect(motionAllows("boot", "full", false)).toBe(true);
    expect(motionAllows("lfoChase", "full", false)).toBe(true);
    expect(motionAllows("pipeWave", "full", false)).toBe(true);
    expect(motionAllows("chipReact", "full", false)).toBe(true);
    expect(motionAllows("crtScan", "full", false)).toBe(true);
    expect(motionAllows("techNoise", "full", false)).toBe(true);
    expect(motionAllows("techNoise", "reduced", false)).toBe(false);
    expect(motionAllows("techNoise", "off", false)).toBe(false);
    expect(motionAllows("techNoise", "full", true)).toBe(true);
  });

  it("Settings full animates even when the OS prefers reduced motion", () => {
    expect(motionAllows("crtScan", "full", true)).toBe(true);
    expect(motionAllows("bloom", "full", true)).toBe(true);
    expect(motionAllows("lfoChase", "full", true)).toBe(true);
    expect(motionAllows("crtScan", "off", false)).toBe(false);
    expect(motionAllows("crtScan", "reduced", true)).toBe(false);
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

  it("off freezes everything; reduced still yields to the OS", () => {
    expect(motionAllows("crtScan", "off", false)).toBe(false);
    expect(motionAllows("lfoChase", "off", true)).toBe(false);
    expect(motionCopy("full").toLowerCase()).not.toMatch(/grain|noise|crt/);
    expect(motionCopy("off").toLowerCase()).not.toMatch(/grain|noise|crt/);
  });
});
