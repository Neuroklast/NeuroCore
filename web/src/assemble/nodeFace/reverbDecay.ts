import { parseFaceNumber } from "./faceArgs";

/** Engine clamps from SignalChain::Reverb (process / tailSeconds). Do not rewrite factory args. */
export const REVERB_DECAY_MIN = 0.05;
export const REVERB_DECAY_MAX = 0.95;
export const REVERB_SIZE_MIN = 0.05;
export const REVERB_RT_MIN = 0.2;
export const REVERB_RT_MAX = 10;
export const REVERB_TIME_AXIS = 10;
const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;

export type ReverbFaceModel = {
  size: number;
  decay: number;
  damp: number;
  mix: number;
  rt60: number;
};

export function clampReverbDecay(raw: number): number {
  return Math.min(REVERB_DECAY_MAX, Math.max(REVERB_DECAY_MIN, raw));
}

/** Empirical RT60 proxy: `0.4 + size * 1.2 + decay * 4.5`, clamped 0.2–10 s. */
export function reverbRt60(size: number, decay: number): number {
  const s = Math.min(1, Math.max(REVERB_SIZE_MIN, size));
  const d = clampReverbDecay(decay);
  return Math.min(REVERB_RT_MAX, Math.max(REVERB_RT_MIN, 0.4 + s * 1.2 + d * 4.5));
}

export function reverbFaceModel(args: Record<string, string>): ReverbFaceModel {
  const size = parseFaceNumber(args.size, 0.45, REVERB_SIZE_MIN, 1);
  const decay = parseFaceNumber(args.decay, 0.5, REVERB_DECAY_MIN, REVERB_DECAY_MAX);
  const damp = parseFaceNumber(args.damp, 0.3, 0, 0.95);
  const mix = parseFaceNumber(args.mix, 0.3, 0, 1);
  return { size, decay, damp, mix, rt60: reverbRt60(size, decay) };
}

/** Amplitude vs time. −60 dB at RT60. Mix scales wet level. Damp is not in the engine RT60 formula. */
export function reverbEnvelope(args: Record<string, string>, t: number): number {
  const m = reverbFaceModel(args);
  if (t <= 0) {
    return m.mix;
  }
  return m.mix * Math.exp(-Math.log(1000) * t / m.rt60);
}

export function reverbDecayPath(args: Record<string, string>): string {
  const last = FACE_POINTS - 1;
  const parts: string[] = [];
  for (let i = 0; i < FACE_POINTS; i += 1) {
    const t = REVERB_TIME_AXIS * (i / last);
    const amp = Math.min(1, Math.max(0, reverbEnvelope(args, t)));
    const x = (VIEW_W * i) / last;
    const y = VIEW_H - VIEW_H * amp;
    parts.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return parts.join(" ");
}
