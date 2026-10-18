import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import {
  clampReverbDecay,
  reverbDecayPath,
  reverbEnvelope,
  reverbFaceModel,
  reverbRt60,
} from "./reverbDecay";

const factory = readFileSync(new URL("../../../../resources/factory_presets.json", import.meta.url), "utf8");

describe("calculated reverb decay", () => {
  it("follows the engine RT60 proxy when decay or size changes", () => {
    const short = reverbFaceModel({ size: "0.45", decay: "0.3", mix: "0.4" });
    const long = reverbFaceModel({ size: "0.45", decay: "0.9", mix: "0.4" });
    expect(long.rt60).toBeGreaterThan(short.rt60);
    expect(reverbEnvelope({ size: "0.45", decay: "0.9", mix: "0.4" }, 2))
      .toBeGreaterThan(reverbEnvelope({ size: "0.45", decay: "0.3", mix: "0.4" }, 2));
    expect(reverbDecayPath({ size: "0.45", decay: "0.3", mix: "0.4" }))
      .not.toBe(reverbDecayPath({ size: "0.45", decay: "0.9", mix: "0.4" }));
    expect(reverbRt60(0.45, 0.5)).toBeCloseTo(0.4 + 0.45 * 1.2 + 0.5 * 4.5, 5);
  });

  it("clamps factory decay 2.6 the way the engine does, and does not rewrite the preset", () => {
    expect(clampReverbDecay(2.6)).toBe(0.95);
    expect(reverbFaceModel({ decay: "2.6", size: "0.45", mix: "0.4" }).decay).toBe(0.95);
    expect(reverbDecayPath({ decay: "2.6", size: "0.45", mix: "0.4" }))
      .toBe(reverbDecayPath({ decay: "0.95", size: "0.45", mix: "0.4" }));
    expect(factory).toMatch(/# Drone Low Bed[\s\S]*decay = 2\.6/);
  });

  it("binds size from FaceHost knobs and leaves mixed formulas on the ChipSpec default", () => {
    const host = { knobs: [{ id: "c", value: (0.62 - 0.4) / (0.85 - 0.4), min: 0.4, max: 0.85 }] };
    expect(reverbFaceModel({ size: "c", decay: "2.6" }).size).toBe(0.45);
    expect(reverbFaceModel({ size: "c", decay: "2.6" }, host).size).toBeCloseTo(0.62, 5);
    expect(reverbFaceModel({ size: "c", decay: "2.6" }, host).decay).toBe(0.95);
    expect(reverbDecayPath({ size: "c", decay: "2.6" }, host))
      .not.toBe(reverbDecayPath({ size: "0.45", decay: "2.6" }));
  });

  it("scales the envelope by mix, not by a fake IR", () => {
    expect(reverbEnvelope({ decay: "0.5", size: "0.45", mix: "0.8" }, 0)).toBeCloseTo(0.8, 5);
    expect(reverbEnvelope({ decay: "0.5", size: "0.45", mix: "0.2" }, 0)).toBeCloseTo(0.2, 5);
    expect(reverbDecayPath({ decay: "0.5", size: "0.45", mix: "0.2" }))
      .not.toBe(reverbDecayPath({ decay: "0.5", size: "0.45", mix: "0.8" }));
  });
});
