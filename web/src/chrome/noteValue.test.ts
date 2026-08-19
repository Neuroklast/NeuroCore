import { describe, expect, it } from "vitest";
import {
  formatKnobDisplay,
  formatNoteBound,
  noteLabelForMapped,
  noteUnitForRange,
  parseNoteToken,
  round2,
  typedToMapped,
  wholeToHz,
  wholeToMs,
} from "./noteValue";

describe("knob note values", () => {
  it("parses 1/4, dotted, and bar", () => {
    expect(parseNoteToken("1/4")).toBeCloseTo(0.25);
    expect(parseNoteToken("1/2")).toBeCloseTo(0.5);
    expect(parseNoteToken("1/4.")).toBeCloseTo(0.375);
    expect(parseNoteToken("bar")).toBeCloseTo(1);
    expect(parseNoteToken("250")).toBeNull();
  });

  it("turns 1/4 into 500 ms or 2 Hz at 120 BPM", () => {
    expect(wholeToMs(0.25, 120)).toBeCloseTo(500);
    expect(wholeToHz(0.25, 120)).toBeCloseTo(2);
  });

  it("does not invent note labels on a numeric range", () => {
    expect(noteUnitForRange(180, 520)).toBeNull();
    expect(noteUnitForRange(0.05, 6)).toBeNull();
    expect(noteUnitForRange(200, 8000)).toBeNull();
    expect(typedToMapped("1/4", 180, 520, 120, false)).toBeNull();
    expect(typedToMapped("250", 180, 520, 120, false)).toBe(250);
    expect(noteLabelForMapped(500, 180, 520, 120, false)).toBeNull();
    expect(noteLabelForMapped(2, 0.05, 6, 120, false)).toBeNull();
  });

  it("uses the note grid only when the param range is notes", () => {
    const norm = typedToMapped("1/4", 1, 0.0625, 120, true);
    expect(norm).not.toBeNull();
    expect(noteLabelForMapped(0.25, 1, 0.0625, 120, true, norm ?? 0)).toBe("1/4");
    expect(formatNoteBound(1, 0.0625, "min", true)).toBe("1/1");
    expect(formatNoteBound(1, 0.0625, "max", true)).toBe("1/16");
    expect(formatNoteBound(180, 520, "min", false)).toBeNull();
  });

  it("rounds display to 2 dp and scales percent units", () => {
    expect(round2(0.35000002)).toBe("0.35");
    expect(formatKnobDisplay(0.405, "%")).toBe("40.50%");
  });
});
