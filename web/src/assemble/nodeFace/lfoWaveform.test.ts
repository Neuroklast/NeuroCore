import { describe, expect, it } from "vitest";
import { lfoFaceModel, lfoWaveformPath } from "./lfoWaveform";

describe("calculated LFO waveform", () => {
  it("changes the path when shape or depth changes", () => {
    expect(lfoWaveformPath({ shape: "sine", depth: "1" }))
      .not.toBe(lfoWaveformPath({ shape: "square", depth: "1" }));
    expect(lfoWaveformPath({ shape: "saw", depth: "1" }))
      .not.toBe(lfoWaveformPath({ shape: "saw", depth: "0.2" }));
    expect(lfoFaceModel({ shape: "triangle", depth: "0.5" }).shape).toBe("triangle");
    expect(lfoFaceModel({ shape: "sine", depth: "b" }).depth).toBe(1);
  });

  it("reads sync at 120 BPM without the knobs store", () => {
    expect(lfoFaceModel({ shape: "sine", freq: "1", sync: "1/4" }).hz).toBeCloseTo(2);
    expect(lfoFaceModel({ shape: "sine", freq: "3", sync: "off" }).hz).toBeCloseTo(3);
  });
});
