export type MotionPref = "full" | "reduced" | "off";

export type MotionFeature =
  | "boot"
  | "crtScan"
  | "lfoChase"
  | "pipeWave"
  | "chipReact"
  | "jitter"
  | "faceGlow"
  | "overlay"
  | "bloom"
  | "dof"
  | "techNoise";

export function motionAllows(
  feature: MotionFeature,
  motion: MotionPref,
  prefersReduced: boolean,
): boolean {
  if (motion === "off") {
    return false;
  }
  if (motion === "full") {
    return true;
  }
  if (prefersReduced) {
    return false;
  }
  return feature === "crtScan" || feature === "chipReact" || feature === "faceGlow" || feature === "bloom";
}

export function motionCopy(motion: MotionPref): string {
  if (motion === "off") {
    return "";
  }
  if (motion === "reduced") {
    return "";
  }
  return "";
}
