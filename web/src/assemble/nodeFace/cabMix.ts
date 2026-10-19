import { parseFaceNumber, type FaceHost } from "./faceArgs";

const VIEW_H = 40;

export type CabFaceModel = {
  mix: number;
  gain: number;
};

export function cabFaceModel(args: Record<string, string>, host?: FaceHost): CabFaceModel {
  return {
    mix: parseFaceNumber(args.mix, 0.3, 0, 1, host),
    gain: parseFaceNumber(args.gain, 0, -24, 24, host),
  };
}

export type CabStem = { x: number; y1: number };

function stem(x: number, amp: number): CabStem {
  return { x, y1: VIEW_H - VIEW_H * Math.min(1, Math.max(0, amp)) };
}

/** Dry / wet mix. IR slot name lives on the host, not in ChipSpec. */
export function cabStems(args: Record<string, string>, host?: FaceHost): CabStem[] {
  const m = cabFaceModel(args, host);
  return [stem(28, 1 - m.mix), stem(72, m.mix)];
}

export function cabMixPath(args: Record<string, string>, host?: FaceHost): string {
  const stems = cabStems(args, host);
  return stems.map((s, i) => `${i === 0 ? "M" : "L"}${s.x.toFixed(2)} ${s.y1.toFixed(2)}`).join(" ");
}
