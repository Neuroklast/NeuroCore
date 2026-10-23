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
    expect(faceKindForType("widen")).toBe("routing");
    expect(faceKindForType("phaser")).toBe("modfx");
    expect(faceKindForType("ir")).toBe("cab");
    expect(faceKindForType("in")).toBe("io");
    expect(faceKindForType("utility")).toBe("utility");
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
    const first = allChipSpecs()[0]!;
    expect(faceParamKeys(first).every((k) => first.paramJacks.includes(k))).toBe(true);
    expect(closedFaceGlyph("filter")).toBe("response");
    expect(closedFaceGlyph("reverb")).toBe("decay");
    expect(closedFaceGlyph("delay")).toBe("taps");
    expect(closedFaceGlyph("lfo")).toBe("waveform");
    expect(closedFaceGlyph("dynamics")).toBe("curve");
    expect(closedFaceGlyph("env")).toBe("envelope");
    expect(closedFaceGlyph("pitch")).toBe("intervals");
    expect(closedFaceGlyph("drive")).toBe("transfer");
    expect(closedFaceGlyph("routing")).toBe("map");
    expect(closedFaceGlyph("modfx")).toBe("waveform");
    expect(closedFaceGlyph("cab")).toBe("mix");
    expect(closedFaceGlyph("io")).toBe("jack");
    expect(closedFaceGlyph("utility")).toBe("params");
    expect(closedChipChrome("osc", "osc1").lamp).toBe("lfo");
    expect(closedChipChrome("env", "env1").lamp).toBe("env");
    expect(closedChipChrome("filter", "filter1").lamp).toBe("clip");
    expect(closedChipChrome("reverb", "reverb1").lamp).toBe("clip");
  });

  it("gives each DSP family its target closed-face glyph", () => {
    for (const kind of FACE_KINDS) {
      expect(String(closedFaceGlyph(kind)), kind).toBe(TARGET_CLOSED_FACE[kind]);
    }
  });
});
