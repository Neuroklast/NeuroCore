import { describe, expect, it } from "vitest";
import { writePresetStar } from "./presetStars";

describe("preset stars", () => {
  it("clamps 1..5", () => {
    const a = writePresetStar("Phaser Lab", 5, {});
    expect(a["Phaser Lab"]).toBe(5);
    expect(writePresetStar("Phaser Lab", 0, a)["Phaser Lab"]).toBe(1);
    expect(writePresetStar("Phaser Lab", 9, a)["Phaser Lab"]).toBe(5);
  });
});
