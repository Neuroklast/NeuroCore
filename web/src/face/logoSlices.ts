import type { MotionPref } from "../theme/motionPolicy";

export const LOGO_SLICE_COUNT = 12;
export type LogoSlice = { top: number; bottom: number; x: number; opacity: number };

/** Short, deterministic tear bursts with long intact holds. Original pixels remain full resolution. */
export function logoSliceFrame(elapsedMs: number, level: number, motion: MotionPref, impulse = 0): LogoSlice[] {
  const time = Math.max(0, Number.isFinite(elapsedMs) ? elapsedMs : 0);
  const energy = Number.isFinite(level) ? Math.max(0, Math.min(1, level)) : 0;
  const phase = time % 4200;
  const idleBurst = phase >= 2500 && phase < 2750
    ? Math.sin((phase - 2500) / 250 * Math.PI) ** 2
    : phase >= 2880 && phase < 3000 ? Math.sin((phase - 2880) / 120 * Math.PI) ** 2 * 0.45 : 0;
  const burst = Math.max(idleBurst, Number.isFinite(impulse) ? Math.max(0, Math.min(1, impulse)) : 0);
  return Array.from({ length: LOGO_SLICE_COUNT }, (_, i) => {
    const top = i * 100 / LOGO_SLICE_COUNT;
    const bottom = 100 - (i + 1) * 100 / LOGO_SLICE_COUNT;
    if (motion !== "full") return { top, bottom, x: 0, opacity: 1 };
    const progress = Math.max(0, Math.min(1, (time - i * 24) / 300));
    const entry = (1 - progress) ** 3;
    const stagger = Math.sin(i * 2.37 + Math.floor(time / 48) * 1.7);
    const x = entry * (i % 2 ? -18 : 18) + burst * stagger * (3 + energy * 12);
    return { top, bottom, x: Math.max(-18, Math.min(18, x)), opacity: progress };
  });
}
