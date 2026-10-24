import { describe, expect, it } from "vitest";
import { movePackPreset, packCandidates, packSuggestedId, resolvePackSelection, selectPackPreset, validatePackDraft, type PackDraft } from "./packBuilderModel";
import type { ExplorerRow } from "./explorerModel";
const data: ExplorerRow[] = [
  { id: "factory:A", name: "A", category: "Bass", factory: true, sourceType: "factory" },
  { id: "user:b", name: "B", category: "Bass", factory: false, sourceType: "user" },
  { id: "pack:c", name: "C", category: "Bass", factory: false, sourceType: "pack" },
  { name: "Unknown", category: "Bass", factory: false, sourceType: "user" },
];
const meta: PackDraft = {
  name: "Bass Essentials", author: "Neuroklast", version: "1.0.0",
  description: "", website: "", artworkSource: "", id: "pack.neuroklast.bass-essentials",
};
describe("pack builder", () => {
  it("offers file-backed presets, never embedded factory or unidentified entries", () => {
    expect(packCandidates(data).map((row) => row.id)).toEqual(["user:b", "pack:c"]);
    expect(resolvePackSelection(data, ["factory:A", "pack:c", "missing", "user:b"])
      .map((row) => row.id)).toEqual(["pack:c", "user:b"]);
  });
  it("preserves explicitly chosen order and supports removing and moving entries", () => {
    expect(selectPackPreset(["user:b"], "pack:c", true)).toEqual(["user:b", "pack:c"]);
    expect(selectPackPreset(["user:b"], "user:b", true)).toEqual(["user:b"]);
    expect(selectPackPreset(["user:b", "pack:c"], "user:b", false)).toEqual(["pack:c"]);
    expect(movePackPreset(["user:b", "pack:c"], 1, -1)).toEqual(["pack:c", "user:b"]);
    expect(movePackPreset(["user:b", "pack:c"], 0, -1)).toEqual(["user:b", "pack:c"]);
  });
  it("validates metadata and export selection", () => {
    expect(validatePackDraft(meta, [data[1]!])).toEqual([]);
    expect(validatePackDraft({ ...meta, name: "", author: "", id: "..", version: "one", website: "http://unsafe.example" }, []))
      .toHaveLength(6);
    expect(validatePackDraft({ ...meta, id: ".hidden" }, [data[1]!]).length).toBeGreaterThan(0);
  });
  it("produces safe stable suggested pack identifiers", () => {
    expect(packSuggestedId("NEUROKLAST", "Industrial Essentials")).toBe("pack.neuroklast.industrial-essentials");
    expect(packSuggestedId("Müller", "Äther")).toBe("pack.muller.ather");
  });
});
