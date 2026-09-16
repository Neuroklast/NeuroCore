import type { MotionPref } from "./motionPolicy";
import { motionAllows } from "./motionPolicy";

export function shouldPlayBoot(prefersReducedMotion: boolean, motion?: MotionPref): boolean {
  return motionAllows("boot", motion ?? (prefersReducedMotion ? "reduced" : "full"), prefersReducedMotion);
}
