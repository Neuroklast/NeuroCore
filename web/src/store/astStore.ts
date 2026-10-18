import { create } from "zustand";
import type {
  AstDocument,
  AstEventPayload,
  CompileResultPayload,
  Diagnostic,
  Origin,
} from "../bridge/ast";
import { parseAstJson } from "../bridge/ast";
import { muteHidNodes, muteOverlayOnly } from "../assemble/muteSolo";
import { useHostStore } from "./hostStore";

export interface AstState {
  origin: Origin;
  ast: AstDocument | null;
  lastValidAst: AstDocument | null;
  lastValidScript: string;
  script: string;
  diagnostics: Diagnostic[];
  activeRevision: number;
  draftRevision: number;
  draftEditing: boolean;
  draftBaseRevision: number;
  pendingTransaction: { id: string; baseRevision: number } | null;
  applyAstEvent: (payload: AstEventPayload, opts?: { updateScript?: boolean }) => void;
  applyCompileResult: (payload: CompileResultPayload) => void;
  setDraftScript: (script: string) => void;
  setDraftEditing: (on: boolean) => void;
}

export function isStaleAstEvent(
  payload: { txId?: string; baseRevision?: number; origin?: Origin },
  live: { pendingTransaction: { id: string; baseRevision: number } | null; activeRevision: number },
): boolean {
  const pending = live.pendingTransaction;
  if (payload.txId && pending && payload.txId !== pending.id) {
    return true;
  }
  if (pending && ! payload.txId && payload.origin !== "preset" && payload.origin !== "host") {
    return true;
  }
  if (payload.baseRevision != null && Number.isFinite(payload.baseRevision)
      && payload.baseRevision < live.activeRevision
      && ! (payload.txId && pending && payload.txId === pending.id)) {
    return true;
  }
  return false;
}

export const useAstStore = create<AstState>((set) => ({
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

  applyAstEvent: (payload, opts) => {
    const live = useAstStore.getState();
    if (isStaleAstEvent(payload, live)) {
      return;
    }
    const parsed = parseAstJson(payload.astJson);
    if (parsed == null) {
      set({
        origin: payload.origin,
        diagnostics: payload.diagnostics,
      });
      return;
    }
    set((state) => {
      const live = state.lastValidScript || state.script;
      const overlay = muteOverlayOnly(live, payload.script)
        || (state.ast != null && muteHidNodes(payload.script || live, state.ast, parsed));
      if (overlay) {
        return {
          lastValidScript: payload.script,
          script: opts?.updateScript === false ? state.script : payload.script,
          diagnostics: payload.diagnostics,
        };
      }
      const dirtyDraft = state.draftEditing
        && state.script.length > 0
        && state.script !== state.lastValidScript;
      const keepDraft = opts?.updateScript === false
        || (dirtyDraft && (payload.origin === "preset" || payload.origin === "host"));
      const nextActive = state.activeRevision + 1;
      const matchedTx = payload.txId && state.pendingTransaction?.id === payload.txId;
      return {
        origin: payload.origin,
        ast: parsed,
        lastValidAst: parsed,
        lastValidScript: payload.script,
        script: keepDraft ? state.script : payload.script,
        diagnostics: keepDraft ? state.diagnostics : payload.diagnostics,
        activeRevision: nextActive,
        draftRevision: keepDraft ? state.draftRevision : nextActive,
        draftBaseRevision: keepDraft ? state.draftBaseRevision : nextActive,
        draftEditing: keepDraft ? state.draftEditing : false,
        pendingTransaction: matchedTx ? null : state.pendingTransaction,
      };
    });
    if (payload.origin === "canvas" || payload.origin === "editor" || payload.origin === "undo") {
      useHostStore.getState().markDirty();
    }
    if (payload.origin === "preset" || payload.origin === "host") {
      useHostStore.setState({ presetDirty: false });
    }
  },

  applyCompileResult: (payload) => {
    const live = useAstStore.getState();
    if (payload.txId && live.pendingTransaction && payload.txId !== live.pendingTransaction.id) {
      return;
    }
    if (payload.origin === "editor" && payload.script != null
        && payload.script !== live.script && ! payload.txId) {
      return;
    }
    if (! payload.ok) {
      set({
        origin: payload.origin,
        diagnostics: payload.diagnostics,
        pendingTransaction: payload.txId && live.pendingTransaction?.id === payload.txId
          ? null
          : live.pendingTransaction,
      });
      return;
    }
    set({
      diagnostics: [],
      pendingTransaction: null,
    });
  },

  setDraftScript: (script) => set((s) => {
    const wasClean = s.script === s.lastValidScript;
    const dirty = script !== s.lastValidScript;
    return {
      script,
      diagnostics: [],
      draftRevision: script === s.script ? s.draftRevision : s.draftRevision + 1,
      draftBaseRevision: wasClean && dirty ? s.activeRevision : s.draftBaseRevision,
    };
  }),
  setDraftEditing: (draftEditing) => set({ draftEditing }),
}));
