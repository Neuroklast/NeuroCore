import { parseFaceNumber, parseFaceToken } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;

export type EnvFaceModel = {
  attack: number;
  hold: number;
  release: number;
  min: number;
  max: number;
  invert: boolean;
  unit: "lin" | "db";
  source: string;
};

export function envFaceModel(args: Record<string, string>): EnvFaceModel {
  const invertTok = parseFaceToken(args.invert, "off");
  return {
    attack: parseFaceNumber(args.attack, 0.01, 0.001, 1),
    hold: parseFaceNumber(args.hold, 0, 0, 0.5),
    release: parseFaceNumber(args.release, 0.1, 0.001, 2),
    min: parseFaceNumber(args.min, 0, 0, 1),
    max: parseFaceNumber(args.max, 1, 0, 1),
    invert: invertTok === "on" || invertTok === "true" || invertTok === "1",
    unit: parseFaceToken(args.unit, "lin") === "db" ? "db" : "lin",
    source: parseFaceToken(args.source, "in"),
  };
}

/** Parameter curve: linear attack, hold plateau, exponential release. The header lamp is the live follower. */
export function envEnvelope(args: Record<string, string>, t: number): number {
  const m = envFaceModel(args);
  let v = 0;
  if (t <= 0) {
    v = 0;
  } else if (t < m.attack) {
    v = t / m.attack;
  } else if (t < m.attack + m.hold) {
    v = 1;
  } else {
    v = Math.exp(-(t - m.attack - m.hold) / m.release);
  }
  if (m.invert) {
    v = 1 - v;
  }
  return m.min + (m.max - m.min) * Math.min(1, Math.max(0, v));
}

export function envEnvelopePath(args: Record<string, string>): string {
  const m = envFaceModel(args);
  const span = Math.max(0.05, m.attack + m.hold + m.release * 2);
  const last = FACE_POINTS - 1;
  const parts: string[] = [];
  for (let i = 0; i < FACE_POINTS; i += 1) {
    const t = span * (i / last);
    const y = VIEW_H - VIEW_H * envEnvelope(args, t);
    const x = (VIEW_W * i) / last;
    parts.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return parts.join(" ");
}
