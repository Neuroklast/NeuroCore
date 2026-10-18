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
  applyAstEvent: (payload: AstEventPayload, opts?: { updateScript?: boolean }) => void;
  applyCompileResult: (payload: CompileResultPayload) => void;
  setDraftScript: (script: string) => void;
  setDraftEditing: (on: boolean) => void;
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

  applyAstEvent: (payload, opts) => {
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
      return {
        origin: payload.origin,
        ast: parsed,
        lastValidAst: parsed,
        lastValidScript: payload.script,
        script: keepDraft ? state.script : payload.script,
        diagnostics: keepDraft ? state.diagnostics : payload.diagnostics,
        activeRevision: nextActive,
        draftRevision: keepDraft ? state.draftRevision : nextActive,
        draftEditing: keepDraft ? state.draftEditing : false,
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
    if (payload.origin === "editor" && payload.script != null
        && payload.script !== useAstStore.getState().script) return;
    if (! payload.ok) {
      set({
        origin: payload.origin,
        diagnostics: payload.diagnostics,
      });
      return;
    }
    set({
      origin: payload.origin,
      diagnostics: [],
    });
  },

  setDraftScript: (script) => set((s) => ({
    script,
    diagnostics: [],
    draftRevision: script === s.script ? s.draftRevision : s.draftRevision + 1,
  })),
  setDraftEditing: (draftEditing) => set({ draftEditing }),
}));
