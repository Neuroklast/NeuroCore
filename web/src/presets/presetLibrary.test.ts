import { describe, expect, it } from "vitest";
import { importPreview, type PackPreview } from "./presetLibrary";

const first: PackPreview = {
  ok: false,
  error: "Not a preset or pack",
  name: "a",
  id: "",
  author: "",
  description: "",
  version: "",
  presetCount: 0,
  categories: [],
  warnings: [],
  source: "a.nrk",
};

describe("importPreview", () => {
  it("keeps a single-file analyze result", () => {
    expect(importPreview(["a.nrk"], first)).toEqual(first);
  });

  it("names every selected file so install can take the full list", () => {
    const next = importPreview(["a.nrk", "b.nrk"], first);
    expect(next.ok).toBe(true);
    expect(next.name).toBe("2 files");
    expect(next.presetCount).toBe(2);
    expect(next.source).toBe("a.nrk");
  });
});
