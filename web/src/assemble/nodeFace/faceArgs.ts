import { knobMapped, type LiveKnob } from "../liveArg";

/** Host default when tempo is missing or zero. */
export const FACE_BPM = 120;

/** Host default when sample rate is missing or zero. */
export const FACE_FS = 48000;

export type FaceKnob = LiveKnob & { isNote?: boolean };

export type FaceHost = {
  knobs?: FaceKnob[];
  controlA?: number;
  controlB?: number;
  controlOut?: number;
  bpm?: number;
  sr?: number;
  /** Live step-sequencer position, or -1 when the host is stopped. */
  step?: number;
  stepCount?: number;
  /** Live envelope-trigger pulse state (0/1). */
  trigger?: number;
};

export function faceBpm(host?: FaceHost): number {
  const n = host?.bpm;
  return Number.isFinite(n) && (n as number) > 0 ? (n as number) : FACE_BPM;
}

export function faceSr(host?: FaceHost): number {
  const n = host?.sr;
  return Number.isFinite(n) && (n as number) > 0 ? (n as number) : FACE_FS;
}

function clampFace(v: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, v));
}

/** Literal, `b[900]`, bare `a`–`f`, or `map(x,in0,in1,out0,out1)`. No eval. */
export function resolveFaceScalar(raw: string | undefined, host?: FaceHost): number | null {
  if (raw == null) {
    return null;
  }
  const t = raw.trim();
  if (! t) {
    return null;
  }
  const atom = resolveFaceAtom(t, host);
  if (atom != null) {
    return atom;
  }
  const mapped = /^map\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([^)]+)\s*\)$/i.exec(t);
  if (! mapped) {
    return null;
  }
  const x = resolveFaceAtom(mapped[1]!.trim(), host);
  const in0 = resolveFaceAtom(mapped[2]!.trim(), host);
  const in1 = resolveFaceAtom(mapped[3]!.trim(), host);
  const out0 = resolveFaceAtom(mapped[4]!.trim(), host);
  const out1 = resolveFaceAtom(mapped[5]!.trim(), host);
  if (x == null || in0 == null || in1 == null || out0 == null || out1 == null) {
    return null;
  }
  const span = in1 - in0;
  const t01 = Math.abs(span) < 1e-12 ? 0 : (x - in0) / span;
  return out0 + t01 * (out1 - out0);
}

function resolveFaceAtom(raw: string, host?: FaceHost): number | null {
  const n = Number(raw);
  if (Number.isFinite(n)) {
    return n;
  }
  const inlay = /^([a-f])\s*\[\s*([-+0-9.eE]+)\s*\]$/i.exec(raw);
  if (inlay) {
    const v = Number(inlay[2]);
    return Number.isFinite(v) ? v : null;
  }
  if (/^[a-f]$/i.test(raw)) {
    const v = knobMapped(raw, host?.knobs ?? []);
    return v;
  }
  return null;
}

/** Parse a ChipSpec arg for a calculated face. Bound letters follow FaceHost knobs. */
export function parseFaceNumber(
  raw: string | undefined,
  fallback: number,
  min: number,
  max: number,
  host?: FaceHost,
): number {
  const n = resolveFaceScalar(raw, host);
  return clampFace(n != null ? n : fallback, min, max);
}

export function parseFaceToken(raw: string | undefined, fallback: string): string {
  const t = (raw ?? "").trim().toLowerCase();
  return t || fallback;
}
