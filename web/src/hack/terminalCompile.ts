import type { CompileResultPayload, Diagnostic } from "../bridge/ast";

type CompileLike = (script: string) => void | unknown | Promise<unknown>;

export async function commitTerminalDraft(
  script: string,
  diagnostics: Diagnostic[],
  compile: CompileLike,
): Promise<CompileResultPayload> {
  if (diagnostics.length > 0) return { origin: "editor", ok: false, diagnostics };
  try {
    const raw = await compile(script);
    if (raw && typeof raw === "object" && "ok" in raw) {
      const result = raw as CompileResultPayload;
      return { origin: "editor", ok: result.ok, diagnostics: result.diagnostics ?? [] };
    }
    return { origin: "editor", ok: true, diagnostics: [] };
  } catch (error) {
    return {
      origin: "editor",
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: error instanceof Error ? error.message : String(error) }],
    };
  }
}
