import { spectrumBins, stereoMetrics } from "./faceModel";
import {
  CONTAINMENT_STATIC,
  PULSE_MAX,
  containmentPose,
  type ContainmentLayout,
  type ContainmentPose,
  type TeleBox,
} from "../viz/containmentModel";
import { BALL_STATIC, ballDrive, type BallDrive } from "../viz/ballModel";
import type { MotionPref } from "../theme/motionPolicy";

function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

export type CoreViz = {
  activity: number;
  stability: number;
  excitation: number;
  sync: number;
  field: number;
  mode: "IDLE" | "STABLE" | "ACTIVE";
};

export function coreSpectrum(samples: ArrayLike<number>, live: boolean, bins = 32): number[] {
  if (! live || samples.length < 2) {
    return new Array(Math.max(1, bins)).fill(0);
  }
  return spectrumBins(samples, bins).map(clamp01);
}

export function coreWave(samples: ArrayLike<number>, live: boolean, points = 48): number[] {
  const out = new Array(points).fill(0);
  if (! live || samples.length < 2) {
    return out;
  }
  const n = samples.length;
  for (let i = 0; i < points; i += 1) {
    const x = samples[Math.floor((i / (points - 1)) * (n - 1))] ?? 0;
    out[i] = Number.isFinite(x) ? Math.max(-1, Math.min(1, x)) : 0;
  }
  return out;
}

export const FIELD_SEGS = 12;

export const HUD = {
  inset: 12,
  diagTop: 108,
  diagW: 228,
  calloutW: 108,
  calloutH: 48,
} as const;

export type CalloutGeom = {
  box: TeleBox;
  tip: { x: number; y: number };
};

export function hudBoxes(paneW: number, paneH: number): TeleBox[] {
  const w = Math.max(0, paneW);
  const h = Math.max(0, paneH);
  const boxH = Math.max(0, h - HUD.diagTop - 8);
  return [
    { x: HUD.inset, y: HUD.diagTop, w: Math.min(HUD.diagW, w), h: boxH },
    { x: Math.max(0, w - HUD.inset - HUD.diagW), y: HUD.diagTop, w: HUD.diagW, h: boxH },
  ];
}

export function fieldStacks(viz: CoreViz): Array<{ id: "FIELD" | "ACT"; v: number; lit: number; n: number }> {
  return [
    { id: "FIELD", v: viz.field, lit: segsLit(viz.field, FIELD_SEGS), n: FIELD_SEGS },
    { id: "ACT", v: viz.activity, lit: segsLit(viz.activity, FIELD_SEGS), n: FIELD_SEGS },
  ];
}

export function coreCallouts(layout: ContainmentLayout): { core: CalloutGeom; ring: CalloutGeom } {
  const { cx, cy, ballR, paneW, paneH } = layout;
  const w = HUD.calloutW;
  const h = HUD.calloutH;
  const gap = 12;
  const cols = hudBoxes(paneW, paneH);
  const leftCol = cols[0]!;
  const rightCol = cols[1]!;
  const tipCore = { x: cx - layout.clamp.pipX, y: cy };
  const coreBox: TeleBox = {
    x: Math.max(leftCol.x + leftCol.w + 8, Math.min(cx - ballR - gap - w, tipCore.x - gap - w)),
    y: Math.max(8, tipCore.y - ballR * 0.55 - h),
    w,
    h,
  };
  const ringR = layout.rings[1]?.r ?? layout.outerR;
  const ang = 0.62;
  const tipRing = { x: cx + ringR * Math.cos(ang), y: cy + ringR * Math.sin(ang) };
  const ringBox: TeleBox = {
    x: Math.min(rightCol.x - 8 - w, Math.max(cx + ballR + gap, tipRing.x + gap)),
    y: Math.min(paneH - 8 - h, Math.max(cy + 12, tipRing.y + gap)),
    w,
    h,
  };
  return {
    core: { box: coreBox, tip: tipCore },
    ring: { box: ringBox, tip: tipRing },
  };
}

export function coreViz(drive: BallDrive, pose: ContainmentPose): CoreViz {
  if (drive.still || pose.still) {
    return { activity: 0, stability: 1, excitation: 0, sync: 1, field: 0, mode: "IDLE" };
  }
  const activity = clamp01(pose.glow);
  const stability = clamp01(1 - pose.pulse / Math.max(PULSE_MAX, 1e-6));
  const excitation = clamp01(drive.twinkle);
  const sync = clamp01(1 - Math.abs(drive.swirl - 0.2));
  const field = clamp01(0.5 * drive.lift + 0.5 * drive.kick + 0.35 * pose.clampSurge);
  const mode: CoreViz["mode"] = activity > 0.28 || field > 0.2 ? "ACTIVE" : "STABLE";
  return { activity, stability, excitation, sync, field, mode };
}

function bandsFromSpec(spec: number[], sr: number): { low: number; mid: number; high: number } {
  const nyquist = Math.max(1, sr / 2);
  let low = 0;
  let mid = 0;
  let high = 0;
  let nL = 0;
  let nM = 0;
  let nH = 0;
  for (let k = 0; k < spec.length; k += 1) {
    const hz = ((k + 0.5) / spec.length) * nyquist;
    const v = spec[k] ?? 0;
    if (hz < 400) {
      low += v;
      nL += 1;
    } else if (hz < 4000) {
      mid += v;
      nM += 1;
    } else {
      high += v;
      nH += 1;
    }
  }
  const norm = (acc: number, n: number) => clamp01(n > 0 ? (acc / n) * 8 : 0);
  return { low: norm(low, nL), mid: norm(mid, nM), high: norm(high, nH) };
}

