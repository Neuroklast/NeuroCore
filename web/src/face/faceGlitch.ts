export type FaceGlitchKind = "idle" | "slice" | "flash" | "noise";

/** Overlay FX follow the PNG alpha. Never invert — invert turns empty pixels white. */
export function logoOverlayMask(src: string): Record<string, string> {
  return {
    WebkitMaskImage: `url("${src}")`,
    maskImage: `url("${src}")`,
    WebkitMaskSize: "contain",
    maskSize: "contain",
    WebkitMaskRepeat: "no-repeat",
    maskRepeat: "no-repeat",
    WebkitMaskPosition: "center",
    maskPosition: "center",
  };
}

export function flashKeepsAlpha(css: string): boolean {
  return ! /\binvert\s*\(/i.test(css) && ! /#f4f1ea|#ffffff|#fff\b/i.test(css);
}

export function pickFaceGlitch(roll: number): FaceGlitchKind {
  if (roll < 0.62) {
    return "slice";
  }
  if (roll < 0.82) {
    return "flash";
  }
  return "noise";
}

export function scheduleFaceGlitch(kind: FaceGlitchKind, roll: number): number {
  const t = Math.max(0, Math.min(1, roll));
  if (kind === "idle") {
    return 700 + Math.floor(t * 2600);
  }
  if (kind === "slice") {
    return 70 + Math.floor(t * 140);
  }
  if (kind === "flash") {
    return 40 + Math.floor(t * 80);
  }
  return 200 + Math.floor(t * 340);
}

export function faceGlitchStyle(kind: FaceGlitchKind, seed: number): Record<string, string> {
  if (kind === "idle") {
    return {};
  }
  const n = Math.abs(Math.floor(seed)) % 1000;
  const y1 = 6 + (n % 68);
  const h1 = 5 + (n % 14);
  const y2 = 12 + ((n * 7) % 62);
  const h2 = 3 + ((n * 3) % 11);
  const bot1 = Math.max(0, 100 - y1 - h1);
  const bot2 = Math.max(0, 100 - y2 - h2);
  return {
    "--nk-g-y1": `${y1}%`,
    "--nk-g-h1": `${h1}%`,
    "--nk-g-bot1": `${bot1}%`,
    "--nk-g-y2": `${y2}%`,
    "--nk-g-h2": `${h2}%`,
    "--nk-g-bot2": `${bot2}%`,
    "--nk-g-x": `${5 + (n % 11)}px`,
    "--nk-g-x2": `${-(4 + (n % 9))}px`,
  };
}
