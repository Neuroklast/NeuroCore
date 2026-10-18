import { readFileSync } from "node:fs";
import { beforeEach, describe, expect, it } from "vitest";
import {
  capturePatchEditorView,
  PATCH_MODEL_URI,
  readPatchEditorView,
  resetPatchEditorView,
} from "./patchEditorSession";

const inspector = readFileSync(new URL("../assemble/SourceInspector.tsx", import.meta.url), "utf8");
const assemble = readFileSync(new URL("../assemble/AssembleView.tsx", import.meta.url), "utf8");
const hack = readFileSync(new URL("./HackView.tsx", import.meta.url), "utf8");
const patchEditor = readFileSync(new URL("./PatchEditor.tsx", import.meta.url), "utf8");
const app = readFileSync(new URL("../app/App.tsx", import.meta.url), "utf8");

describe("one patch editor session", () => {
  beforeEach(() => {
    resetPatchEditorView();
  });

  it("remembers caret and scroll so Circuit Edit and Terminal share them", () => {
    expect(PATCH_MODEL_URI).toContain("neurokore");
    expect(readPatchEditorView()).toEqual({
      cursorLine: 1,
      cursorColumn: 1,
      scrollTop: 0,
      scrollLeft: 0,
    });
    capturePatchEditorView({ cursorLine: 12, cursorColumn: 4, scrollTop: 80 });
    expect(readPatchEditorView()).toEqual({
      cursorLine: 12,
      cursorColumn: 4,
      scrollTop: 80,
      scrollLeft: 0,
    });
  });

  it("Source inspector Edit lazy-loads PatchEditor; Circuit does not import monacoEnv", () => {
    expect(inspector).toContain("PatchEditor");
    expect(inspector).toMatch(/lazy\(\s*\(\)\s*=>\s*import\(\s*["']\.\.\/hack\/PatchEditor["']\s*\)/);
    expect(inspector).not.toMatch(/<textarea/);
    expect(inspector).not.toMatch(/from ["'].*monacoEnv["']/);
    expect(assemble).not.toMatch(/from ["'].*monacoEnv["']/);
    expect(assemble).toContain("active={active}");
    expect(inspector).toContain("active");
  });

  it("Terminal Edit uses the same PatchEditor and shared model URI", () => {
    expect(hack).toContain("PatchEditor");
    expect(hack).toContain("PATCH_MODEL_URI");
    expect(patchEditor).toContain("monacoEnv");
    expect(patchEditor).toContain("PATCH_MODEL_URI");
    expect(patchEditor).toContain("capturePatchEditorView");
    expect(app).not.toMatch(/monacoEnv/);
    expect(app).toMatch(/lazy\(\s*\(\)\s*=>\s*import\(\s*["']\.\.\/hack\/HackView["']\s*\)/);
  });
});
