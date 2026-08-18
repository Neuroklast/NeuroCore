import { create } from "zustand";
import type {
  AstDocument,
  AstEventPayload,
  CompileResultPayload,
  Diagnostic,
  Origin,
} from "../bridge/ast";
import { parseAstJson } from "../bridge/ast";

export interface AstState {
  origin: Origin;
  ast: AstDocument | null;
  lastValidAst: AstDocument | null;
  lastValidScript: string;
  script: string;
  diagnostics: Diagnostic[];
  applyAstEvent: (payload: AstEventPayload, opts?: { updateScript?: boolean }) => void;
  applyCompileResult: (payload: CompileResultPayload) => void;
  setDraftScript: (script: string) => void;
}

export const useAstStore = create<AstState>((set) => ({
  origin: "bridge",
  ast: null,
  lastValidAst: null,
  lastValidScript: "",
  script: "",
  diagnostics: [],

  applyAstEvent: (payload, opts) => {
    const parsed = parseAstJson(payload.astJson);
    if (parsed == null) {
      set({
        origin: payload.origin,
        diagnostics: payload.diagnostics,
      });
      return;
    }
    set((state) => ({
      origin: payload.origin,
      ast: parsed,
      lastValidAst: parsed,
      lastValidScript: payload.script,
      script: opts?.updateScript === false ? state.script : payload.script,
      diagnostics: payload.diagnostics,
    }));
  },

  applyCompileResult: (payload) => {
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

  setDraftScript: (script) => set({ script }),
}));
