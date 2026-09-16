import { describe, expect, it } from "vitest";
import { formatBound, formatMapped, kindLabel, nk } from "./tokens";

describe("board tokens", () => {
  it("uses the Neuroklast CI red, void black and off-white text", () => {
    expect(nk.accent).toBe("#ff2222");
    expect(nk.warn).toBe("#ff6b6b");
    expect(nk.cyan).toBe("#ededed");
    expect(nk.background).toBe("#000000");
    expect(nk.surface).toBe("#111111");
    expect(nk.surfaceHigh).toBe("#1a1a1a");
    expect(nk.ink).toBe("#ededed");
    expect(nk.inkMuted).toBe("#9a9a9a");
    expect(nk.version).toBe("0.6.4-beta");
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
