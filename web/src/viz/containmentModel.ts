import type { ThemePalette } from "../theme/theme";
import type { MotionPref } from "../theme/motionPolicy";
import { ballRadius, rgbTriplet, type BallDrive, type Triplet } from "./ballModel";

const TAU = Math.PI * 2;

function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

function mixColor(a: Triplet, b: Triplet, t: number): Triplet {
  return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t];
}

function ease(cur: number, target: number, dt: number, attack: number, release: number): number {
  if (! (dt > 0)) {
    return cur;
  }
  const tau = target > cur ? attack : release;
  return cur + (target - cur) * (1 - Math.exp(-dt / tau));
}

export const RING_COUNT = 3;
export const CLAMP_COUNT = 4;
export const SHARD_COUNT = 16;
export const CLAMP_ANGLES: readonly number[] = [
  Math.PI / 4,
  (3 * Math.PI) / 4,
  (5 * Math.PI) / 4,
  (7 * Math.PI) / 4,
];

/** Rest pose so gaps don't line up. Frozen when motion is off. */
export const RING_OMEGA: readonly number[] = [0.065, -0.042, 0.028];
export const RING_REST: readonly number[] = [0.18, -0.41, 0.62];

export const TELE_L = { x: 12, y: 12, w: 188, h: 90 };
export const TELE_RIGHT_INSET = 192;
export const TELE_W = 168;
export const TELE_H = 90;
export const EDGE_MARGIN = 20;
export const OUTER_K_MIN = 1.1;
export const OUTER_K_MAX = 1.23;
export const OUTER_K = 1.16;
export const PULSE_MAX = 0.02;

export type TeleBox = { x: number; y: number; w: number; h: number };

export type RingSpec = {
  r: number;
  thick: number;
  segments: number;
  gap: number;
  phase: number;
  bright: number;
};

export type ClampSpec = {
  jawX: number;
  jawHx: number;
  jawHy: number;
  armX: number;
  armHx: number;
  armHy: number;
  yokeX: number;
  yokeHx: number;
  yokeHy: number;
  hingeX: number;
  hingeR: number;
  pipX: number;
  pipR: number;
  splitX: number;
};

export type OrbitSpec = { rx: number; ry: number; tilt: number; count: number };

export type TickSpec = { inner: number; outer: number; count: number };

export type BracketSpec = { span: number; arm: number };

export type ContainmentLayout = {
  cx: number;
  cy: number;
  paneW: number;
  paneH: number;
  ballR: number;
  outerR: number;
  rings: RingSpec[];
  clamp: ClampSpec;
  orbits: OrbitSpec[];
  ticks: TickSpec;
  brackets: BracketSpec;
};

export type ContainmentPose = {
  ringRot: [number, number, number];
  pulse: number;
  clampSurge: number;
  orbitPhase: number;
  discharge: [number, number, number, number];
  still: boolean;
};

export type OrbitShard = { x: number; y: number; z: number; size: number };

export type ContainmentPalette = {
  metalLo: Triplet;
  metal: Triplet;
  metalHi: Triplet;
  accent: Triplet;
  cyan: Triplet;
};

export const CONTAINMENT_STATIC: ContainmentPose = {
  ringRot: [RING_REST[0]!, RING_REST[1]!, RING_REST[2]!],
  pulse: 0,
  clampSurge: 0,
  orbitPhase: 0.37,
  discharge: [0, 0, 0, 0],
  still: true,
};

export function teleBoxes(paneW: number, paneH: number): TeleBox[] {
  const w = Math.max(0, paneW);
  const h = Math.max(0, paneH);
  const boxH = Math.min(TELE_H, Math.max(0, h - TELE_L.y));
  return [
    { x: TELE_L.x, y: TELE_L.y, w: Math.min(TELE_L.w, w), h: boxH },
    { x: Math.max(0, w - TELE_RIGHT_INSET - TELE_W), y: TELE_L.y, w: TELE_W, h: boxH },
  ];
}

export function circleHitsAabb(
  cx: number,
  cy: number,
  r: number,
  box: TeleBox,
): boolean {
  if (! (r > 0) || box.w <= 0 || box.h <= 0) {
    return false;
  }
  const nx = Math.max(box.x, Math.min(cx, box.x + box.w));
  const ny = Math.max(box.y, Math.min(cy, box.y + box.h));
  const dx = cx - nx;
  const dy = cy - ny;
  return dx * dx + dy * dy < r * r;
}

