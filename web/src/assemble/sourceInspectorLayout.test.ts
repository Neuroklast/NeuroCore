import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { useAstStore } from "../store/astStore";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";
import { publishSelection } from "../store/patchWorkspace";
import { shouldFitCameraOnPaneSize } from "./boardCamera";
import {
  clampInspectorWidth,
  inspectorBox,
  inspectorDock,
  SOURCE_INSPECTOR,
} from "./sourceInspectorLayout";

const board = readFileSync(new URL("./BoardView.tsx", import.meta.url), "utf8");
const assemble = readFileSync(new URL("./AssembleView.tsx", import.meta.url), "utf8");
const inspector = readFileSync(new URL("./SourceInspector.tsx", import.meta.url), "utf8");
const app = readFileSync(new URL("../app/App.tsx", import.meta.url), "utf8");

describe("source inspector geometry on the 1280×860 stage", () => {
  it("keeps the side panel inside 360–420 logical pixels", () => {
    expect(SOURCE_INSPECTOR.width).toBeGreaterThanOrEqual(360);
    expect(SOURCE_INSPECTOR.width).toBeLessThanOrEqual(420);
    expect(clampInspectorWidth(200, 1280)).toBe(360);
    expect(clampInspectorWidth(900, 1280)).toBe(420);
    expect(clampInspectorWidth(384, 1280)).toBe(384);
  });

  it("uses the full Circuit pane when closed and docks bottom when the board would starve", () => {
    expect(inspectorDock(false, 1280, 640)).toBe("closed");
    expect(inspectorBox("closed", 1280, 640)).toEqual({ w: 0, h: 0 });
    expect(inspectorDock(true, 1280, 640)).toBe("side");
    expect(inspectorDock(true, 800, 640)).toBe("bottom");
    expect(inspectorBox("side", 1280, 640).w).toBe(384);
    expect(inspectorBox("bottom", 800, 640).w).toBe(800);
    expect(inspectorBox("bottom", 800, 640).h).toBeGreaterThanOrEqual(160);
  });

  it("does not refit the camera when an already-valid pane only changes size", () => {
    expect(shouldFitCameraOnPaneSize(0, 0, 960, 420, false)).toBe(true);
    expect(shouldFitCameraOnPaneSize(960, 420, 576, 420, false)).toBe(false);
    expect(shouldFitCameraOnPaneSize(960, 420, 576, 420, true)).toBe(false);
    expect(board).toContain("shouldFitCameraOnPaneSize");
  });

  it("AssembleView owns the inspector; BoardView stays a board; Monaco stays lazy", () => {
    expect(assemble).toContain("nk-source-inspector");
    expect(assemble).toContain("SourceInspector");
    expect(board).not.toContain("nk-source-inspector");
    expect(app).toMatch(/lazy\(\s*\(\)\s*=>\s*import\(\s*["']\.\.\/hack\/HackView["']\s*\)/);
    expect(assemble).not.toMatch(/from ["'].*monacoEnv["']/);
  });

  it("selecting a chip does not open Source or enter Edit", () => {
    useSourceInspectorStore.setState({ open: false });
    useAstStore.setState({ draftEditing: false });
    publishSelection({ nodeIds: ["reverb1"], primaryId: "reverb1", origin: "circuit", intent: "mark" });
    expect(useSourceInspectorStore.getState().open).toBe(false);
    expect(useAstStore.getState().draftEditing).toBe(false);
    expect(inspector).toContain("Edit Source");
    expect(inspector).toContain("Open Full Terminal");
    expect(inspector).toContain("data-chip-id");
    expect(inspector).toContain("nk-source-meter");
    expect(readFileSync(new URL("./BoardChip.tsx", import.meta.url), "utf8")).toContain("data-kind");
  });
});
