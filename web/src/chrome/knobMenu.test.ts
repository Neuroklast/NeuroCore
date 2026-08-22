import { describe, expect, it } from "vitest";
import type { KnobState } from "../store/hostStore";
import { applyKnobEdit, knobMenuFields, parseKnobBound, rewriteParamLine } from "./knobMenu";

const base: KnobState = {
  id: "a",
  name: "Rate",
  value: 0.4,
  active: true,
  min: 0.05,
  max: 6,
  isNote: false,
};

describe("knob context menu", () => {
  it("offers name, range, unit and MIDI learn in Unit and Circuit", () => {
    expect(knobMenuFields(base)).toEqual(["name", "min", "max", "unit", "note", "learn"]);
    expect(knobMenuFields({ enums: ["lowpass", "highpass"] })).toEqual(["name", "learn"]);
  });

  it("renames and remaps min/max, swapping inverted bounds", () => {
    const named = applyKnobEdit(base, { name: "  Drive  " });
    expect(named.name).toBe("Drive");
    expect(named.id).toBe("a");
    expect(named.value).toBe(0.4);

    const ranged = applyKnobEdit(base, { min: 200, max: 80 });
    expect(ranged.min).toBe(80);
    expect(ranged.max).toBe(200);

    const note = applyKnobEdit(base, { isNote: true, unit: "Hz" });
    expect(note.isNote).toBe(true);
    expect(note.unit).toBe("Hz");

    expect(applyKnobEdit(base, { name: "   " }).name).toBe("A");
    expect(parseKnobBound("1.25", 0)).toBe(1.25);
    expect(parseKnobBound("x", 3)).toBe(3);
    expect(parseKnobBound("1/4", 0, true)).toBeCloseTo(0.25);
  });

  it("flips time bounds onto the note grid and writes them back as ms", () => {
    const delay: KnobState = {
      id: "a",
      name: "Time",
      value: 0.5,
      active: true,
      min: 20,
      max: 2000,
      unit: "ms",
      isNote: false,
    };
    const on = applyKnobEdit(delay, { isNote: true }, 120);
    expect(on.isNote).toBe(true);
    expect(on.min).toBeCloseTo(0.0625, 3);
    expect(on.max).toBeCloseTo(1, 3);
    const off = applyKnobEdit(on, { isNote: false }, 120);
    expect(off.isNote).toBe(false);
    expect(off.min).toBeCloseTo(125, 0);
    expect(off.max).toBeCloseTo(2000, 0);
  });

  it("rewrites the param line with note tokens for the terminal", () => {
    const src = "param a = Time [20, 2000]\ndelay1: time = a\n";
    const next = rewriteParamLine(src, "a", { min: 1, max: 0.0625, isNote: true });
    expect(next).toContain("param a = Time [1/1, 1/16]");
    expect(next).toContain("delay1: time = a");
  });
});
