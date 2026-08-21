import { describe, expect, it } from "vitest";
import {
  KNOB_CARD_CLIP,
  formatKnobDisplay,
  knobArcLen,
  knobArcOffset,
  knobBindKind,
  knobInteractive,
  knobPlugPlacement,
  knobScaleMode,
  knobShowsLetter,
  knobTickCount,
  knobTickNorms,
  knobTitleInset,
  round2,
  snapEnum01,
  enumAbbrev,
  enumLabelAt01,
  knobBindFace,
  applyWheel01,
  wheelStep01,
} from "./knobChrome";

describe("circuit knob chrome", () => {
  it("circuit rail has a bind jack and no letter over the title", () => {
    expect(knobShowsLetter(true)).toBe(false);
    expect(knobShowsLetter(false)).toBe(true);
    expect(knobBindKind(true)).toBe("jack");
    expect(knobBindKind(false)).toBe("letter");
    expect(knobPlugPlacement(true)).toBe("stack");
    expect(knobPlugPlacement(false)).toBe("corner");
  });

  it("fills the value arc with dashoffset, empty at 0 and closed at 1", () => {
    expect(knobArcOffset(0)).toBeCloseTo(knobArcLen());
    expect(knobArcOffset(1)).toBe(0);
    expect(knobArcOffset(0.5)).toBeCloseTo(knobArcLen() * 0.5);
    expect(KNOB_CARD_CLIP.startsWith("polygon(")).toBe(true);
    expect(KNOB_CARD_CLIP.includes("border-radius")).toBe(false);
  });

  it("dead knobs do not take drag or type", () => {
    expect(knobInteractive(true)).toBe(true);
    expect(knobInteractive(false)).toBe(false);
  });

  it("leaves air above the title on the left rail", () => {
    expect(knobTitleInset("corner")).toBeGreaterThanOrEqual(10);
    expect(knobTitleInset("stack")).toBeLessThan(knobTitleInset("corner"));
  });

  it("stores display text at 2 decimals; percent unit scales ×100", () => {
    expect(round2(0.35000002)).toBe("0.35");
    expect(formatKnobDisplay(0.35000002)).toBe("0.35");
    expect(formatKnobDisplay(0.405, "%")).toBe("40.50%");
    expect(formatKnobDisplay(1000.004, "Hz")).toBe("1000.00 Hz");
  });

  it("enum binds use N detents and N scale ticks, not a continuous arc", () => {
    const opts = ["lowpass", "highpass", "bandpass"];
    expect(knobScaleMode(opts)).toBe("ticks");
    expect(knobScaleMode(undefined)).toBe("arc");
    expect(knobTickCount(opts)).toBe(3);
    expect(knobTickNorms(3)).toEqual([0, 0.5, 1]);
    expect(snapEnum01(0.1, 3)).toBe(0);
    expect(snapEnum01(0.4, 3)).toBe(0.5);
    expect(snapEnum01(0.9, 3)).toBe(1);
    expect(enumLabelAt01(0.5, opts)).toBe("highpass");
  });

  it("abbreviates enum tokens so the knob face does not overflow", () => {
    expect(enumAbbrev("lowpass")).toBe("LP");
    expect(enumAbbrev("highpass")).toBe("HP");
    expect(enumAbbrev("bandpass")).toBe("BP");
    expect(enumAbbrev("lowshelf")).toBe("LS");
    expect(enumAbbrev("highshelf")).toBe("HS");
    expect(enumAbbrev("peak")).toBe("PK");
    expect(enumAbbrev("triangle")).toBe("TRI");
    expect(enumAbbrev("1/4")).toBe("1/4");
    expect(enumAbbrev("off")).toBe("OFF");
  });

  it("marks the source knob while a bind drag is live", () => {
    expect(knobBindFace("a", "a")).toBe("src");
    expect(knobBindFace("b", "a")).toBe("dim");
    expect(knobBindFace("a", null)).toBe("idle");
  });

  it("maps one wheel notch to a fixed 0..1 step and ignores a zero delta", () => {
    expect(wheelStep01(120)).toBeCloseTo(-0.03);
    expect(wheelStep01(-80)).toBeCloseTo(0.03);
    expect(wheelStep01(0)).toBe(0);
    expect(applyWheel01(0.4, -120)).toBeCloseTo(0.43);
    expect(applyWheel01(0.4, 120, true)).toBeCloseTo(0.3925);
    expect(applyWheel01(0.99, -120)).toBe(1);
    expect(applyWheel01(0.01, 120)).toBe(0);
  });
});
