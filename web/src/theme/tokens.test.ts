import { describe, expect, it } from "vitest";
import { formatBound, formatMapped, kindLabel, nk } from "./tokens";

describe("board tokens", () => {
  it("uses neon red plus yellow and cyan, not a single CRT red", () => {
    expect(nk.accent).toBe("#ff003c");
    expect(nk.warn).toBe("#fcee0a");
    expect(nk.cyan).toBe("#00f0ff");
    expect(nk.background).toBe("#0a0a0c");
    expect(nk.surface).toBe("#14141c");
    expect(nk.surfaceHigh).toBe("#1c1c26");
    expect(nk.ink).toBe("#f4f1ea");
    expect(nk.inkMuted).toBe("#8a909c");
    expect(nk.version).toBe("0.5.1-alpha");
  });

  it("maps chip types like the native kindLabel", () => {
    expect(kindLabel("stage")).toBe("DRIVE");
    expect(kindLabel("custom")).toBe("CUSTOM");
    expect(kindLabel("osc1")).toBe("LFO");
    expect(kindLabel("filter")).toBe("FILTER");
    expect(kindLabel("ir2")).toBe("CAB");
    expect(kindLabel("in")).toBe("IN");
  });

  it("formats mapped knob readouts", () => {
    expect(formatMapped(6)).toBe("6.000");
    expect(formatMapped(900)).toBe("900.0");
    expect(formatMapped(1.12)).toBe("1.120");
    expect(formatBound(6)).toBe("6");
    expect(formatBound(0.05)).toBe("0.05");
  });
});
