export type RevealRow = {
  name: string;
  category: string;
};

export type RevealResult = {
  cat: string;
  sel: number;
  scope: "all";
  q: "";
};

export function revealLoadedPreset(
  presets: RevealRow[],
  currentName: string,
): RevealResult | null {
  const name = currentName.trim();
  if (! name) {
    return null;
  }
  const hit = presets.find((p) => p.name === name);
  if (! hit) {
    return null;
  }
  const cat = hit.category || "";
  const rows = presets
    .filter((p) => ! cat || p.category === cat)
    .slice()
    .sort((a, b) => (a.name || "").localeCompare(b.name || ""));
  const sel = rows.findIndex((p) => p.name === name);
  if (sel < 0) {
    return null;
  }
  return { cat, sel, scope: "all", q: "" };
}
