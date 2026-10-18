import { readFileSync } from "node:fs";
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
import { applyPatchDraft } from "../hack/terminalCompile";
import { useAstStore } from "./astStore";
import { beginPatchTx, draftBanner, readPatchDocument } from "./patchWorkspace";

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
      draftBaseRevision: 0,
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

  it("AstEventPayload carries txId and baseRevision like compileResult", () => {
    const astTs = readFileSync(new URL("../bridge/ast.ts", import.meta.url), "utf8");
    const native = readFileSync(new URL("../../../src/bridge/CompileSession.cpp", import.meta.url), "utf8");
    const holder = readFileSync(new URL("../../../src/bridge/WebViewHolder.cpp", import.meta.url), "utf8");
    expect(astTs).toMatch(/export interface AstEventPayload \{[^}]*txId\?: string/s);
    expect(astTs).toMatch(/export interface AstEventPayload \{[^}]*baseRevision\?: number/s);
    expect(native).toContain("txId");
    expect(holder).toContain("txId");
  });

  it("a late ast event for an older tx does not replace a newer graph", () => {
    const stage = { ...ast, nodes: [ast.nodes[0]] };
    const filter = { ...ast, nodes: [ast.nodes[1]] };
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(stage),
      diagnostics: [],
    });
    const first = beginPatchTx("compile", "editor");
    const second = beginPatchTx("compile", "editor");
    useAstStore.getState().applyAstEvent({
      origin: "editor",
      script: "filter1: cutoff = 900\n",
      astJson: JSON.stringify(filter),
      diagnostics: [],
      txId: second.id,
      baseRevision: second.baseRevision,
    });
    expect(useAstStore.getState().lastValidAst?.nodes[0]?.id).toBe("filter1");
    const after = useAstStore.getState().activeRevision;
    useAstStore.getState().applyAstEvent({
      origin: "editor",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(stage),
      diagnostics: [],
      txId: first.id,
      baseRevision: first.baseRevision,
    });
    expect(useAstStore.getState().lastValidAst?.nodes[0]?.id).toBe("filter1");
    expect(useAstStore.getState().lastValidScript).toContain("filter1");
    expect(useAstStore.getState().activeRevision).toBe(after);
  });

  it("a preset load with an unapplied draft is a conflict; Apply does not overwrite", async () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify({ ...ast, nodes: [ast.nodes[0]] }),
      diagnostics: [],
    });
    useAstStore.getState().setDraftEditing(true);
    useAstStore.getState().setDraftScript("stage1: y = tanh(x)\n");
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script: "filter1: cutoff = 900\n",
      astJson: JSON.stringify({ ...ast, nodes: [ast.nodes[1]] }),
      diagnostics: [],
    });
    const doc = readPatchDocument();
    expect(doc.draftStatus).toBe("conflict");
    expect(draftBanner(doc.draftStatus)).toBe("CONFLICT");
    expect(doc.draftScript).toContain("tanh");
    expect(doc.activeScript).toContain("filter1");
    const ok = await applyPatchDraft();
    expect(ok).toBe(false);
    expect(useAstStore.getState().lastValidScript).toContain("filter1");
    expect(useAstStore.getState().script).toContain("tanh");
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
