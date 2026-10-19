import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { cableSourcePeak } from "./cableMotion";
import { nodeMeter } from "./nodeMeter";

describe("honest node meters", () => {
  it("a DSP chip does not inherit plugin output as its level or wave", () => {
    const meter = nodeMeter({
      id: "sat1",
      type: "sat",
      clips: { OUT: 0.9, __out__: 0.9 },
      clipsRms: { OUT: 0.4 },
      pluginOutPeak: 0.9,
      pluginOutRms: 0.4,
    });
    expect(meter.peak).toBeNull();
    expect(meter.rms).toBeNull();
    expect(meter.source).toBe("none");
    expect(meter.wave).toBe("none");
    expect(meter.waveLabel).toBe("No waveform tap");
    expect(meter.peak).not.toBe(0.9);
  });

  it("a DSP chip with no clip key has a null peak, not a silent zero", () => {
    const meter = nodeMeter({ id: "filter1", type: "filter", clips: {} });
    expect(meter.peak).toBeNull();
    expect(meter.source).toBe("none");
    const still = nodeMeter({ id: "filter1", type: "filter", clips: { filter1: 0 } });
    expect(still.peak).toBe(0);
    expect(still.source).toBe("node");
  });

  it("a native node peak is that chip; cables use the same clip", () => {
    const clips = { sat1: 0.4, OUT: 0.9 };
    const meter = nodeMeter({ id: "sat1", type: "sat", clips, clipsRms: { sat1: 0.2 } });
    expect(meter.peak).toBe(0.4);
    expect(meter.rms).toBe(0.2);
    expect(meter.source).toBe("node");
    expect(meter.sourceLabel).toBe("Node peak");
    expect(meter.wave).toBe("none");
    expect(meter.waveLabel).toBe("No waveform tap");
    expect(cableSourcePeak("sat1", "sat", clips)).toBe(0.4);
  });

  it("IN and OUT read plugin buses; send does not steal IN", () => {
    const clips = { IN: 0.5, OUT: 0.8 };
    const inn = nodeMeter({
      id: "IN",
      type: "in",
      clips,
      pluginInPeak: 0.5,
      pluginInRms: 0.3,
    });
    expect(inn.source).toBe("plugin.in");
    expect(inn.wave).toBe("plugin.in");
    expect(inn.peak).toBe(0.5);
    expect(inn.waveLabel).toBe("Plugin input");
    const out = nodeMeter({
      id: "OUT",
      type: "out",
      clips,
      pluginOutPeak: 0.8,
    });
    expect(out.source).toBe("plugin.out");
    expect(out.wave).toBe("plugin.out");
    expect(out.peak).toBe(0.8);
    const send = nodeMeter({ id: "send1", type: "send", clips });
    expect(send.peak).toBeNull();
    expect(send.waveLabel).toBe("No waveform tap");
  });
});

describe("selected Source inspector meters", () => {
  it("shows peak/rms and wave source; never paints a fake chip scope", () => {
    const inspector = readFileSync(new URL("./SourceInspector.tsx", import.meta.url), "utf8");
    expect(inspector).toContain("nodeMeter");
    expect(inspector).toContain("nk-source-meter");
    expect(inspector).toContain("data-meter-source");
    expect(inspector).toContain("waveLabel");
    expect(inspector).not.toMatch(/waveFor\(/);
    expect(inspector).not.toMatch(/<canvas/);
    expect(inspector).not.toMatch(/<svg/);
  });
});
