import type { ExplorerScope } from "./explorerSession";
import facets from "../presets/presetFacets.json";

export type ExplorerRow = {
  id?: string;
  packId?: string;
  name: string;
  category: string;
  author?: string;
  description?: string;
  factory?: boolean;
  sourceType?: string;
  origin?: string;
  tags?: string[];
  soundSources?: string[];
  processingTypes?: string[];
  style?: string;
  role?: string;
  requiredInputs?: string[];
};

export const SOUND_SOURCES = facets.soundSources;
export const PROCESSING_TYPES = facets.processingTypes;
export const STYLES = facets.styles;

export type ExplorerFacets = {
  sources: string[];
  types: string[];
  styles: string[];
};

export type ActiveFilterChip = { facet: "source" | "type" | "style"; value: string };

export const EMPTY_FACETS: ExplorerFacets = { sources: [], types: [], styles: [] };

export function activeFilterChips(facets: ExplorerFacets): ActiveFilterChip[] {
  return [
    ...facets.sources.map((value) => ({ facet: "source" as const, value })),
    ...facets.types.map((value) => ({ facet: "type" as const, value })),
    ...facets.styles.map((value) => ({ facet: "style" as const, value })),
  ];
}

export function emptyExplorerCopy(inScopeCount: number, filteredCount: number, hasQueryOrFacets: boolean): string {
  if (filteredCount > 0) {
    return "";
  }
  if (inScopeCount === 0 || ! hasQueryOrFacets) {
    return "No presets available.";
  }
  return "No matching presets. Change the search or filters.";
}

const CAT_SOURCE: Record<string, string> = {
  Distortion: "Guitar", Guitar: "Guitar", Metal: "Guitar", Bass: "Bass Guitar",
  Vocals: "Vocals", Drums: "Drums", Club: "Drums", Synth: "Synth",
  Mastering: "Full Mix", Mixing: "Full Mix", Master: "Full Mix",
};
const CAT_TYPE: Record<string, string> = {
  Distortion: "Distortion", Delay: "Delay", Reverb: "Reverb", Modulation: "Modulation",
  EQ: "EQ", Filter: "Filter", Dynamics: "Dynamics", Utility: "Utility",
};

export function rowSources(p: ExplorerRow): string[] {
  if (p.soundSources && p.soundSources.length > 0) return p.soundSources;
  const mapped = CAT_SOURCE[p.category];
  return mapped ? [mapped] : ["Any"];
}
export function rowTypes(p: ExplorerRow): string[] {
  if (p.processingTypes && p.processingTypes.length > 0) return p.processingTypes;
  const mapped = CAT_TYPE[p.category];
  return mapped ? [mapped] : [];
}
export function rowStyles(p: ExplorerRow): string[] {
  return p.style ? [p.style] : [];
}
export function toggleFacet(selected: string[], value: string): string[] {
  return selected.includes(value) ? selected.filter((v) => v !== value) : [...selected, value];
}

export const PRESET_ROW_PX = 44;
export const PRESET_LIST_HEADING_PX = 32;
export const PRESET_NINE_ROW_BUDGET_PX = PRESET_ROW_PX * 9 + PRESET_LIST_HEADING_PX;

export function scopeRows(rows: ExplorerRow[], scope: ExplorerScope): ExplorerRow[] {
  if (scope === "factory") {
    return rows.filter((p) => p.factory !== false && p.sourceType !== "pack");
  }
  if (scope === "user") {
    return rows.filter((p) => p.factory === false && p.sourceType !== "pack");
  }
  if (scope.startsWith("pack:")) {
    const packId = scope.slice(5);
    return rows.filter((p) => p.packId === packId);
  }
  return rows;
}

export function libraryFolders(
  rows: ExplorerRow[],
  packs: Array<{ id: string; name: string }>,
): Array<{ id: ExplorerScope | "favorites"; label: string; count: number }> {
  const factory = rows.filter((p) => p.factory !== false && p.sourceType !== "pack").length;
  const user = rows.filter((p) => p.factory === false && p.sourceType !== "pack").length;
  return [
    { id: "all", label: "All Presets", count: rows.length },
    { id: "factory", label: "Factory", count: factory },
    { id: "user", label: "My Presets", count: user },
    { id: "favorites", label: "Favorites", count: -1 },
    ...packs.map((pack) => ({
      id: `pack:${pack.id}` as ExplorerScope,
      label: pack.name,
      count: rows.filter((p) => p.packId === pack.id).length,
    })),
  ];
}

