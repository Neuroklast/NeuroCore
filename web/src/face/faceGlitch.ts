import type { MotionPref } from "../theme/motionPolicy";

export type FaceGlitchKind = "idle" | "slice" | "flash" | "noise";

export const UNIT_MARK_DEFAULT = "./img/neurokore.png";
export const UNIT_MARK_DIGICIDE = "./img/digicide.png";
export const UNIT_MARK_H = 168;
export const UNIT_MARK_DIGICIDE_H = 252;

/** DIGICIDE theme uses the Digicide mask. Every other theme keeps the Neurokore mark. */
export function unitMarkSrc(themeId: string): string {
  return themeId === "digicide" ? UNIT_MARK_DIGICIDE : UNIT_MARK_DEFAULT;
}

export function unitMarkMaxHeight(themeId: string): number {
  return themeId === "digicide" ? UNIT_MARK_DIGICIDE_H : UNIT_MARK_H;
}

/** Rain / scan overlays must clip to this theme’s mark — never a leftover Neurokore silhouette. */
export function unitMarkCssVars(src: string, themeId = ""): Record<string, string> {
  return {
    "--nk-face-mark": `url("${src}")`,
    "--nk-face-mark-h": `${unitMarkMaxHeight(themeId)}px`,
  };
}

export type LogoMotion = {
  glow: number;
  chromaPx: number;
  jitterHz: number;
};

const CHROMA_PX_MAX = 6;
const JITTER_HZ_MAX = 8;

function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

/** Log loudness so quiet signals still read (same curve as tube logAmp). */
function glowFromLoudness(loudness: number): number {
  return Math.min(1, Math.log10(1 + clamp01(loudness) * 9));
}

/**
 * Unit logo driven by level bands.
 * loudness → glow, treble → chroma split (px), bass → jitter rate (Hz).
 * prefers-reduced-motion or motion off → all zero. SNAP keeps glow only.
 */
export function logoMotion(
  loudness: number,
  bass: number,
  treble: number,
  opts: { motion?: MotionPref; prefersReduced?: boolean } = {},
): LogoMotion {
  const motion = opts.motion ?? "full";
  const prefersReduced = opts.prefersReduced === true;
  if (prefersReduced || motion === "off") {
    return { glow: 0, chromaPx: 0, jitterHz: 0 };
  }
  const glow = glowFromLoudness(loudness);
  if (motion === "reduced") {
    return { glow, chromaPx: 0, jitterHz: 0 };
  }
  return {
    glow,
    chromaPx: clamp01(treble) * CHROMA_PX_MAX,
    jitterHz: clamp01(bass) * JITTER_HZ_MAX,
  };
}

export function logoMotionStyle(m: LogoMotion): Record<string, string> {
  return {
    "--nk-logo-chroma": `${m.chromaPx}px`,
    "--nk-logo-jitter-hz": `${m.jitterHz}`,
  };
}

/**
 * Crude time-domain bands when telemetry has no bass/treble fields.
 * Fixed buffers in tests; live FaceView uses scopeOut + outPeak.
 */
export function estimateBands(samples: Float32Array, peak = 0): {
  loudness: number;
  bass: number;
  treble: number;
} {
  const loudness = clamp01(peak);
  if (! samples.length) {
    return { loudness, bass: 0, treble: 0 };
  }
  let bassAcc = 0;
  let trebAcc = 0;
  let lp = 0;
  let prev = 0;
  for (let i = 0; i < samples.length; i++) {
    const x = samples[i] ?? 0;
    lp += 0.12 * (x - lp);
    bassAcc += Math.abs(lp);
    trebAcc += Math.abs(x - prev);
    prev = x;
  }
  const n = samples.length;
  return {
    loudness: loudness > 0 ? loudness : clamp01(bassAcc / n * 8),
    bass: clamp01((bassAcc / n) * 6),
    treble: clamp01((trebAcc / n) * 3),
  };
}

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
