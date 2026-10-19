import { describe, expect, it } from "vitest";
import { modfxFaceModel, modfxSample, modfxWaveformPath } from "./modfxWaveform";

describe("calculated modfx waveform", () => {
  it("paints one engine LFO cycle; flanger invert flips the sign", () => {
    expect(modfxSample("phaser", { depth: "0.7" }, 0.25)).toBeCloseTo(0.85, 5);
    expect(modfxSample("flanger", { depth: "0.7", invert: "on" }, 0.25)).toBeCloseTo(0.15, 5);
    expect(modfxSample("flanger", { depth: "0.7", invert: "off" }, 0.25)).toBeCloseTo(0.85, 5);
    expect(modfxWaveformPath("phaser", { depth: "0.4" }))
      .not.toBe(modfxWaveformPath("phaser", { depth: "1" }));
    expect(modfxWaveformPath("flanger", { invert: "on" }))
      .not.toBe(modfxWaveformPath("flanger", { invert: "off" }));
  });

  it("uses ChipSpec defaults, clamps phaser depth, and averages chorus voices", () => {
    expect(modfxFaceModel("phaser1", { rate: "a" }).rate).toBe(0.4);
    expect(modfxFaceModel("phaser1", { depth: "a" }).depth).toBe(0.7);
    expect(modfxFaceModel("flanger1", {}).invert).toBe(true);
    expect(modfxFaceModel("flanger1", { invert: "f" }).invert).toBe(true);
    expect(modfxFaceModel("flanger1", { invert: "off" }).invert).toBe(false);
    expect(modfxFaceModel("phaser", { depth: "1.4" }).depth).toBe(1.4);
    expect(modfxSample("phaser", { depth: "1.4" }, 0.25)).toBeCloseTo(1, 5);
    expect(modfxFaceModel("chorus", {}).voices).toBe(3);
    expect(modfxFaceModel("chorus", { depth: "a" }).depth).toBe(0.5);
    expect(modfxWaveformPath("chorus", { voices: "2" }))
      .not.toBe(modfxWaveformPath("chorus", { voices: "4" }));
    expect(modfxWaveformPath("chorus", { depth: "0.5" }))
      .not.toBe(modfxWaveformPath("phaser", { depth: "0.5" }));
  });
});
