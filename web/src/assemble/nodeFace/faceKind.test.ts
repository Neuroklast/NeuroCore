import { describe, expect, it } from "vitest";
import { allChipSpecs, collapsedFace, faceParamKeys } from "../chipSpec";
import { closedChipChrome } from "../chipFace";
import {
  closedFaceGlyph,
  FACE_KINDS,
  faceKindForType,
  TARGET_CLOSED_FACE,
} from "./faceKind";

describe("NodeFace catalog", () => {
  it("assigns every ChipSpec a visual family", () => {
    const missing: string[] = [];
    for (const spec of allChipSpecs()) {
      const kind = faceKindForType(spec.id);
      if (! FACE_KINDS.includes(kind)) {
        missing.push(spec.id);
      }
    }
    expect(missing, missing.join(",")).toEqual([]);
    expect(faceKindForType("filter")).toBe("filter");
    expect(faceKindForType("eq")).toBe("filter");
    expect(faceKindForType("reverb")).toBe("reverb");
    expect(faceKindForType("delay")).toBe("delay");
    expect(faceKindForType("osc")).toBe("lfo");
    expect(faceKindForType("env")).toBe("env");
    expect(faceKindForType("octaver")).toBe("pitch");
    expect(faceKindForType("comp")).toBe("dynamics");
    expect(faceKindForType("stage")).toBe("drive");
    expect(faceKindForType("bus")).toBe("routing");
    expect(faceKindForType("in")).toBe("io");
  });

  it("keeps the closed plate as title + typecode, with filter/reverb as the reference faces", () => {
    expect(collapsedFace("filter", { type: "lowpass", cutoff: "900" })).toEqual({
      title: "FILTER",
      code: "FL-LP-900",
    });
    expect(collapsedFace("reverb", { decay: "0.88", size: "0.7", mix: "0.4" })).toEqual({
      title: "REVERB",
      code: "RV-0.40",
    });
    expect(faceParamKeys(allChipSpecs()[0]!)).toEqual([]);
    expect(closedFaceGlyph("filter")).toBe("response");
    expect(closedFaceGlyph("reverb")).toBe("decay");
    expect(closedFaceGlyph("delay")).toBe("taps");
    expect(closedFaceGlyph("lfo")).toBe("waveform");
    expect(closedFaceGlyph("dynamics")).toBe("curve");
    expect(closedChipChrome("osc", "osc1").lamp).toBe("lfo");
    expect(closedChipChrome("filter", "filter1").lamp).toBe("clip");
    expect(closedChipChrome("reverb", "reverb1").lamp).toBe("clip");
  });

  it.fails("gives each DSP family a distinct closed-face glyph", () => {
    for (const kind of FACE_KINDS) {
      expect(String(closedFaceGlyph(kind)), kind).toBe(TARGET_CLOSED_FACE[kind]);
    }
  });
});
