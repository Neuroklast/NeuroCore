import type { CompileResultPayload, Diagnostic } from "../bridge/ast";
import { publishScript } from "../assemble/addBlock";
import { validateOnSave } from "../overlays/validateModel";
import { useAstStore } from "../store/astStore";

type CompileLike = (script: string) => void | unknown | Promise<unknown>;

export async function commitTerminalDraft(
  script: string,
  diagnostics: Diagnostic[],
  compile: CompileLike,
): Promise<CompileResultPayload> {
  if (diagnostics.length > 0) return { origin: "editor", script, ok: false, diagnostics };
  try {
    const raw = await compile(script);
    if (raw && typeof raw === "object" && "ok" in raw) {
      const result = raw as CompileResultPayload;
      return { origin: "editor", script, ok: result.ok, diagnostics: result.diagnostics ?? [] };
    }
    return { origin: "editor", script, ok: true, diagnostics: [] };
  } catch (error) {
    return {
      origin: "editor", script,
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: error instanceof Error ? error.message : String(error) }],
    };
  }
}

export async function applyPatchDraft(): Promise<boolean> {
  const { script, diagnostics, setDraftEditing } = useAstStore.getState();
  const report = validateOnSave(script, diagnostics);
  const preflight = report.ok ? diagnostics : [
    ...diagnostics,
    ...report.issues.filter((i) => i.severity === "error").map((i) => ({ line: 1, column: 1, message: i.message })),
  ];
  const result = await commitTerminalDraft(script, preflight, (draft) => publishScript(draft, "editor"));
  useAstStore.getState().applyCompileResult(result);
  if (result.ok && useAstStore.getState().script === script) {
    setDraftEditing(false);
    return true;
  }
  return false;
}

export function discardPatchDraft(): void {
  const s = useAstStore.getState();
  s.setDraftScript(s.lastValidScript);
  s.setDraftEditing(false);
}
