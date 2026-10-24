import { describe, expect, it } from "vitest";
import {
  EMPTY_FACETS,
  activeFilterChips,
  emptyExplorerCopy,
  explorerFolders,
  filterExplorer,
  libraryFolders,
  neighborName,
  PRESET_NINE_ROW_BUDGET_PX,
  PRESET_ROW_PX,
  resolveSelectedName,
  saveAsError,
  scopeRows,
  seedSelectedName,
  selectedRow,
  SOUND_SOURCES,
  sourceFolders,
  styleFolders,
  toggleFacet,
  typeFolders,
} from "./explorerModel";
import facets from "../presets/presetFacets.json";

const presets = [
  { name: "Arp Shimmer", category: "Synth", author: "A", description: "shine", factory: true, tags: ["arp"], soundSources: ["Synth"], processingTypes: ["Modulation"], style: "Electronic" },
  { name: "Blues Break OD", category: "Distortion", author: "A", description: "break", factory: true, tags: ["od"], soundSources: ["Guitar"], processingTypes: ["Distortion"], style: "Rock" },
  { name: "Metal Scoop", category: "Distortion", author: "A", description: "scoop", factory: true, tags: ["metal"], soundSources: ["Guitar"], processingTypes: ["Distortion"], style: "Metal" },
  { name: "My Plate", category: "Reverb", author: "Me", description: "user plate", factory: false, tags: ["plate"] },
  { name: "Spring Tank", category: "Reverb", author: "A", description: "spring", factory: true, tags: ["spring"], soundSources: ["Any"], processingTypes: ["Reverb"] },
];

