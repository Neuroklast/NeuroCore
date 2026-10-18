import { lfoWave, parseLfoShape, resolveLfoHz, type LfoShape } from "../lfoLamp";
import { faceBpm, parseFaceNumber, type FaceHost } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;
export type LfoFaceModel = {
  shape: LfoShape;
  depth: number;
  hz: number;
};

export function lfoFaceModel(args: Record<string, string>, host?: FaceHost): LfoFaceModel {
  return {
    shape: parseLfoShape(args.shape ?? "sine"),
    depth: parseFaceNumber(args.depth, 1, 0, 1, host),
    hz: resolveLfoHz({ freq: args.freq, sync: args.sync }, host?.knobs ?? [], faceBpm(host)),
  };
}

/** One cycle. Depth scales around 0.5. Freq/sync follow FaceHost knobs and bpm. */
export function lfoWaveformPath(args: Record<string, string>, host?: FaceHost): string {
  const m = lfoFaceModel(args, host);
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