export function containmentLayout(paneW: number, paneH: number): ContainmentLayout {
  const w = Math.max(1, paneW);
  const h = Math.max(1, paneH);
  const cx = w * 0.5;
  const cy = h * 0.5;
  const ballR = ballRadius(w, h);
  const maxOuter = Math.max(ballR + 4, Math.min(w, h) * 0.5 - EDGE_MARGIN);
  const k = Math.min(OUTER_K_MAX, Math.min(OUTER_K, maxOuter / ballR));
  const outerR = ballR * k;
  const span = Math.max(8, outerR - ballR);

  const rings: RingSpec[] = [
    { r: ballR + span * 0.62, thick: Math.max(2.2, ballR * 0.018), segments: 3, gap: 0.2, phase: 0.15, bright: 0.95 },
    { r: ballR + span * 0.88, thick: Math.max(3.4, ballR * 0.028), segments: 2, gap: 0.24, phase: 0.7, bright: 0.52 },
    { r: outerR - Math.max(1.0, ballR * 0.008), thick: Math.max(2.0, ballR * 0.016), segments: 4, gap: 0.18, phase: 1.1, bright: 0.28 },
  ];

  const clamp: ClampSpec = {
    jawX: ballR + span * 0.12,
    jawHx: span * 0.2,
    jawHy: Math.max(7, ballR * 0.055),
    armX: ballR + span * 0.48,
    armHx: span * 0.34,
    armHy: Math.max(2.4, ballR * 0.018),
    yokeX: outerR + span * 0.02,
    yokeHx: Math.max(6, span * 0.22),
    yokeHy: Math.max(9, ballR * 0.07),
    hingeX: outerR - span * 0.08,
    hingeR: Math.max(3.2, ballR * 0.028),
    pipX: ballR + Math.max(2.5, span * 0.06),
    pipR: Math.max(1.4, ballR * 0.01),
    splitX: ballR + span * 0.4,
  };

  const maxRx = Math.max(outerR, Math.min(w * 0.42, outerR * 1.42));
  const orbits: OrbitSpec[] = [
    { rx: maxRx, ry: Math.max(18, outerR * 0.36), tilt: 0.32, count: 10 },
    { rx: maxRx * 0.92, ry: Math.max(14, outerR * 0.28), tilt: -0.22, count: 6 },
  ];

  return {
    cx,
    cy,
    paneW: w,
    paneH: h,
    ballR,
    outerR,
    rings,
    clamp,
    orbits,
    ticks: { inner: outerR + 4, outer: outerR + 9, count: 12 },
    brackets: { span: outerR * 1.12, arm: Math.max(10, outerR * 0.08) },
  };
}

export function orbitShards(layout: ContainmentLayout, phase: number): OrbitShard[] {
  const out: OrbitShard[] = [];
  let n = 0;
  for (const orbit of layout.orbits) {
    for (let i = 0; i < orbit.count && n < SHARD_COUNT; i += 1) {
      const a = phase + (i + 0.5) * TAU / orbit.count + n * 0.17;
      const x0 = orbit.rx * Math.cos(a);
      const y0 = orbit.ry * Math.sin(a);
      const ct = Math.cos(orbit.tilt);
      const st = Math.sin(orbit.tilt);
      out.push({
        x: layout.cx + x0,
        y: layout.cy + y0 * ct,
        z: y0 * st,
        size: 1.6 + (n % 3) * 0.7,
      });
      n += 1;
    }
  }
  return out;
}

export function containmentPalette(theme: ThemePalette): ContainmentPalette {
  const line = rgbTriplet(theme.lineRgb);
  const muted = rgbTriplet(theme.inkMutedRgb);
  const ink = rgbTriplet(theme.inkRgb);
  return {
    metalLo: mixColor(line, muted, 0.18),
    metal: mixColor(muted, line, 0.35),
    metalHi: mixColor(muted, ink, 0.55),
    accent: rgbTriplet(theme.accentRgb),
    cyan: rgbTriplet(theme.cyanRgb),
  };
}

export function containmentPose(
  prev: ContainmentPose | null,
  drive: BallDrive,
  dt: number,
  t: number,
  motion: MotionPref,
): ContainmentPose {
  if (motion === "off") {
    return CONTAINMENT_STATIC;
  }
  const scale = motion === "reduced" ? 0.35 : 1;
  const p = prev ?? CONTAINMENT_STATIC;
  const pulseTarget = clamp01(drive.lift * 0.55 + drive.kick) * PULSE_MAX * scale;
  const surgeTarget = clamp01(drive.kick * 0.85 + drive.lift * 0.25) * scale;
  const rot: [number, number, number] = [
    RING_REST[0]! + t * RING_OMEGA[0]! * scale,
    RING_REST[1]! + t * RING_OMEGA[1]! * scale,
    RING_REST[2]! + t * RING_OMEGA[2]! * scale,
  ];
  const discharge: [number, number, number, number] = [0, 0, 0, 0];
  if (drive.twinkle > 0.16) {
    for (let i = 0; i < 4; i += 1) {
      const u = Math.sin(t * 1.7 + i * 1.618) * 0.5 + 0.5;
      const hit = u * drive.twinkle;
      discharge[i] = hit > 0.55 ? ((hit - 0.55) / 0.45) * scale : 0;
    }
  }
  return {
    ringRot: rot,
    pulse: ease(p.pulse, pulseTarget, dt, 0.04, 0.12),
    clampSurge: ease(p.clampSurge, surgeTarget, dt, 0.03, 0.11),
    orbitPhase: CONTAINMENT_STATIC.orbitPhase + t * 0.11 * (0.55 + drive.swirl) * scale,
    discharge,
    still: false,
  };
}
