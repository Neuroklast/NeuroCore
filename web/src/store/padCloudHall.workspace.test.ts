import { beforeEach, describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { undoCircuit } from "../assemble/addBlock";
import { useBoardStore } from "../assemble/boardStore";
import { inferModLinks } from "../assemble/flowFromAst";
import { shouldFitCameraOnPaneSize } from "../assemble/boardCamera";
import { terminalMounted } from "../app/workspace";
import { sourceForNode } from "../hack/dslSourceMap";
import { applyPatchDraft } from "../hack/terminalCompile";
import {
  capturePatchEditorView,
  readPatchEditorView,
  resetPatchEditorView,
} from "../hack/patchEditorSession";
import { parseDslSketch } from "../presets/parseDslSketch";
import { findFactory } from "../presets/factoryCatalog";
import { resetScriptHistory } from "./scriptHistory";
import { useAstStore } from "./astStore";
import { useHostStore } from "./hostStore";
import { EMPTY_SELECTION, usePatchSelectionStore } from "./patchSelectionStore";
import {
  draftBanner,
  publishSelection,
  readPatchDocument,
  readPatchSelection,
} from "./patchWorkspace";

const board = readFileSync(new URL("../assemble/BoardView.tsx", import.meta.url), "utf8");

const PAD = "Pad Cloud Hall";

function loadPad() {
  const preset = findFactory(PAD);
  expect(preset, PAD).toBeTruthy();
  const script = preset!.script;
  const { doc } = parseDslSketch(script);
  useAstStore.getState().applyAstEvent({
    origin: "preset",
    script,
    astJson: JSON.stringify(doc),
    diagnostics: [],
  });
  useBoardStore.getState().hydrate(useAstStore.getState().lastValidAst);
  return script;
}

describe("Pad Cloud Hall workspace", () => {
  beforeEach(() => {
    resetScriptHistory();
    resetPatchEditorView();
    useAstStore.setState({
      origin: "bridge",
      ast: null,
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
      activeRevision: 0,
      draftRevision: 0,
      draftEditing: false,
      draftBaseRevision: 0,
      pendingTransaction: null,
    });
    useHostStore.setState({ focusNodeId: null, overlay: null, inspectId: null });
    usePatchSelectionStore.setState({ ...EMPTY_SELECTION });
    useBoardStore.setState({ camera: { tx: 40, ty: 24, scale: 1.25 }, userMoved: false });
  });

  it("select, edit, apply, undo, tabs and invalid DSL keep one patch", async () => {
    const original = loadPad();
    expect(original).toContain("reverb1:");
    expect(original).toContain("osc1:");
    expect(original).toContain("delay1:");

    publishSelection({
      nodeIds: ["reverb1"],
      primaryId: "reverb1",
      origin: "circuit",
      intent: "mark",
    });
    expect(sourceForNode(useAstStore.getState().lastValidScript, "reverb1")).toContain("decay = 0.88");
    expect(readPatchSelection().primaryId).toBe("reverb1");

    useAstStore.getState().setDraftEditing(true);
    const edited = original.replace("decay = 0.88", "decay = 0.72");
    useAstStore.getState().setDraftScript(edited);
    expect(useAstStore.getState().lastValidScript).toContain("decay = 0.88");
    expect(readPatchDocument().draftStatus).toBe("modified");
    expect(draftBanner(readPatchDocument().draftStatus)).toBe("UNAPPLIED");

    const ok = await applyPatchDraft();
    expect(ok).toBe(true);
    expect(useAstStore.getState().lastValidScript).toContain("decay = 0.72");
    expect(useAstStore.getState().lastValidAst?.nodes.find((n) => n.id === "reverb1")?.args.decay).toBe("0.72");
    expect(useAstStore.getState().draftEditing).toBe(false);

    const undone = await undoCircuit();
    expect(undone).toBe(true);
    expect(useAstStore.getState().lastValidScript).toContain("decay = 0.88");
    expect(useAstStore.getState().script).toContain("decay = 0.88");

    const ast = useAstStore.getState().lastValidAst;
    const mods = inferModLinks(ast?.nodes ?? []);
    expect(mods.some((e) => e.from === "osc1" && e.to === "delay1")).toBe(true);

    const cam = { ...useBoardStore.getState().camera };
    publishSelection({
      nodeIds: ["delay1"],
      primaryId: "delay1",
      origin: "terminal",
      intent: "mark",
    });
    expect(terminalMounted("assemble")).toBe(false);
    expect(readPatchSelection().primaryId).toBe("delay1");
    expect(useBoardStore.getState().camera).toEqual(cam);
    expect(shouldFitCameraOnPaneSize(960, 420, 576, 420, false)).toBe(false);
    expect(board).not.toMatch(/focusNodeId[^\n]*fitCamera/);

    capturePatchEditorView({ cursorLine: 8, cursorColumn: 12, scrollTop: 48 });
    useAstStore.getState().setDraftEditing(true);
    useAstStore.getState().setDraftScript(edited);
    expect(terminalMounted("assemble")).toBe(false);
    expect(useAstStore.getState().draftEditing).toBe(true);
    expect(useAstStore.getState().script).toContain("decay = 0.72");
    expect(readPatchEditorView().cursorLine).toBe(8);
    expect(readPatchEditorView().scrollTop).toBe(48);

    const active = useAstStore.getState().lastValidScript;
    useAstStore.getState().setDraftScript("this is not dsl");
    const bad = await applyPatchDraft();
    expect(bad).toBe(false);
    expect(useAstStore.getState().lastValidScript).toBe(active);
    expect(useAstStore.getState().diagnostics.length).toBeGreaterThan(0);

    useAstStore.getState().setDraftScript(edited);
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script: original.replace("reverb1:", "stage1: y = x\nreverb1:"),
      astJson: JSON.stringify(parseDslSketch("stage1: y = x\n").doc),
      diagnostics: [],
    });
    expect(readPatchDocument().draftStatus).toBe("conflict");
    expect(await applyPatchDraft()).toBe(false);
    expect(useAstStore.getState().script).toContain("decay = 0.72");
  });
});
