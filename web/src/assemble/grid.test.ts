import { describe, expect, it } from "vitest";
import { snapToGrid } from "./grid";
import { cableAccent, cableFace, isValidLink } from "./validateLink";

describe("snapToGrid", () => {
  it("snaps to 16 px", () => {
    expect(snapToGrid(0)).toBe(0);
    expect(snapToGrid(17)).toBe(16);
    expect(snapToGrid(23)).toBe(16);
    expect(snapToGrid(24)).toBe(32);
  });
});

describe("isValidLink", () => {
  it("rejects output-to-output and input-to-input", () => {
    expect(isValidLink({ kind: "audio", output: true }, { kind: "audio", output: true })).toBe(false);
    expect(isValidLink({ kind: "audio", output: false }, { kind: "audio", output: false })).toBe(false);
  });

  it("accepts audio out to audio in", () => {
    expect(isValidLink({ kind: "audio", output: true }, { kind: "audio", output: false })).toBe(true);
  });

  it("rejects knob and audio/mod mismatch", () => {
    expect(isValidLink({ kind: "knob", output: true }, { kind: "audio", output: false })).toBe(false);
    expect(isValidLink({ kind: "audio", output: true }, { kind: "mod", output: false })).toBe(false);
  });

  it("keeps audio / param / LFO (mod) as separate nets", () => {
    expect(isValidLink({ kind: "param", output: true }, { kind: "param", output: false })).toBe(true);
    expect(isValidLink({ kind: "mod", output: true }, { kind: "mod", output: false })).toBe(true);
    expect(isValidLink({ kind: "audio", output: true }, { kind: "param", output: false })).toBe(false);
    expect(isValidLink({ kind: "param", output: true }, { kind: "mod", output: false })).toBe(false);
    expect(isValidLink({ kind: "mod", output: true }, { kind: "audio", output: false })).toBe(false);
  });

  it("paints audio red+flow, param yellow bottom, LFO blue top", () => {
    expect(cableAccent("audio")).toBe("#ff003c");
    expect(cableAccent("param")).toBe("#fcee0a");
    expect(cableAccent("mod")).toBe("#00f0ff");
    expect(cableFace("param")).toBe("bottom");
    expect(cableFace("mod")).toBe("top");
    expect(cableFace("audio")).toBe("side");
  });
});
