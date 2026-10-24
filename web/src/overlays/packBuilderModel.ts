import type { ExplorerRow } from "./explorerModel";
import { rowKey } from "../presets/presetIdentity";

export type PackDraft = {
  name: string; author: string; version: string; description: string;
  id: string; website: string; artworkSource: string;
};

export function packCandidates(rows: ExplorerRow[]): ExplorerRow[] {
  return rows.filter((row) => row.factory === false && row.sourceType !== "factory"
    && Boolean(row.id) && Boolean(row.name.trim()));
}

export function packSuggestedId(author: string, name: string): string {
  const slug = (s: string) => s.normalize("NFKD").replace(/[\u0300-\u036f]/g, "")
    .toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "") || "custom";
  return "pack." + slug(author) + "." + slug(name);
}

export function selectPackPreset(ids: string[], id: string, enabled: boolean): string[] {
  if (!id) return ids;
  if (enabled) return ids.includes(id) ? ids : [...ids, id];
  return ids.filter((current) => current !== id);
}

export function movePackPreset(ids: string[], index: number, delta: number): string[] {
  const target = index + delta;
  if (index < 0 || target < 0 || index >= ids.length || target >= ids.length) return ids;
  const next = [...ids];
  [next[index], next[target]] = [next[target]!, next[index]!];
  return next;
}

export function resolvePackSelection(rows: ExplorerRow[], selected: string[]) {
  const candidates = new Map(packCandidates(rows).map((row) => [rowKey(row), row]));
  return selected.map((id) => candidates.get(id)).filter((row): row is ExplorerRow => Boolean(row));
}

export function validatePackDraft(draft: PackDraft, selected: ExplorerRow[]): string[] {
  const errors: string[] = [];
  if (!draft.name.trim()) errors.push("Enter a pack name.");
  if (draft.name.length > 120) errors.push("Pack name is too long.");
  if (!draft.author.trim()) errors.push("Enter an author or label.");
  if (draft.author.length > 120) errors.push("Author is too long.");
  if (draft.description.length > 2000) errors.push("Description must be 2000 characters or less.");
  if (!/^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$/.test(draft.id) ||
      draft.id.includes("..") || draft.id.endsWith(".") ||
      /^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(draft.id.split(".")[0] || ""))
    errors.push("Pack ID must use letters, numbers, dots, dashes or underscores without consecutive dots.");
  if (!/^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$/.test(draft.version))
    errors.push("Use a version such as 1.0.0 or 1.0.0-beta.1.");
  if (draft.website.trim() && (!/^https:\/\/[^\s/]+(?:\/\S*)?$/.test(draft.website.trim())))
    errors.push("Website must be an HTTPS URL.");
  if (selected.length === 0) errors.push("Select at least one saved user or installed-pack preset.");
  return errors;
}
