import type { AstDocument, Diagnostic } from "../bridge/ast";
import { boardIdForSource, canonicalNodeId } from "../hack/dslSourceMap";
import { useAstStore } from "./astStore";
import { useHostStore } from "./hostStore";
import {
  EMPTY_SELECTION,
  normalizeSelection,
  selectionSnapshot,
  usePatchSelectionStore,
} from "./patchSelectionStore";

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
    activeRevision: s.activeRevision,
    draftScript,
    draftRevision: s.draftRevision,
    draftStatus: s.diagnostics.length > 0
      ? "invalid"
      : draftLooksModified(draftScript, activeScript)
        ? "modified"
        : "clean",
    diagnostics: s.diagnostics,
    pendingTransaction: null,
  };
}

export function draftBanner(status: DraftStatus): string | null {
  if (status === "modified" || status === "conflict") {
    return "UNAPPLIED";
  }
  if (status === "invalid") {
    return "INVALID";
  }
  return null;
}

export function readPatchSelection(): PatchSelection {
  return selectionSnapshot(usePatchSelectionStore.getState());
}

/** Returns false when the payload matches the live selection. */
export function publishSelection(next: Partial<PatchSelection>): boolean {
  const cur = readPatchSelection();
  const merged = normalizeSelection({
    ...cur,
    ...next,
    nodeIds: next.nodeIds ?? cur.nodeIds,
    param: next.param === undefined ? cur.param : next.param,
    edgeId: next.edgeId === undefined ? cur.edgeId : next.edgeId,
    origin: next.origin === undefined ? cur.origin : next.origin,
    intent: next.intent === undefined ? cur.intent : next.intent,
    primaryId: next.primaryId === undefined ? cur.primaryId : next.primaryId,
  });
  const changed = usePatchSelectionStore.getState().replace(merged);
  if (changed) {
    const id = merged.primaryId;
    if (useHostStore.getState().focusNodeId !== id) {
      useHostStore.getState().setFocusNodeId(id);
    }
  }
  return changed;
}

export function pruneSelection(validIds: Iterable<string>): void {
  const valid = [...validIds];
  const cur = readPatchSelection();
  if (cur.nodeIds.length === 0) {
    return;
  }
  const kept = cur.nodeIds.filter((id) => boardIdForSource(id, valid) != null
    || valid.some((v) => canonicalNodeId(v).toLowerCase() === canonicalNodeId(id).toLowerCase()));
  if (kept.length === cur.nodeIds.length
    && cur.primaryId != null
    && kept.includes(cur.primaryId)) {
    return;
  }
  const primaryId = cur.primaryId && kept.includes(cur.primaryId)
    ? cur.primaryId
    : (kept[kept.length - 1] ?? null);
  publishSelection({
    nodeIds: kept,
    primaryId,
    param: kept.length ? cur.param : null,
    edgeId: kept.length ? cur.edgeId : null,
    origin: cur.origin,
    intent: "mark",
  });
}

export { EMPTY_SELECTION };
