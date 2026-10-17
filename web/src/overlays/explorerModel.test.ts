import { describe, expect, it } from "vitest";
import {
  explorerFolders,
  filterExplorer,
  neighborName,
  PRESET_NINE_ROW_BUDGET_PX,
  PRESET_ROW_PX,
  resolveSelectedName,
  saveAsError,
  scopeRows,
  seedSelectedName,
  selectedRow,
} from "./explorerModel";

const presets = [
  { name: "Arp Shimmer", category: "Synth", author: "A", description: "shine", factory: true, tags: ["arp"] },
  { name: "Blues Break OD", category: "Distortion", author: "A", description: "break", factory: true, tags: ["od"] },
  { name: "Metal Scoop", category: "Distortion", author: "A", description: "scoop", factory: true, tags: ["metal"] },
  { name: "My Plate", category: "Reverb", author: "Me", description: "user plate", factory: false, tags: ["plate"] },
  { name: "Spring Tank", category: "Reverb", author: "A", description: "spring", factory: true, tags: ["spring"] },
];

describe("explorer model", () => {
  it("keeps nine 44px rows plus heading inside 440px", () => {
    expect(PRESET_ROW_PX).toBe(44);
    expect(PRESET_NINE_ROW_BUDGET_PX).toBeLessThanOrEqual(440);
  });

  it("keeps the same preset when the filter order changes", () => {
    const dist = filterExplorer(presets, "", "Distortion");
    expect(resolveSelectedName(dist, "Metal Scoop")).toBe("Metal Scoop");
    expect(selectedRow(dist, "Metal Scoop")?.name).toBe("Metal Scoop");
    const all = filterExplorer(presets, "", "");
    expect(resolveSelectedName(all, "Metal Scoop")).toBe("Metal Scoop");
    expect(resolveSelectedName(dist, "Spring Tank")).toBe("Blues Break OD");
  });

  it("does not wipe a restored selection on open", () => {
    expect(seedSelectedName(presets, "Spring Tank", "Metal Scoop")).toBe("Metal Scoop");
    expect(seedSelectedName(presets, "Spring Tank", "")).toBe("Spring Tank");
    expect(seedSelectedName(presets, "", "")).toBe("Arp Shimmer");
    expect(seedSelectedName(presets, "untitled", "")).toBe("Arp Shimmer");
  });

  it("moves by name, not list index", () => {
    const dist = filterExplorer(presets, "", "Distortion");
    expect(neighborName(dist, "Blues Break OD", "ArrowDown")).toBe("Metal Scoop");
    expect(neighborName(dist, "Metal Scoop", "ArrowDown")).toBe("Metal Scoop");
    expect(neighborName(dist, "Metal Scoop", "ArrowUp")).toBe("Blues Break OD");
  });

  it("searches name, tags, description — not a formula field", () => {
    expect(filterExplorer(presets, "spring", "").map((p) => p.name)).toEqual(["Spring Tank"]);
    expect(filterExplorer(presets, "od", "").map((p) => p.name)).toEqual(["Blues Break OD"]);
    expect(explorerFolders(scopeRows(presets, "factory")).find((f) => f.name === "")?.label).toBe("All Presets");
  });

  it("reports Save As against a factory name", () => {
    expect(saveAsError("", false)).toBe("Enter a name.");
    expect(saveAsError("Spring Tank", true)).toBe("Factory names are locked. Pick another.");
    expect(saveAsError("My Hall", false)).toBe("");
  });
});
