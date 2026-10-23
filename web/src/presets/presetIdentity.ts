export function factoryPresetId(name: string): string {
  return `factory:${name}`;
}

export function isFactoryPresetId(id: string): boolean {
  return id.startsWith("factory:");
}

export function rowKey(row: { id?: string; name: string; packId?: string }): string {
  return row.id || `${row.packId ?? ""}:${row.name}`;
}

export function migrateStarKeys(
  stars: Record<string, number>,
  rows: Array<{ id?: string; name: string }>,
): Record<string, number> {
  const byName = new Map<string, string[]>();
  for (const row of rows) {
    const id = row.id;
    if (! id) {
      continue;
    }
    const list = byName.get(row.name) ?? [];
    list.push(id);
    byName.set(row.name, list);
  }
  const out: Record<string, number> = { ...stars };
  for (const [key, value] of Object.entries(stars)) {
    if (rows.some((row) => row.id === key)) {
      continue;
    }
    const ids = byName.get(key);
    if (ids?.length === 1) {
      out[ids[0]!] = value;
      delete out[key];
    }
  }
  return out;
}
