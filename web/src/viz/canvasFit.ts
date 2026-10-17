/** Backing store matches the CSS box. Never stretch a 720×128 bitmap. */
export function fitCanvas(
  el: HTMLCanvasElement,
  dpr = typeof window !== "undefined" ? window.devicePixelRatio : 1,
  pixelBudget = Number.POSITIVE_INFINITY,
): { w: number; h: number; scale: number } {
  const cssW = Math.max(1, el.clientWidth || 1);
  const cssH = Math.max(1, el.clientHeight || 1);
  const rect = el.getBoundingClientRect?.();
  const hostScale = rect && rect.width > 0 ? rect.width / cssW : 1;
  const requested = Math.max(1, (Number.isFinite(dpr) ? dpr : 1) * hostScale);
  const cap = pixelBudget > 0 ? Math.sqrt(pixelBudget / (cssW * cssH)) : requested;
  const scale = Math.min(requested, cap);
  const bw = Math.max(1, Math.round(cssW * scale));
  const bh = Math.max(1, Math.round(cssH * scale));
  if (el.width !== bw) {
    el.width = bw;
  }
  if (el.height !== bh) {
    el.height = bh;
  }
  return { w: cssW, h: cssH, scale };
}
