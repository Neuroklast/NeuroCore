import { describe, expect, it } from "vitest";
import { FACE_BPM, FACE_FS, faceBpm, faceSr, parseFaceNumber, resolveFaceScalar } from "./faceArgs";

const droneB = [{ id: "b", value: (900 - 400) / (1600 - 400), min: 400, max: 1600 }];
const droneC = [{ id: "c", value: (0.62 - 0.4) / (0.85 - 0.4), min: 0.4, max: 0.85 }];

describe("face arg host", () => {
  it("resolves bound letters, inlays, and map() from FaceHost", () => {
    expect(parseFaceNumber("b", 1000, 20, 20000, { knobs: droneB })).toBeCloseTo(900, 5);
    expect(parseFaceNumber("b[900.00]", 1000, 20, 20000)).toBeCloseTo(900, 5);
    expect(parseFaceNumber("c", 0.45, 0.05, 1, { knobs: droneC })).toBeCloseTo(0.62, 5);
    const unitB = [{ id: "b", value: (900 - 400) / (1600 - 400), min: 0, max: 1 }];
    expect(resolveFaceScalar("map(b,0,1,400,1600)", { knobs: unitB })).toBeCloseTo(900, 5);
    expect(parseFaceNumber("400", 1000, 20, 20000)).toBe(400);
  });

  it("leaves mixed formulas on the ChipSpec default and uses host bpm / sr", () => {
    expect(parseFaceNumber("b", 1000, 20, 20000)).toBe(1000);
    expect(parseFaceNumber("180 + osc1", 250, 0.1, 2000)).toBe(250);
    expect(resolveFaceScalar("softclip(x, a)")).toBeNull();
    expect(faceBpm({ bpm: 80 })).toBe(80);
    expect(faceBpm({ bpm: 0 })).toBe(FACE_BPM);
    expect(faceSr({ sr: 44100 })).toBe(44100);
    expect(faceSr({ sr: 0 })).toBe(FACE_FS);
  });
});