export function explorerFolders(rows: ExplorerRow[]): Array<{ name: string; label: string; count: number }> {
  const counts = new Map<string, number>();
  for (const p of rows) {
    const c = p.category || "Unsorted";
    counts.set(c, (counts.get(c) ?? 0) + 1);
  }
  return [
    { name: "", label: "All Presets", count: rows.length },
    ...Array.from(counts.entries()).sort((a, b) => a[0].localeCompare(b[0]))
      .map(([name, count]) => ({ name, label: name, count })),
  ];
}

function matchesList(have: string[], selected: string[]): boolean {
  if (selected.length === 0) return true;
  return selected.some((s) => have.includes(s));
}

export function filterExplorer(rows: ExplorerRow[], q: string, cat: string, facets?: ExplorerFacets): ExplorerRow[] {
  const needle = q.trim().toLowerCase();
  const out = rows.filter((p) => {
    if (cat && p.category !== cat) {
      return false;
    }
    if (facets && ! matchesList(rowSources(p), facets.sources)) return false;
    if (facets && ! matchesList(rowTypes(p), facets.types)) return false;
    if (facets && ! matchesList(rowStyles(p), facets.styles)) return false;
    if (! needle) {
      return true;
    }
    const hay = `${p.name} ${p.category} ${p.author ?? ""} ${p.description ?? ""} ${(p.tags ?? []).join(" ")} ${rowSources(p).join(" ")} ${rowTypes(p).join(" ")} ${rowStyles(p).join(" ")} ${(p.requiredInputs ?? []).join(" ")}`.toLowerCase();
    return hay.includes(needle);
  });
  out.sort((a, b) => (a.name || "").localeCompare(b.name || ""));
  return out;
}

export function sourceFolders(rows: ExplorerRow[], q: string, facets: ExplorerFacets): Array<{ name: string; label: string; count: number }> {
  const base = filterExplorer(rows, q, "", { ...facets, sources: [] });
  return [
    { name: "", label: "All Presets", count: base.length },
    ...SOUND_SOURCES.map((name) => ({
      name,
      label: name,
      count: base.filter((p) => rowSources(p).includes(name)).length,
    })).filter((f) => f.count > 0 || f.name === "Any" || facets.sources.includes(f.name)),
  ];
}

export function typeFolders(rows: ExplorerRow[], q: string, facets: ExplorerFacets): Array<{ name: string; label: string; count: number }> {
  const base = filterExplorer(rows, q, "", { ...facets, types: [] });
  return PROCESSING_TYPES.map((name) => ({
    name,
    label: name,
    count: base.filter((p) => rowTypes(p).includes(name)).length,
  })).filter((f) => f.count > 0 || facets.types.includes(f.name));
}

export function styleFolders(rows: ExplorerRow[], q: string, facets: ExplorerFacets): Array<{ name: string; label: string; count: number }> {
  const base = filterExplorer(rows, q, "", { ...facets, styles: [] });
  return STYLES.map((name) => ({
    name,
    label: name,
    count: base.filter((p) => rowStyles(p).includes(name)).length,
  })).filter((f) => f.count > 0 || facets.styles.includes(f.name));
}

export function resolveSelectedName(filtered: ExplorerRow[], selectedName: string): string {
  if (filtered.some((p) => p.name === selectedName)) {
    return selectedName;
  }
  return filtered[0]?.name ?? "";
}

export function resolveSelectedRow(
  filtered: ExplorerRow[],
  selectedId: string,
  selectedName: string,
): ExplorerRow | undefined {
  return filtered.find((p) => (p.id && p.id === selectedId) || p.name === selectedName) ?? filtered[0];
}

export function selectedRow(filtered: ExplorerRow[], selectedName: string): ExplorerRow | undefined {
  return filtered.find((p) => p.name === selectedName) ?? filtered[0];
}

export function neighborName(filtered: ExplorerRow[], selectedName: string, key: string): string {
  if (filtered.length === 0) {
    return "";
  }
  const i = filtered.findIndex((p) => p.name === selectedName);
  const at = i < 0 ? 0 : i;
  if (key === "ArrowDown") {
    return filtered[Math.min(filtered.length - 1, at + 1)]!.name;
  }
  if (key === "ArrowUp") {
    return filtered[Math.max(0, at - 1)]!.name;
  }
  return filtered[at]!.name;
}

export function seedSelectedName(
  presets: Array<{ name: string }>,
  currentName: string,
  existingSelectedName: string,
): string {
  if (existingSelectedName && presets.some((p) => p.name === existingSelectedName)) {
    return existingSelectedName;
  }
  const name = currentName.trim();
  if (name && presets.some((p) => p.name === name)) {
    return name;
  }
  return presets[0]?.name ?? "";
}

export function saveAsError(name: string, factoryLocked: boolean): string {
  if (! name.trim()) {
    return "Enter a name.";
  }
  if (factoryLocked) {
    return "Factory names are locked. Pick another.";
  }
  return "";
}
