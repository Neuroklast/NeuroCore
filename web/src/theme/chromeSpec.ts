import { CHIP_PAD_X, CHIP_PAD_Y } from "../assemble/chipMetrics";

/** Thin screen bezel around the whole editor. Content glows the inner lip. */
export const SHELL_BEZEL = 6;
/** Bottom-right hit for host resize. Native corner sits on top of this. */
export const RESIZE_GRIP = 22;

export function shellBezelCss(): string {
  return [
    `inset 0 0 0 1px rgba(var(--nk-accent-rgb), 0.42)`,
    `inset 0 0 0 ${SHELL_BEZEL}px var(--nk-surface)`,
    `inset 0 0 16px 2px rgba(var(--nk-accent-rgb), 0.2)`,
    `inset 0 0 28px 4px rgba(var(--nk-cyan-rgb), 0.06)`,
  ].join(", ");
}

/** One chrome spec: high-tech board, not a CRT terminal. */

export const CHIP_CLIP =
  "polygon(10px 0, 100% 0, 100% calc(100% - 10px), calc(100% - 10px) 100%, 0 100%, 0 10px)";

export const CHIP_FRAME_DASH = "16 5 3 5 22 6";
export const CHIP_CUT = 10;
export const CHIP_CORNER = 14;
/** Details plate on a chip. ui-ux: hit ≥ 26 px, not a ghost chevron. */
export const DETAIL_HIT = 26;

/** Expand sits on the chip chrome, not inside the overflow-clipped body. */
export const chipExpandOutsideBody = true;

export function chipExpandOffset(): { top: number; right: number; size: number } {
  return { top: CHIP_PAD_Y, right: CHIP_PAD_X, size: DETAIL_HIT };
}

export function greebleCode(id: string): string {
  let h = 2166136261;
  for (let i = 0; i < id.length; i += 1) {
    h ^= id.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return `TRG-${(h >>> 0).toString(16).toUpperCase().slice(-4)}`;
}

export function barcodeBits(id: string, n = 16): boolean[] {
  let h = 0;
  for (let i = 0; i < id.length; i += 1) {
    h = (h * 33 + id.charCodeAt(i)) >>> 0;
  }
  return Array.from({ length: n }, (_, i) => ((h >>> (i % 32)) & 1) === 1);
}

export function framePoints(w: number, h: number, cut = CHIP_CUT): string {
  return [
    `${cut},0`,
    `${w},0`,
    `${w},${h - cut}`,
    `${w - cut},${h}`,
    `0,${h}`,
    `0,${cut}`,
  ].join(" ");
}

/** Four hard L-brackets. Solid frame, not a dashed marquee. */
export function frameCorners(w: number, h: number, cut = CHIP_CUT, arm = CHIP_CORNER): string[] {
  return [
    `M 0 ${cut + arm} L 0 ${cut} L ${cut} 0 L ${cut + arm} 0`,
    `M ${w - arm} 0 L ${w} 0 L ${w} ${arm}`,
    `M ${w} ${h - cut - arm} L ${w} ${h - cut} L ${w - cut} ${h} L ${w - cut - arm} ${h}`,
    `M ${arm} ${h} L 0 ${h} L 0 ${h - arm}`,
  ];
}

/** Output sat lamp: true-peak ceiling neighbourhood. */
export function satLampOn(db: number): boolean {
  return Number.isFinite(db) && db >= -0.3;
}

export function segmentFill(live: string, segments = 10): number {
  const n = Number.parseFloat(live);
  if (! Number.isFinite(n)) {
    return 0;
  }
  if (n >= 0 && n <= 1) {
    return Math.round(n * segments);
  }
  const abs = Math.abs(n);
  if (abs <= 10) {
    return Math.max(1, Math.min(segments, Math.round((abs / 10) * segments)));
  }
  return Math.max(1, Math.min(segments, Math.round(Math.log10(abs + 1) * 3)));
}
