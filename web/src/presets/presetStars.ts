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

export function starGlyphs(n: number): string {
  const v = Math.max(0, Math.min(5, Math.round(n)));
  return "★".repeat(v) + "☆".repeat(5 - v);
}

function persistStars(next: Record<string, number>): Record<string, number> {
  try {
    localStorage.setItem(KEY, JSON.stringify(next));
  } catch {
    /* private mode */
  }
  return next;
}

export function writePresetStar(name: string, stars: number, prev = readPresetStars()): Record<string, number> {
  const n = Math.max(1, Math.min(5, Math.round(stars)));
  return persistStars({ ...prev, [name]: n });
}

export function isPresetFavorite(name: string, stars: Record<string, number>): boolean {
  return (stars[name] ?? 0) >= 1;
}

export function togglePresetStar(name: string, prev = readPresetStars()): Record<string, number> {
  const next = { ...prev };
  if ((next[name] ?? 0) >= 1) {
    delete next[name];
  } else {
    next[name] = 1;
  }
  return persistStars(next);
}

/** One list cell. Click cycles empty → 1…5 → empty. */
export function cyclePresetStar(name: string, prev = readPresetStars()): Record<string, number> {
  const cur = prev[name] ?? 0;
  const n = cur >= 5 ? 0 : cur + 1;
  const next = { ...prev };
  if (n === 0) {
    delete next[name];
  } else {
    next[name] = n;
  }
  return persistStars(next);
}
