import { describe, expect, it } from "vitest";
import { driveFaceModel, driveSample, driveTransferPath } from "./driveTransfer";

describe("calculated drive transfer", () => {
  it("paints named engine shapers and leaves unknown formulas as identity", () => {
    expect(driveSample("stage", { y: "x" }, 0.8)).toBeCloseTo(0.8, 5);
    expect(driveFaceModel("stage", { y: "x" }).shaper).toBe("identity");
    expect(driveFaceModel("stage", { y: "x * (c + 1)" }).shaper).toBe("identity");
    expect(driveSample("stage", { y: "softclip(x, 4)" }, 0.8))
      .not.toBe(driveSample("stage", { y: "x" }, 0.8));
    expect(driveFaceModel("stage", { y: "softclip(x)" }).shaper).toBe("softclip");
    expect(driveFaceModel("stage", { y: "softclip(x)" }).drive).toBe(1);
    expect(driveFaceModel("stage", { y: "diode(x, c)" }).shaper).toBe("diode");
    expect(driveFaceModel("stage", { y: "diode(x, c)" }).drive).toBe(2);
    expect(driveTransferPath("stage", { y: "tube(x, 2)" }))
      .not.toBe(driveTransferPath("stage", { y: "tube(x, 8)" }));
  });

  it("uses the outer named shaper, bitcrush chip args, and mix", () => {
    expect(driveFaceModel("stage", { y: "hardclip(softclip(x, 1.1), 0.7)" }).shaper).toBe("hardclip");
    expect(driveFaceModel("stage", { y: "hardclip(softclip(x, 1.1), 0.7)" }).limit).toBe(0.7);
    expect(driveFaceModel("bitcrush", { bits: "6", mix: "0.5" }).shaper).toBe("bitcrush");
    expect(driveFaceModel("bitcrush", { bits: "6", mix: "0.5" }).bits).toBe(6);
    expect(driveSample("bitcrush", { bits: "4", mix: "1" }, 0.3))
      .not.toBe(driveSample("bitcrush", { bits: "12", mix: "1" }, 0.3));
    expect(driveSample("stage", { y: "fold(x, -0.4, 0.4)" }, 0.9))
      .toBeLessThan(0.5);
    expect(driveSample("stage", { y: "x" }, 0)).toBe(0);
  });
});
