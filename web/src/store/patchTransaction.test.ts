import { beforeEach, describe, expect, it } from "vitest";
import { applyCanvasScript, setCircuitArg } from "../assemble/addBlock";
import { commitBoardConnect } from "../assemble/boardCommit";
import { portId } from "../assemble/boardModel";
import { useBoardStore } from "../assemble/boardStore";
import { alreadyLinked } from "../assemble/connectModel";
import {
  canUndoScript,
  pushScriptHistory,
  resetScriptHistory,
  undoScript,
} from "./scriptHistory";
import { useAstStore } from "./astStore";
import { beginPatchTx, readPatchDocument } from "./patchWorkspace";

const ast = {
  version: 1 as const,
  leadingComments: [],
  params: [],
  nodes: [
    { id: "stage1", type: "stage", busName: "main", args: { y: "x" }, trailingComment: "" },
    { id: "filter1", type: "filter", busName: "main", args: { cutoff: "900" }, trailingComment: "" },
  ],
};

describe("patch transactions", () => {
  beforeEach(() => {
    resetScriptHistory();
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
      pendingTransaction: null,
    });
    useBoardStore.setState({
      nodes: {},
      ports: {},
      edges: {},
      userMoved: false,
      layoutEpoch: 0,
      commandedLayout: null,
      layoutBusy: false,
    });
  });

  it("failed compile keeps the active graph and revision", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify({ ...ast, nodes: [ast.nodes[0]] }),
      diagnostics: [],
    });
    const before = useAstStore.getState();
    const tx = beginPatchTx("compile", "editor");
    useAstStore.getState().setDraftScript("stage1: y = tanh(x\n");
    useAstStore.getState().applyCompileResult({
      origin: "editor",
      ok: false,
      script: "stage1: y = tanh(x\n",
      diagnostics: [{ line: 1, column: 1, message: "syntax" }],
      txId: tx.id,
      baseRevision: tx.baseRevision,
    });
    const after = useAstStore.getState();
    expect(after.lastValidAst).toBe(before.lastValidAst);
    expect(after.lastValidScript).toBe(before.lastValidScript);
    expect(after.activeRevision).toBe(before.activeRevision);
    expect(after.diagnostics[0]?.message).toBe("syntax");
    expect(readPatchDocument().pendingTransaction).toBeNull();
  });

  it("an older tx result does not land on a newer pending tx", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify({ ...ast, nodes: [ast.nodes[0]] }),
      diagnostics: [],
    });
    const first = beginPatchTx("compile", "editor");
    const second = beginPatchTx("compile", "editor");
    useAstStore.getState().setDraftScript("stage1: y = tanh(x)\n");
    useAstStore.getState().applyCompileResult({
      origin: "editor",
      ok: false,
      script: "stage1: y = x\n",
      diagnostics: [{ line: 1, column: 1, message: "stale" }],
      txId: first.id,
      baseRevision: first.baseRevision,
    });
    expect(useAstStore.getState().diagnostics).toEqual([]);
    expect(readPatchDocument().pendingTransaction?.id).toBe(second.id);
    useAstStore.getState().applyCompileResult({
      origin: "editor",
      ok: true,
      script: "stage1: y = tanh(x)\n",
      diagnostics: [],
      txId: second.id,
      baseRevision: second.baseRevision,
    });
    expect(readPatchDocument().pendingTransaction).toBeNull();
  });

  it("setArg coalesces one undo step for a gesture", () => {
    applyCanvasScript("filter1: cutoff = 900\n");
    setCircuitArg("filter1", "cutoff", "1000");
    setCircuitArg("filter1", "cutoff", "1200");
    setCircuitArg("filter1", "cutoff", "1400");
    expect(canUndoScript()).toBe(true);
    const back = undoScript(useAstStore.getState().lastValidScript);
    expect(back).toContain("cutoff = 900");
    expect(back).not.toContain("1200");
  });
});
