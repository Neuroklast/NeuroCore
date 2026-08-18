import { describe, expect, it } from "vitest";
import type { KnobState } from "../store/hostStore";
import { applyKnobEdit, knobMenuFields, parseKnobBound } from "./knobMenu";

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
  });
});
