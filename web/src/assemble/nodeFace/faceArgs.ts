/** Parse a ChipSpec arg for a calculated face. Formulas fall back; they are not eval'd. */
export function parseFaceNumber(raw: string | undefined, fallback: number, min: number, max: number): number {
  const n = raw != null && raw.trim() !== "" ? Number(raw) : Number.NaN;
  const v = Number.isFinite(n) ? n : fallback;
  return Math.min(max, Math.max(min, v));
}

export function parseFaceToken(raw: string | undefined, fallback: string): string {
  const t = (raw ?? "").trim().toLowerCase();
  return t || fallback;
}
