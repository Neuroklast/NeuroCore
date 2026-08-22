import { describe, expect, it } from "vitest";
import { cyclePresetStar, starGlyphs, writePresetStar } from "./presetStars";

describe("preset stars", () => {
  it("clamps 1..5", () => {
    const a = writePresetStar("Phaser Lab", 5, {});
    expect(a["Phaser Lab"]).toBe(5);
    expect(writePresetStar("Phaser Lab", 0, a)["Phaser Lab"]).toBe(1);
    expect(writePresetStar("Phaser Lab", 9, a)["Phaser Lab"]).toBe(5);
    expect(starGlyphs(0)).toBe("☆☆☆☆☆");
    expect(starGlyphs(3)).toBe("★★★☆☆");
  });

  it("cycles a single list cell through empty and five stars", () => {
    let row: Record<string, number> = {};
    expect(starGlyphs(row.Airy ?? 0)).toBe("☆☆☆☆☆");
    row = cyclePresetStar("Airy", row);
    expect(row.Airy).toBe(1);
    row = cyclePresetStar("Airy", row);
    row = cyclePresetStar("Airy", row);
    expect(starGlyphs(row.Airy ?? 0)).toBe("★★★☆☆");
    row = cyclePresetStar("Airy", row);
    row = cyclePresetStar("Airy", row);
    expect(row.Airy).toBe(5);
    row = cyclePresetStar("Airy", row);
    expect(row.Airy).toBeUndefined();
  });
});
