import { lfoWave, parseLfoShape, resolveLfoHz, type LfoShape } from "../lfoLamp";
import { FACE_BPM, parseFaceNumber } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;
const NO_KNOBS: Array<{ id: string; value: number; min: number; max: number; isNote?: boolean }> = [];

export type LfoFaceModel = {
  shape: LfoShape;
  depth: number;
  hz: number;
};

export function lfoFaceModel(args: Record<string, string>): LfoFaceModel {
  return {
    shape: parseLfoShape(args.shape ?? "sine"),
    depth: parseFaceNumber(args.depth, 1, 0, 1),
    hz: resolveLfoHz({ freq: args.freq, sync: args.sync }, NO_KNOBS, FACE_BPM),
  };
}

/** One cycle. Depth scales around 0.5. Formulas for freq/sync use ChipSpec defaults, not the knobs store. */
export function lfoWaveformPath(args: Record<string, string>): string {
  const m = lfoFaceModel(args);
  const last = FACE_POINTS;
  const parts: string[] = [];
  for (let i = 0; i <= last; i += 1) {
    const phase = i / last;
    const v = 0.5 + (lfoWave(phase, m.shape) - 0.5) * m.depth;
    const x = (VIEW_W * i) / last;
    const y = VIEW_H - VIEW_H * Math.min(1, Math.max(0, v));
    parts.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return parts.join(" ");
}
