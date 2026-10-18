import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { PATCH_MODEL_URI } from "./patchEditorSession";
import { draftBanner } from "../store/patchWorkspace";
import { patchStructure } from "../store/patchStructure";
import { findFactory } from "../presets/factoryCatalog";
import { parseDslSketch } from "../presets/parseDslSketch";

const dir = path.dirname(fileURLToPath(import.meta.url));
const hack = readFileSync(path.join(dir, "HackView.tsx"), "utf8");
const editor = readFileSync(path.join(dir, "PatchEditor.tsx"), "utf8");

describe("Terminal workspace baseline", () => {
  it("keeps the shared Monaco document as the only editor state", () => {
    expect(PATCH_MODEL_URI).toContain("neurokore");
    expect(hack).toContain("PATCH_MODEL_URI");
    expect(editor).toContain("PATCH_MODEL_URI");
    expect(hack).toContain("PatchEditor");
    expect(hack).not.toContain("new monaco.editor.createModel");
  });

  it("labels dirty drafts UNAPPLIED / INVALID / CONFLICT", () => {
    expect(draftBanner("modified")).toBe("UNAPPLIED");
    expect(draftBanner("invalid")).toBe("INVALID");
    expect(draftBanner("conflict")).toBe("CONFLICT");
    expect(draftBanner("clean")).toBeNull();
  });

  it("does not mount a second Circuit canvas in Terminal", () => {
    expect(hack).not.toContain("BoardView");
    expect(hack).not.toContain("CableCanvas");
  });

  it("shows VALIDATING and APPLYING on the Terminal banner", () => {
    expect(draftBanner("validating")).toBe("VALIDATING");
    expect(draftBanner("applying")).toBe("APPLYING");
  });

  it("renders a structure navigator next to the source", () => {
    expect(hack).toMatch(/data-patch-structure|nk-term-structure|PatchStructure/);
    const row = findFactory("Drone Low Bed");
    const { doc } = parseDslSketch(row!.script);
    expect(patchStructure(doc).some((n) => n.id === "filter1")).toBe(true);
    expect(hack).toContain("patchStructure(");
    expect(hack).toContain("publishSelection");
    expect(hack).toContain("origin: \"nav\"");
  });
});
