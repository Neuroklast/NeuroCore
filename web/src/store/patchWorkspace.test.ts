import { readFileSync } from "node:fs";
import { beforeEach, describe, expect, it } from "vitest";
import { terminalMounted } from "../app/workspace";
import { useAstStore } from "./astStore";
import { useHostStore } from "./hostStore";
import { readPatchDocument, readPatchSelection } from "./patchWorkspace";

const root = new URL("../../../", import.meta.url);
const hack = readFileSync(new URL("../hack/HackView.tsx", import.meta.url), "utf8");
const board = readFileSync(new URL("../assemble/BoardView.tsx", import.meta.url), "utf8");
const astSrc = readFileSync(new URL("./astStore.ts", import.meta.url), "utf8");
const app = readFileSync(new URL("../app/App.tsx", import.meta.url), "utf8");
const sourceMap = readFileSync(new URL("../hack/dslSourceMap.ts", import.meta.url), "utf8");
const overlays = readFileSync(new URL("../overlays/Overlays.tsx", import.meta.url), "utf8");

const validAst = {
  version: 1 as const,
  leadingComments: [],
  params: [],
  nodes: [{ id: "reverb1", type: "reverb", busName: "main", args: { mix: "0.3" }, trailingComment: "" }],
};

describe("PR1 baseline: Circuit and Terminal are split editors", () => {
  beforeEach(() => {
    useAstStore.setState({
      origin: "bridge",
      ast: null,
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
    });
    useHostStore.setState({ focusNodeId: null, overlay: null, inspectId: null });
  });

  it("Circuit stays mounted hidden; Terminal unmounts off the hack tab", () => {
    expect(app).toContain('workspace === "assemble" ? "h-full min-h-0" : "hidden"');
    expect(app).toContain("terminalMounted(workspace)");
    expect(terminalMounted("assemble")).toBe(false);
    expect(terminalMounted("hack")).toBe(true);
  });

  it("HackView owns editing in local useState so a tab change drops the mode", () => {
    expect(hack).toMatch(/const \[editing, setEditing\] = useState\(false\)/);
    expect(astSrc).not.toMatch(/draftStatus|activeRevision|draftRevision/);
  });

  it("Circuit selection is BoardView useState, separate from focusNodeId", () => {
    expect(board).toMatch(/const \[selection, setSelection\] = useState<string\[\]>\(\[\]\)/);
    expect(board).toContain("setFocusNodeId");
    const sel = readPatchSelection();
    expect(sel.param).toBeNull();
    expect(sel.origin).toBeNull();
    expect(sel.nodeIds).toEqual([]);
  });

  it("astStore has script and lastValidScript with no patch revision", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "reverb1: mix = 0.3\n",
      astJson: JSON.stringify(validAst),
      diagnostics: [],
    });
    useAstStore.getState().setDraftScript("reverb1: mix = 0.5\n");
    const doc = readPatchDocument();
    expect(doc.activeScript).toContain("0.3");
    expect(doc.draftScript).toContain("0.5");
    expect(doc.draftStatus).toBe("modified");
    expect(doc.activeRevision).toBe(0);
    expect(doc.draftRevision).toBe(0);
    expect(doc.pendingTransaction).toBeNull();
  });

  it("applyCompileResult stale-guards by comparing the script string", () => {
    expect(astSrc).toContain("payload.script !== useAstStore.getState().script");
    expect(astSrc).not.toMatch(/pendingTransaction|transactionId|draftRevision/);
  });

  it("boardIdForSource still guesses a prefixed board id", () => {
    expect(sourceMap).toContain("startsWith(`${want}_`)");
  });

  it("Inspect graphOp setArg is a separate path from Terminal publishScript", () => {
    expect(overlays).toContain('op: "setArg"');
    expect(hack).toContain('publishScript(draft, "editor")');
  });
});

describe("PR1 target contracts (must fail until later PRs)", () => {
  beforeEach(() => {
    useAstStore.setState({
      origin: "bridge",
      ast: null,
      lastValidAst: null,
      lastValidScript: "reverb1: mix = 0.3\n",
      script: "reverb1: mix = 0.3\n",
      diagnostics: [],
    });
  });

  it.fails("draftRevision increments when the draft text changes", () => {
    const before = readPatchDocument().draftRevision;
    useAstStore.getState().setDraftScript("reverb1: mix = 0.5\n");
    expect(readPatchDocument().draftRevision).toBeGreaterThan(before);
  });

  it.fails("activeRevision stays put while the draft is unapplied", () => {
    const active = readPatchDocument().activeRevision;
    useAstStore.getState().setDraftScript("reverb1: mix = 0.5\n");
    const doc = readPatchDocument();
    expect(doc.activeRevision).toBe(active);
    expect(doc.activeRevision).toBeGreaterThan(0);
    expect(doc.draftStatus).toBe("modified");
  });

  it.fails("selection records origin and param, not only a node id", () => {
    useHostStore.getState().setFocusNodeId("reverb1");
    const sel = readPatchSelection();
    expect(sel.primaryId).toBe("reverb1");
    expect(sel.origin).toBe("circuit");
    expect(sel.param).toBe("mix");
  });
});
