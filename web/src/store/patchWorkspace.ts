import type { AstDocument, Diagnostic } from "../bridge/ast";
import { useAstStore } from "./astStore";
import { useHostStore } from "./hostStore";

/** Draft vs the last DSP-confirmed script. */
export type DraftStatus =
  | "clean"
  | "modified"
  | "validating"
  | "applying"
  | "invalid"
  | "conflict";

export type PatchTransaction = {
  id: string;
  baseRevision: number;
};

/** One patch document. Active fields are DSP truth after native confirm. */
export type PatchDocument = {
  activeScript: string;
  activeAst: AstDocument | null;
  activeRevision: number;
  draftScript: string;
  draftRevision: number;
  draftStatus: DraftStatus;
  diagnostics: Diagnostic[];
  pendingTransaction: PatchTransaction | null;
};

export type SelectionOrigin = "circuit" | "terminal" | "inspect" | "nav";
export type SelectionIntent = "mark" | "reveal";

export type PatchSelection = {
  nodeIds: string[];
  primaryId: string | null;
  param: string | null;
  edgeId: string | null;
  origin: SelectionOrigin | null;
  intent: SelectionIntent;
};

function draftLooksModified(draft: string, active: string): boolean {
  return draft.length > 0 && active.length > 0 && draft !== active;
}

/** Adapter over today's astStore. Revisions and transactions are not tracked yet. */
export function readPatchDocument(): PatchDocument {
  const s = useAstStore.getState();
  const activeScript = s.lastValidScript;
  const draftScript = s.script;
  return {
    activeScript,
    activeAst: s.lastValidAst,
    activeRevision: 0,
    draftScript,
    draftRevision: 0,
    draftStatus: s.diagnostics.length > 0
      ? "invalid"
      : draftLooksModified(draftScript, activeScript)
        ? "modified"
        : "clean",
    diagnostics: s.diagnostics,
    pendingTransaction: null,
  };
}

/** Adapter over today's focusNodeId. Circuit's local selection is not included. */
export function readPatchSelection(): PatchSelection {
  const id = useHostStore.getState().focusNodeId;
  return {
    nodeIds: id ? [id] : [],
    primaryId: id,
    param: null,
    edgeId: null,
    origin: null,
    intent: "mark",
  };
}
