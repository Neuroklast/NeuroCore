const KEY = "nk-preset-stars";

export function readPresetStars(): Record<string, number> {
  try {
    const raw = typeof localStorage !== "undefined" ? localStorage.getItem(KEY) : null;
    if (! raw) {
      return {};
    }
    const o = JSON.parse(raw) as Record<string, unknown>;
    const out: Record<string, number> = {};
    for (const [k, v] of Object.entries(o)) {
      const n = Number(v);
      if (n >= 1 && n <= 5) {
        out[k] = Math.round(n);
      }
    }
    return out;
  } catch {
    return {};
  }
}

export function writePresetStar(name: string, stars: number, prev = readPresetStars()): Record<string, number> {
  const n = Math.max(1, Math.min(5, Math.round(stars)));
  const next = { ...prev, [name]: n };
  try {
    localStorage.setItem(KEY, JSON.stringify(next));
  } catch {
    /* private mode */
  }
  return next;
}