describe("explorer model", () => {
  it("uses the shared facet vocab", () => {
    expect(SOUND_SOURCES).toEqual(facets.soundSources);
  });

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
    expect(filterExplorer(presets, "modulation", "").map((p) => p.name)).toEqual(["Arp Shimmer"]);
    expect(filterExplorer(presets, "guitar", "").map((p) => p.name)).toEqual(["Blues Break OD", "Metal Scoop"]);
    expect(explorerFolders(scopeRows(presets, "factory")).find((f) => f.name === "")?.label).toBe("All Presets");
  });

  it("filters source and type independently and keeps search", () => {
    const empty = { sources: [] as string[], types: [] as string[], styles: [] as string[] };
    expect(filterExplorer(presets, "", "", { ...empty, sources: ["Guitar"] }).map((p) => p.name)).toEqual(["Blues Break OD", "Metal Scoop"]);
    expect(filterExplorer(presets, "", "", { ...empty, sources: ["Synth"], types: ["Modulation"] }).map((p) => p.name)).toEqual(["Arp Shimmer"]);
    expect(filterExplorer(presets, "", "", { ...empty, sources: ["Guitar"], types: ["Reverb"] })).toEqual([]);
    expect(filterExplorer(presets, "spring", "", { ...empty, sources: ["Any"] }).map((p) => p.name)).toEqual(["Spring Tank"]);
    expect(filterExplorer(presets, "", "", { ...empty, types: ["Distortion", "Reverb"] }).map((p) => p.name)).toEqual(["Blues Break OD", "Metal Scoop", "My Plate", "Spring Tank"]);
    expect(toggleFacet(["Guitar"], "Synth")).toEqual(["Guitar", "Synth"]);
    const sources = sourceFolders(presets, "", empty);
    expect(sources[0]?.label).toBe("All Presets");
    expect(sources.find((f) => f.name === "Guitar")?.count).toBe(2);
    expect(typeFolders(presets, "", { ...empty, sources: ["Guitar"] }).find((f) => f.name === "Distortion")?.count).toBe(2);
  });

  it("falls back to category when factory metadata is missing", () => {
    const empty = { sources: [] as string[], types: [] as string[], styles: [] as string[] };
    expect(filterExplorer(presets, "", "", { ...empty, types: ["Reverb"] }).map((p) => p.name)).toContain("My Plate");
  });

  it("reports Save As against a factory name", () => {
    expect(saveAsError("", false)).toBe("Enter a name.");
    expect(saveAsError("Spring Tank", true)).toBe("Factory names are locked. Pick another.");
    expect(saveAsError("My Hall", false)).toBe("");
  });

  it("keeps an active style visible when another facet zeros its count", () => {
    const facets = { sources: ["Guitar"], types: [] as string[], styles: ["Electronic"] };
    expect(filterExplorer(presets, "", "", facets)).toEqual([]);
    const styles = styleFolders(presets, "", facets);
    expect(styles.find((f) => f.name === "Electronic")).toEqual({ name: "Electronic", label: "Electronic", count: 0 });
  });

  it("keeps an active processing type visible so it can be cleared", () => {
    const facets = { sources: ["Synth"], types: ["Distortion"], styles: [] as string[] };
    expect(typeFolders(presets, "", facets).find((f) => f.name === "Distortion")).toEqual({
      name: "Distortion",
      label: "Distortion",
      count: 0,
    });
  });

  it("clear all restores the full scoped catalog without dropping other facets first", () => {
    const facets = { sources: ["Guitar"], types: ["Distortion"], styles: ["Metal"] };
    expect(filterExplorer(presets, "", "", facets).map((p) => p.name)).toEqual(["Metal Scoop"]);
    expect(filterExplorer(presets, "", "", EMPTY_FACETS).map((p) => p.name)).toEqual([
      "Arp Shimmer",
      "Blues Break OD",
      "Metal Scoop",
      "My Plate",
      "Spring Tank",
    ]);
    expect(activeFilterChips(facets)).toEqual([
      { facet: "source", value: "Guitar" },
      { facet: "type", value: "Distortion" },
      { facet: "style", value: "Metal" },
    ]);
  });

  it("keeps factory and user scope independent of facet chips", () => {
    expect(scopeRows(presets, "factory").every((p) => p.factory !== false)).toBe(true);
    expect(scopeRows(presets, "user").map((p) => p.name)).toEqual(["My Plate"]);
    expect(filterExplorer(scopeRows(presets, "user"), "", "", { ...EMPTY_FACETS, sources: ["Guitar"] })).toEqual([]);
    expect(filterExplorer(scopeRows(presets, "factory"), "", "", EMPTY_FACETS).map((p) => p.name)).not.toContain("My Plate");
  });

  it("does not let a leftover category hide facet results", () => {
    const byCat = filterExplorer(presets, "", "Distortion", EMPTY_FACETS);
    const visible = filterExplorer(presets, "", "", EMPTY_FACETS);
    expect(byCat.map((p) => p.name)).toEqual(["Blues Break OD", "Metal Scoop"]);
    expect(visible.map((p) => p.name)).toContain("Arp Shimmer");
  });

  it("distinguishes an empty catalog from no filter hits", () => {
    expect(emptyExplorerCopy(0, 0, false)).toBe("No presets available.");
    expect(emptyExplorerCopy(5, 0, true)).toBe("No matching presets. Change the search or filters.");
    expect(emptyExplorerCopy(5, 0, true)).not.toBe("No presets available.");
  });

  it("keeps two Heavy Drive rows from different packs", () => {
    const rows = [
      { id: "a", packId: "one", name: "Heavy Drive", category: "Distortion", factory: false, sourceType: "pack", origin: "One" },
      { id: "b", packId: "two", name: "Heavy Drive", category: "Distortion", factory: false, sourceType: "pack", origin: "Two" },
    ];
    expect(scopeRows(rows, "pack:one").map((p) => p.id)).toEqual(["a"]);
    expect(scopeRows(rows, "pack:two").map((p) => p.id)).toEqual(["b"]);
    expect(libraryFolders(rows, [{ id: "one", name: "One" }, { id: "two", name: "Two" }]).map((f) => f.label))
      .toEqual(["All Presets", "Factory", "My Presets", "Favorites", "One", "Two"]);
  });
});
