import { parseFaceNumber, parseFaceToken } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;
const IN_MIN = -60;
const IN_MAX = 0;
const OUT_MIN = -60;
const OUT_MAX = 6;

export type DynamicsMode = "comp" | "limit" | "gate" | "ott" | "deesser" | "transient";

export type DynamicsFaceModel = {
  mode: DynamicsMode;
  threshold: number;
  ratio: number;
  knee: number;
  makeup: number;
  mix: number;
  ceiling: number;
  range: number;
  amount: number;
  attack: number;
  sustain: number;
  depth: number;
};

function dbToLin(db: number): number {
  return 10 ** (db / 20);
}

function linToDb(lin: number): number {
  return 20 * Math.log10(Math.max(lin, 1e-8));
}

/** Engine `Comp::computeGrDb`. */
export function computeGrDb(levelDb: number, thrDb: number, ratio: number, knee: number): number {
  const slope = 1 - 1 / Math.max(1.001, ratio);
  const over = levelDb - thrDb;
  if (knee <= 0.05) {
    return over > 0 ? over * slope : 0;
  }
  const half = 0.5 * knee;
  if (over <= -half) {
    return 0;
  }
  if (over >= half) {
    return over * slope;
  }
  const x = over + half;
  return slope * (x * x) / (2 * knee);
}

function dynamicsMode(chipType: string): DynamicsMode {
  const t = chipType.trim().toLowerCase();
  if (t === "limit" || t === "limiter") {
    return "limit";
  }
  if (t === "noisegate" || t === "gate" || t === "ngate" || t === "noise_gate") {
    return "gate";
  }
  if (t === "ott") {
    return "ott";
  }
  if (t === "deesser") {
    return "deesser";
  }
  if (t === "transient") {
    return "transient";
  }
  return "comp";
}

export function dynamicsFaceModel(chipType: string, args: Record<string, string>): DynamicsFaceModel {
  const mode = dynamicsMode(chipType);
  return {
    mode,
    threshold: parseFaceNumber(args.threshold ?? args.thresh, mode === "deesser" ? -8 : -18, -80, 6),
    ratio: parseFaceNumber(args.ratio, 4, 1, 20),
    knee: parseFaceNumber(args.knee, 0, 0, 24),
    makeup: parseFaceNumber(args.makeup ?? args.gain, 0, -24, 24),
    mix: parseFaceNumber(args.mix, 1, 0, 1),
    ceiling: parseFaceNumber(args.ceiling ?? args.ceil, mode === "limit" ? -0.3 : 0, -24, 0),
    range: parseFaceNumber(args.range, -80, -90, 0),
    amount: parseFaceNumber(args.amount, 0.7, 0, 1),
    attack: parseFaceNumber(args.attack, 0.5, -1, 1),
    sustain: parseFaceNumber(args.sustain, 0, -1, 1),
    depth: parseFaceNumber(args.depth, 1, 0, 1),
  };
}

export function dynamicsOutDb(chipType: string, args: Record<string, string>, inDb: number): number {
  const m = dynamicsFaceModel(chipType, args);
  if (m.mode === "transient") {
    const t = (inDb - IN_MIN) / (IN_MAX - IN_MIN);
    const shape = m.attack * (1 - t) + m.sustain * t;
    return inDb + shape * 6;
  }
  if (m.mode === "limit") {
    return Math.min(inDb, m.ceiling);
  }
  if (m.mode === "gate") {
    return inDb < m.threshold ? inDb + m.range : inDb;
  }
  let ratio = m.ratio;
  let mix = m.mix;
  let thr = m.threshold;
  let knee = m.knee;
  let makeup = m.makeup;
  if (m.mode === "deesser") {
    ratio = 1 + m.amount * 9;
    mix = m.amount;
  }
  if (m.mode === "ott") {
    ratio = 4;
    thr = -18;
    mix = m.depth;
    makeup = 0;
    knee = 0;
  }
  const gr = computeGrDb(inDb, thr, ratio, knee);
  const wet = dbToLin(inDb - gr + makeup);
  const dry = dbToLin(inDb);
  return linToDb(dry * (1 - mix) + wet * mix);
}

export function dynamicsCurvePath(chipType: string, args: Record<string, string>): string {
  const last = FACE_POINTS - 1;
  const parts: string[] = [];
  for (let i = 0; i < FACE_POINTS; i += 1) {
    const inDb = IN_MIN + (IN_MAX - IN_MIN) * (i / last);
    const outDb = dynamicsOutDb(chipType, args, inDb);
    const x = (VIEW_W * i) / last;
    const t = (outDb - OUT_MIN) / (OUT_MAX - OUT_MIN);
    const y = VIEW_H - VIEW_H * Math.min(1, Math.max(0, t));
    parts.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return parts.join(" ");
}

export function dynamicsDetector(args: Record<string, string>): string {
  const t = parseFaceToken(args.detector, "peak");
  return t === "rms" ? "rms" : "peak";
}