export function coreDrivePose(
  samples: ArrayLike<number>,
  rms: number,
  gonioX: ArrayLike<number>,
  gonioY: ArrayLike<number>,
  dt: number,
  t: number,
  motion: MotionPref,
  prevDrive: BallDrive | null,
  prevPose: ContainmentPose | null,
  live: boolean,
  sr = 48000,
): { drive: BallDrive; pose: ContainmentPose; viz: CoreViz; spec: number[]; wave: number[] } {
  const idle = {
    drive: BALL_STATIC,
    pose: CONTAINMENT_STATIC,
    viz: coreViz(BALL_STATIC, CONTAINMENT_STATIC),
    spec: coreSpectrum(samples, false, 32),
    wave: coreWave(samples, false, 48),
  };
  if (! live || motion === "off") {
    return idle;
  }
  const raw = spectrumBins(samples, 32);
  const bands = bandsFromSpec(raw, sr);
  const width = stereoMetrics(gonioX, gonioY);
  const side = Math.max(0, 1 - Math.abs(width.corr));
  const drive = ballDrive(prevDrive, bands, rms, side, dt, { motion });
  const pose = containmentPose(prevPose, drive, dt, t, motion);
  return {
    drive,
    pose,
    viz: coreViz(drive, pose),
    spec: raw.map(clamp01),
    wave: coreWave(samples, true, 48),
  };
}

export function polyline(values: ArrayLike<number>, w: number, h: number, bipolar = false): string {
  const n = values.length;
  if (n < 2) {
    return "";
  }
  let d = "";
  for (let i = 0; i < n; i += 1) {
    const x = (i / (n - 1)) * w;
    const v = values[i] ?? 0;
    const y = bipolar ? h * 0.5 - v * h * 0.42 : h - clamp01(v) * h * 0.92;
    d += i === 0 ? `M ${x.toFixed(1)} ${y.toFixed(1)}` : ` L ${x.toFixed(1)} ${y.toFixed(1)}`;
  }
  return d;
}

export function formatViz(n: number): string {
  const v = Number.isFinite(n) ? Math.max(0, Math.min(1, n)) : 0;
  return v.toFixed(2);
}

export const SCOPE_WIN = 256;

export function specFloorHz(sr: number, n = SCOPE_WIN): number {
  const rate = sr > 0 ? sr : 48000;
  return rate / Math.max(2, n);
}

export function specLogX(hz: number, sr: number, n = SCOPE_WIN): number {
  const f0 = specFloorHz(sr, n);
  const f1 = Math.max(f0 * 2, (sr > 0 ? sr : 48000) / 2);
  if (! (hz > f0)) {
    return 0;
  }
  return clamp01(Math.log(hz / f0) / Math.log(f1 / f0));
}

export function specLogArea(spec: number[], sr: number, w: number, h: number): string {
  const line = specLogPath(spec, sr, w, h);
  if (! line) {
    return "";
  }
  return `${line} L ${w.toFixed(1)} ${h.toFixed(1)} L 0 ${h.toFixed(1)} Z`;
}

export function specLogPath(spec: number[], sr: number, w: number, h: number): string {
  const n = spec.length;
  if (n < 2) {
    return "";
  }
  const nyquist = Math.max(1, (sr > 0 ? sr : 48000) / 2);
  let d = "";
  let started = false;
  for (let i = 0; i < n; i += 1) {
    const hz = ((i + 0.5) / n) * nyquist;
    const x = specLogX(hz, sr) * w;
    const y = h - clamp01(spec[i] ?? 0) * (h - 8);
    if (! started) {
      d = `M ${x.toFixed(1)} ${y.toFixed(1)}`;
      started = true;
    } else {
      d += ` L ${x.toFixed(1)} ${y.toFixed(1)}`;
    }
  }
  return d;
}

export function specTickHz(sr: number): number[] {
  const f0 = specFloorHz(sr);
  const f1 = (sr > 0 ? sr : 48000) / 2;
  const cand = [200, 500, 1000, 2000, 5000, 10000, 20000];
  return cand.filter((hz) => hz >= f0 * 1.05 && hz <= f1 * 0.98);
}

export function segsLit(v: number, n = 12): number {
  if (! Number.isFinite(v) || v <= 0) {
    return 0;
  }
  return Math.max(0, Math.min(n, Math.round(v * n)));
}

export function gonioDots(
  gx: ArrayLike<number>,
  gy: ArrayLike<number>,
  live: boolean,
  cap = 48,
): Array<{ x: number; y: number }> {
  if (! live) {
    return [];
  }
  const n = Math.min(gx.length, gy.length);
  if (n < 2) {
    return [];
  }
  const step = Math.max(1, Math.floor(n / cap));
  const out: Array<{ x: number; y: number }> = [];
  for (let i = 0; i < n && out.length < cap; i += step) {
    const x = gx[i] ?? 0;
    const y = gy[i] ?? 0;
    if (! Number.isFinite(x) || ! Number.isFinite(y)) {
      continue;
    }
    out.push({ x: Math.max(-1, Math.min(1, x)), y: Math.max(-1, Math.min(1, y)) });
  }
  return out;
}
