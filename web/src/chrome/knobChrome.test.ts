import { describe, expect, it } from "vitest";
import {
  KNOB_CARD_CLIP,
  knobArcLen,
  knobArcOffset,
  knobBindKind,
  knobInteractive,
  knobPlugPlacement,
  knobShowsLetter,
  knobTitleInset,
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
});
