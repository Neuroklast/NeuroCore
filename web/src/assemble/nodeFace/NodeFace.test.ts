import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { closedFaceGlyph, faceKindForType, TARGET_CLOSED_FACE } from "./faceKind";

const filterFace = readFileSync(new URL("./FilterFace.tsx", import.meta.url), "utf8");
const reverbFace = readFileSync(new URL("./ReverbFace.tsx", import.meta.url), "utf8");
const nodeFace = readFileSync(new URL("./NodeFace.tsx", import.meta.url), "utf8");

describe("reference NodeFaces", () => {
  it("registers filter and reverb as distinct calculated plots", () => {
    expect(faceKindForType("filter")).toBe("filter");
    expect(faceKindForType("eq")).toBe("filter");
    expect(faceKindForType("reverb")).toBe("reverb");
    expect(closedFaceGlyph("filter")).toBe(TARGET_CLOSED_FACE.filter);
    expect(closedFaceGlyph("reverb")).toBe(TARGET_CLOSED_FACE.reverb);
    expect(closedFaceGlyph("filter")).not.toBe(closedFaceGlyph("reverb"));
    expect(filterFace).toContain('data-glyph="response"');
    expect(filterFace).toContain("calculated filter response");
    expect(reverbFace).toContain('data-glyph="decay"');
    expect(reverbFace).toContain("calculated decay envelope");
    expect(nodeFace).toContain("FilterFace");
    expect(nodeFace).toContain("ReverbFace");
  });
});
