import { describe, expect, it } from "vitest";
import {
  formatStepMutes,
  formatStepValues,
  readStepMutes,
  readStepValues,
  stepCountFromArgs,
  stepIndexFromPointer,
  stepValueFromPointer,
} from "./stepSeqModel";

describe("step sequencer model", () => {
  it("clamps the step count to 1..64", () => {
    expect(stepCountFromArgs({ steps: "0" })).toBe(1);
    expect(stepCountFromArgs({ steps: "64" })).toBe(64);
    expect(stepCountFromArgs({ steps: "999" })).toBe(64);
    expect(stepCountFromArgs({})).toBe(4);
  });

  it("reads values lists and legacy step args", () => {
    expect(readStepValues({ values: "1, -0.5, 0.25", steps: "3" })).toEqual([1, -0.5, 0.25]);
    expect(readStepValues({ steps: "2", step0: "0.5", step1: "-1" })).toEqual([0.5, -1]);
    expect(readStepValues({ steps: "2", values: "2, -2" })).toEqual([1, -1]);
  });

  it("round-trips values and mutes through DSL strings", () => {
    const values = [0.5, -0.25, 0, 1];
    expect(readStepValues({ values: formatStepValues(values), steps: "4" })).toEqual(values);
    const mutes = [true, false, true, false];
    expect(readStepMutes({ mutes: formatStepMutes(mutes), steps: "4" })).toEqual(mutes);
  });

  it("maps pointer y to a bipolar step value", () => {
    expect(stepValueFromPointer(0, 0, 100)).toBe(1);
    expect(stepValueFromPointer(50, 0, 100)).toBe(0);
    expect(stepValueFromPointer(100, 0, 100)).toBe(-1);
    expect(stepValueFromPointer(25, 0, 100)).toBe(0.5);
  });

  it("maps pointer x to a clamped step index", () => {
    expect(stepIndexFromPointer(0, 10, 400, 4)).toBe(0);
    expect(stepIndexFromPointer(210, 10, 400, 4)).toBe(2);
    expect(stepIndexFromPointer(999, 10, 400, 4)).toBe(3);
  });
});
