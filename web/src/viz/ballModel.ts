import type { MotionPref } from "../theme/motionPolicy";
import { specMag01 } from "./scopeModel";

/**
 * Unit-face ball. A glass sphere with an electric rim, glowing dust and a GPU
 * particle state (offset + velocity) that is stirred by the audio bands and the
 * audio meters. Pure math here; the WebGL pipeline lives in BallGl.
 */

export type Triplet = [number, number, number];

export const STATE_WIDTH = 256;
export const MAX_STRANDS = 8;
export const MAX_ARCS = 4;
export const MAX_PARTICLES = 40000;

export const MOTIONS: Record<string, number> = { rise: 0, fall: 1, drift: 2, orbit: 3 };
export const SHAPES: Record<string, number> = { square: 0, round: 1 };

const BLACK: Triplet = [0, 0, 0];

/** Mulberry32. Deterministic dust and strands — never Math.random. */
export function ballSeed(start: number): () => number {
  let state = start >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

function smooth(edge0: number, edge1: number, value: number): number {
  const t = clamp01((value - edge0) / (edge1 - edge0));
  return t * t * (3 - 2 * t);
}

const toLinear = (c: number) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
const toGamma = (c: number) => (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055);

function toOklab(rgb: Triplet): Triplet {
  const [r, g, b] = [toLinear(rgb[0]), toLinear(rgb[1]), toLinear(rgb[2])] as Triplet;
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [
    0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
    1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
    0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s,
  ];
}

function fromOklab(lab: Triplet): Triplet {
  const [L, a, b] = lab;
  const l = Math.pow(L + 0.3963377774 * a + 0.2158037573 * b, 3);
  const m = Math.pow(L - 0.1055613458 * a - 0.0638541728 * b, 3);
  const s = Math.pow(L - 0.0894841775 * a - 1.291485548 * b, 3);
  return [
    4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
    -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
    -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
  ].map((v) => clamp01(toGamma(clamp01(v)))) as Triplet;
}

const mixColor = (a: Triplet, b: Triplet, t: number): Triplet =>
  [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];

function shift(rgb: Triplet, hueDeg: number, lightness: number, chroma: number): Triplet {
  const [L, a, b] = toOklab(rgb);
  const c = Math.hypot(a, b) * chroma;
  const h = Math.atan2(b, a) + (hueDeg * Math.PI) / 180;
  return fromOklab([clamp01(L + lightness), c * Math.cos(h), c * Math.sin(h)]);
}

export type BallPalette = {
  rim: Triplet;
  rimHot: Triplet;
  rimMid: Triplet;
  rimDeep: Triplet;
  spark: Triplet;
  sparkGlow: Triplet;
  haze: Triplet;
  edge: Triplet;
  tones: Triplet[];
};

/** Theme accent only — no white sheen, no cyan sparks. Signal reads blood-red. */
export function buildPalette(accent: Triplet, _cyan?: Triplet): BallPalette {
  return {
    rim: accent,
    rimHot: shift(accent, 0, 0.05, 1.12),
    rimMid: mixColor(accent, BLACK, 0.18),
    rimDeep: mixColor(accent, BLACK, 0.48),
    spark: accent,
    sparkGlow: shift(accent, 0, -0.1, 1.08),
    haze: mixColor(accent, BLACK, 0.55),
    edge: mixColor(accent, BLACK, 0.28),
    tones: [
      shift(accent, 0, -0.08, 1.05),
      shift(accent, 8, -0.04, 1.08),
      shift(accent, -8, -0.12, 1.1),
      shift(accent, 0, 0.02, 1.06),
      mixColor(accent, BLACK, 0.22),
    ],
  };
}

export type BallStrands = {
  harmonics: number[];
  rates: number[];
  phases: number[];
  flareHarmonics: number[];
  flareRates: number[];
  flarePhases: number[];
};

/** 8 strands, four harmonics each, plus flare harmonics. Fixed seed, so it never jitters. */
export function buildStrands(): BallStrands {
  const random = ballSeed(99);
  const bands: Array<[number, number, number, number]> = [
    [6, 12, 0.6, 1.6],
    [18, 34, 1.5, 3.5],
    [40, 70, 3, 7],
    [80, 130, 6, 12],
  ];
  const data: BallStrands = { harmonics: [], rates: [], phases: [], flareHarmonics: [], flareRates: [], flarePhases: [] };
  for (let s = 0; s < MAX_STRANDS; s++) {
    for (const [low, high, slow, fast] of bands) {
      const harmonic = Math.round(low + random() * (high - low));
      data.harmonics.push(harmonic + (harmonic % 2));
      data.rates.push((slow + random() * (fast - slow)) * (random() < 0.5 ? -1 : 1));
      data.phases.push(random() * Math.PI * 2);
    }
    for (let j = 0; j < 3; j++) {
      data.flareHarmonics.push(4 + Math.floor(random() * 6));
      data.flareRates.push((0.4 + random() * 0.8) * (random() < 0.5 ? -1 : 1));
      data.flarePhases.push(random() * Math.PI * 2);
    }
    const lead = s === 0;
    data.flareHarmonics.push(lead ? 0.75 : 0.95 + random() * 0.3);
    data.flareRates.push(lead ? 1.15 : 0.8 + random() * 0.2);
    data.flarePhases.push(lead ? 1 : 0.7 + random() * 0.25);
  }
  return data;
}

export const DUST_STRIDE = 4;

export type BallDust = { home: Float32Array; seed: Float32Array; count: number; rows: number };

/** Shell band + interior bowl, five tone classes. Same distribution as the look we want. */
export function buildDust(count: number): BallDust {
  const random = ballSeed(1337);
  const total = Math.max(0, Math.min(MAX_PARTICLES, Math.floor(Number.isFinite(count) ? count : 0)));
  const rows = Math.max(1, Math.ceil(total / STATE_WIDTH));
  const home = new Float32Array(STATE_WIDTH * rows * 4);
  const seed = new Float32Array(STATE_WIDTH * rows * 4);
  let k = 0;
  let guard = 0;
  while (k < total && guard < total * 80) {
    guard++;
    let x: number;
    let y: number;
    let tone: number;
    if (random() < 0.28) {
      const angle = random() * Math.PI * 2;
      const radius = 0.87 + Math.sqrt(random()) * 0.105;
      x = Math.cos(angle) * radius;
      y = Math.sin(angle) * radius;
      if (random() > smooth(-0.6, 0.3, -y)) {
        continue;
      }
      const q = random();
      tone = q < 0.5 ? 3 : q < 0.8 ? 2 : 1;
    } else {
      x = random() * 2 - 1;
      y = random() * 2 - 1;
      const radius = Math.hypot(x, y);
      if (radius > 0.975) {
        continue;
      }
      const bowl = Math.pow(smooth(-0.25, 0.85, -y), 1.3);
      const band = smooth(0.66, 0.96, radius) * smooth(-0.7, 0.3, -y);
      const weight = Math.max(bowl, band * 0.9);
      if (random() > 0.012 + 0.988 * weight) {
        continue;
      }
      if (weight < 0.12) {
        tone = 4;
      } else {
        const q = random();
        tone = q < 0.32 ? 0 : q < 0.52 ? 1 : q < 0.8 ? 2 : 3;
      }
    }
    const chord = Math.sqrt(Math.max(0, 0.95 - x * x - y * y));
    home[k * 4] = x;
    home[k * 4 + 1] = y;
    home[k * 4 + 2] = (random() * 2 - 1) * chord;
    home[k * 4 + 3] = tone;
    seed[k * 4] = tone === 4 ? 1.8 : random() < 0.22 ? 2.1 : 1.3;
    seed[k * 4 + 1] = random();
    seed[k * 4 + 2] = 2.5 + random() * 3.5;
    seed[k * 4 + 3] = 0.16 + random() * 0.26;
    k++;
  }
  return { home, seed, count: k, rows };
}

/** Fraction of the pane's short side. Radius is half of that. */
export const BALL_SIZE = 0.7;

export function ballRadius(width: number, height: number): number {
  const short = Math.min(Math.max(0, width), Math.max(0, height));
  return Math.max(8, (BALL_SIZE * short) / 2);
}

/** Quiet rim. Loudness only lifts the glow; particles take the rest of the audio. */
export const BALL_CONFIG = {
  strands: 5,
  crackle: 0.35,
  flares: 0.18,
  glow: 0.4,
  haze: 0.65,
  sparks: 0,
  fill: 0.52,
  motion: "drift",
  shape: "square",
  depth: 0.55,
  sway: 0.12,
  speed: 0.7,
} as const;

/** Audio meters → particle forces + rim glow. No pointer. */
export type BallDrive = {
  /** Stereo width: rest-radius bias, not a force. */
  spread: number;
  /** Bass: rest-offset bias (pulse), not a continuous lift. */
  lift: number;
  /** Decaying visual envelope for rings / projectors. */
  kick: number;
  /** One-shot impulse this step. Shader applies it once. */
  impulse: number;
  /** Loudness: rim glow 0..1, kept small. */
  glow: number;
  twinkle: number;
  swirl: number;
  size: number;
  dustSpeed: number;
  still: boolean;
};

export const BALL_STATIC: BallDrive = {
  spread: 0,
  lift: 0,
  kick: 0,
  impulse: 0,
  glow: 0.18,
  twinkle: 0.12,
  swirl: 0.2,
  size: 0.7,
  dustSpeed: 0.7,
  still: true,
};

const DRIVE_ATTACK = 0.07;
const DRIVE_RELEASE = 0.45;
const KICK_RELEASE = 0.18;

function ease(cur: number, target: number, dt: number, attack: number, release: number): number {
  if (! (dt > 0)) {
    return cur;
  }
  const tau = target > cur ? attack : release;
  return cur + (target - cur) * (1 - Math.exp(-dt / tau));
}

export function ballStill(motion: MotionPref): boolean {
  return motion === "off";
}

/**
 * Gonio X is side (L−R), Y is mid (L+R). Width is the side share of energy, 0 = mono.
 */
export function stereoWidth(gonioX: ArrayLike<number>, gonioY: ArrayLike<number>): number {
  const n = Math.min(gonioX.length, gonioY.length);
  let side = 0;
  let mid = 0;
  for (let i = 0; i < n; i += 1) {
    const x = gonioX[i] ?? 0;
    const y = gonioY[i] ?? 0;
    side += x * x;
    mid += y * y;
  }
  const t = side + mid;
  return t > 1e-12 ? clamp01(side / t) : 0;
}

/**
 * Goniometer width → spread. Bass + transient → lift/kick. Loudness → rim glow.
 * Reduced keeps moving, calmer. Off is a stand frame.
 */
export function ballDrive(
  prev: BallDrive | null,
  bands: { low: number; mid: number; high: number },
  rms: number,
  width: number,
  dt: number,
  opts: { motion?: MotionPref; prefersReduced?: boolean; transient?: boolean; onset?: number } = {},
): BallDrive {
  const motion = opts.motion ?? "full";
  if (ballStill(motion)) {
    return BALL_STATIC;
  }
  const scale = motion === "reduced" || opts.prefersReduced === true ? 0.4 : 1;
  const low = clamp01(bands.low);
  const mid = clamp01(bands.mid);
  const high = clamp01(bands.high);
  const loud = specMag01(clamp01(rms));
  const w = clamp01(width);
  const p = prev ?? BALL_STATIC;
  const onset = clamp01(opts.onset ?? (opts.transient === true ? 1 : 0)) * scale;

  return {
    spread: ease(p.spread, w * scale, dt, DRIVE_ATTACK, DRIVE_RELEASE),
    lift: ease(p.lift, 0.7 * low * scale, dt, DRIVE_ATTACK, DRIVE_RELEASE),
    kick: ease(p.kick, onset > 0 ? onset : 0, dt, 0.015, KICK_RELEASE),
    impulse: onset,
    glow: ease(p.glow, 0.12 + 0.38 * loud, dt, DRIVE_RELEASE, DRIVE_RELEASE),
    twinkle: ease(p.twinkle, 0.08 + 0.45 * high, dt, DRIVE_ATTACK, DRIVE_RELEASE),
    swirl: ease(p.swirl, 0.15 + 0.55 * mid, dt, DRIVE_RELEASE, DRIVE_RELEASE),
    size: ease(p.size, 0.65 + 0.35 * loud, dt, DRIVE_ATTACK, DRIVE_RELEASE),
    dustSpeed: ease(p.dustSpeed, 0.45 + 0.85 * loud, dt, DRIVE_RELEASE, DRIVE_RELEASE),
    still: false,
  };
}

export function ballCapable(getContext: (id: string) => unknown): boolean {
  try {
    return getContext("webgl2") != null;
  } catch {
    return false;
  }
}

/** "196, 42, 42" (the theme accentRgb string) to a 0..1 triplet. */
export function rgbTriplet(rgb: string): Triplet {
  const parts = rgb.split(",").map((v) => Number(v.trim()) / 255);
  if (parts.length !== 3 || parts.some((v) => ! Number.isFinite(v))) {
    return [1, 1, 1];
  }
  return [clamp01(parts[0]!), clamp01(parts[1]!), clamp01(parts[2]!)];
}

const MIN_DUST = 6000;
const MAX_DUST = MAX_PARTICLES;
const DUST_PER_PX = 1 / 50;

/** Point count from the pane area — never a fixed 18k. */
export function ballDustCount(width: number, height: number): number {
  const area = Math.max(0, width) * Math.max(0, height);
  return Math.round(Math.max(MIN_DUST, Math.min(MAX_DUST, area * DUST_PER_PX)));
}
