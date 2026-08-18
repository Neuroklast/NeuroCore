/** Backing store matches the CSS box. Never stretch a 720×128 bitmap. */
export function fitCanvas(
  el: HTMLCanvasElement,
  dpr = typeof window !== "undefined" ? window.devicePixelRatio : 1,
): { w: number; h: number; scale: number } {
  const cssW = Math.max(1, el.clientWidth || 1);
  const cssH = Math.max(1, el.clientHeight || 1);
  const scale = Math.max(1, Number.isFinite(dpr) ? dpr : 1);
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
