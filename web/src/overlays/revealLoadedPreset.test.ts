import { describe, expect, it } from "vitest";
import { revealLoadedPreset } from "./revealLoadedPreset";

const presets = [
  { name: "Arp Shimmer", category: "Synth" },
  { name: "Blues Break OD", category: "Distortion" },
  { name: "Metal Scoop", category: "Distortion" },
  { name: "Tape Wobble", category: "Lo-Fi" },
];

describe("reveal loaded preset", () => {
  it("opens the loaded category and selects that row after name sort", () => {
    const hit = revealLoadedPreset(presets, "Blues Break OD");
    expect(hit).toEqual({ cat: "Distortion", sel: 0, scope: "all", q: "" });
    const rows = presets
      .filter((p) => p.category === "Distortion")
      .sort((a, b) => a.name.localeCompare(b.name));
    expect(rows[hit!.sel]?.name).toBe("Blues Break OD");
  });

  it("selects the later Distortion row when that name is loaded", () => {
    const hit = revealLoadedPreset(presets, "Metal Scoop");
    expect(hit?.cat).toBe("Distortion");
    expect(hit?.sel).toBe(1);
  });

  it("returns null when untitled or unknown", () => {
    expect(revealLoadedPreset(presets, "")).toBeNull();
    expect(revealLoadedPreset(presets, "untitled")).toBeNull();
  });
});
