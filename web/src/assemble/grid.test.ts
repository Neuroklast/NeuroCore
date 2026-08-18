import { describe, expect, it } from "vitest";
import { snapToGrid } from "./grid";
import { isValidLink } from "./validateLink";

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
});
