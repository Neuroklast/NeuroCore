export type PresetStepRow = { name: string; category: string };

export function sortPresetStep(rows: readonly PresetStepRow[]): PresetStepRow[] {
  return [...rows].sort((a, b) => {
    const c = (a.category || "").localeCompare(b.category || "", undefined, { numeric: true });
    if (c !== 0) {
      return c;
    }
    return a.name.localeCompare(b.name, undefined, { numeric: true });
  });
}

/** Walk category folders, then names. A selected folder is the start if current is outside it. */
export function stepPresetName(
  rows: readonly PresetStepRow[],
  current: string,
  delta: number,
  selectedCat = "",
): string {
  if (rows.length === 0 || delta === 0) {
    return current;
  }
  const order = sortPresetStep(rows);
  const n = order.length;
  let cur = order.findIndex((r) => r.name === current);
  const cat = selectedCat.trim();
  if (cat) {
    const inCat = cur >= 0 && order[cur]!.category.toLowerCase() === cat.toLowerCase();
    if (! inCat) {
      const first = order.findIndex((r) => r.category.toLowerCase() === cat.toLowerCase());
      if (first >= 0) {
        let last = first;
        for (let i = first; i < n; ++i) {
          if (order[i]!.category.toLowerCase() === cat.toLowerCase()) {
            last = i;
          }
        }
        return (delta > 0 ? order[first] : order[last])!.name;
      }
    }
  }
  if (cur < 0) {
    cur = delta > 0 ? -1 : 0;
  }
  const next = ((cur + delta) % n + n) % n;
  return order[next]!.name;
}
