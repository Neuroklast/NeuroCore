import type { ExplorerScope } from "./explorerSession";

export type ExplorerRow = {
  name: string;
  category: string;
  author?: string;
  description?: string;
  factory?: boolean;
  tags?: string[];
};

export const PRESET_ROW_PX = 44;
export const PRESET_LIST_HEADING_PX = 32;
export const PRESET_NINE_ROW_BUDGET_PX = PRESET_ROW_PX * 9 + PRESET_LIST_HEADING_PX;

export function scopeRows(rows: ExplorerRow[], scope: ExplorerScope): ExplorerRow[] {
  if (scope === "factory") {
    return rows.filter((p) => p.factory !== false);
  }
  if (scope === "user") {
    return rows.filter((p) => p.factory === false);
  }
  return rows;
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

export function filterExplorer(rows: ExplorerRow[], q: string, cat: string): ExplorerRow[] {
  const needle = q.trim().toLowerCase();
  const out = rows.filter((p) => {
    if (cat && p.category !== cat) {
      return false;
    }
    if (! needle) {
      return true;
    }
    const hay = `${p.name} ${p.category} ${p.author ?? ""} ${p.description ?? ""} ${(p.tags ?? []).join(" ")}`.toLowerCase();
    return hay.includes(needle);
  });
  out.sort((a, b) => (a.name || "").localeCompare(b.name || ""));
  return out;
}

export function resolveSelectedName(filtered: ExplorerRow[], selectedName: string): string {
  if (filtered.some((p) => p.name === selectedName)) {
    return selectedName;
  }
  return filtered[0]?.name ?? "";
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
