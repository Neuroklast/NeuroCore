import { getNativeFunction, hasJuceBridge, nativeCallResult } from "../bridge/juce";

export type PackPreview = {
  ok: boolean;
  error?: string;
  name: string;
  id: string;
  author: string;
  description: string;
  version: string;
  presetCount: number;
  categories: string[];
  warnings: string[];
  source: string;
};

function asPreview(value: unknown): PackPreview {
  const rec = (value && typeof value === "object") ? value as Record<string, unknown> : {};
  return {
    ok: rec.ok !== false && ! rec.error,
    error: rec.error != null ? String(rec.error) : "",
    name: String(rec.name ?? ""),
    id: String(rec.id ?? ""),
    author: String(rec.author ?? ""),
    description: String(rec.description ?? ""),
    version: String(rec.version ?? ""),
    presetCount: Number(rec.presetCount ?? 0),
    categories: Array.isArray(rec.categories) ? rec.categories.map(String) : [],
    warnings: Array.isArray(rec.warnings) ? rec.warnings.map(String) : [],
    source: String(rec.source ?? ""),
  };
}

export async function libraryAction(cmd: Record<string, unknown>): Promise<{ ok: boolean; error: string; raw: unknown }> {
  if (! hasJuceBridge()) {
    return { ok: false, error: "Library actions need the plugin", raw: null };
  }
  try {
    const raw = await getNativeFunction("presetLibrary")(cmd);
    const result = nativeCallResult(raw);
    return { ok: result.ok, error: result.ok ? "" : result.error, raw };
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : "Native call failed", raw: null };
  }
}

export async function analyzeImport(paths: string[]): Promise<PackPreview> {
  const result = await libraryAction({ action: "analyze", paths });
  return asPreview(result.raw);
}

export async function installImport(paths: string[]): Promise<{ ok: boolean; error: string; imported: number }> {
  const result = await libraryAction({ action: "import", paths });
  const rec = (result.raw && typeof result.raw === "object") ? result.raw as Record<string, unknown> : {};
  return {
    ok: result.ok,
    error: result.error,
    imported: Number(rec.imported ?? 0),
  };
}
