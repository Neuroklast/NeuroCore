import type { MotionPref } from "./motionPolicy";
import { motionAllows } from "./motionPolicy";

export function shouldPlayBoot(prefersReducedMotion: boolean, motion: MotionPref = "full"): boolean {
  return motionAllows("boot", motion, prefersReducedMotion);
}
