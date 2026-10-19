import { chipSpec } from "../chipSpec";
import { parseFaceNumber, parseFaceToken, type FaceHost } from "./faceArgs";

const VIEW_H = 40;
const WEST = 12;
const EAST = 88;

export type RoutingFaceModel = {
  mode: string;
  mix: number;
  width: number;
  f1: number;
  f2: number;
  name: string;
  kanal: string;
};

export function routingFaceModel(chipType: string, args: Record<string, string>, host?: FaceHost): RoutingFaceModel {
  return {
    mode: chipSpec(chipType, args).id,
    mix: parseFaceNumber(args.mix, 0.5, 0, 1, host),
    width: parseFaceNumber(args.width, 1, 0, 1.4, host),
    f1: parseFaceNumber(args.f1, 200, 40, 800, host),
    f2: parseFaceNumber(args.f2, 2000, 200, 8000, host),
    name: parseFaceToken(args.name, "dirt"),
    kanal: parseFaceToken(args.kanal, "both"),
  };
}

export type RouteStem = { x1: number; y1: number; x2: number; y2: number; amp: number };

function spread(count: number, index: number): number {
  if (count <= 1) {
    return VIEW_H * 0.5;
  }
  return 8 + (24 * index) / (count - 1);
}

/** Split / join / bus map from ChipSpec ports. No decorative waveform. */
export function routingStems(chipType: string, args: Record<string, string>, host?: FaceHost): RouteStem[] {
  const spec = chipSpec(chipType, args);
  const m = routingFaceModel(chipType, args, host);
  if (m.mode === "width") {
    return [
      { x1: WEST, y1: 14, x2: EAST, y2: 14, amp: 1 },
      { x1: WEST, y1: 26, x2: EAST, y2: 26, amp: Math.min(1, m.width / 1.4) },
    ];
  }
  if (m.mode === "join") {
    return [
      { x1: WEST, y1: 14, x2: EAST, y2: 20, amp: 1 - m.mix },
      { x1: WEST, y1: 26, x2: EAST, y2: 20, amp: m.mix },
    ];
  }
  const ins = spec.audioIns;
  const outs = spec.audioOuts;
  if (ins.length <= 1) {
    const yIn = spread(1, 0);
    if (outs.length === 0) {
      return [{ x1: WEST, y1: yIn, x2: EAST, y2: yIn, amp: 1 }];
    }
    return outs.map((_, i) => ({
      x1: WEST,
      y1: yIn,
      x2: EAST,
      y2: spread(outs.length, i),
      amp: 1,
    }));
  }
  if (outs.length <= 1) {
    const yOut = spread(1, 0);
    return ins.map((_, i) => ({
      x1: WEST,
      y1: spread(ins.length, i),
      x2: EAST,
      y2: yOut,
      amp: 1,
    }));
  }
  const n = Math.max(ins.length, outs.length);
  const stems: RouteStem[] = [];
  for (let i = 0; i < n; i += 1) {
    stems.push({
      x1: WEST,
      y1: spread(ins.length, Math.min(i, ins.length - 1)),
      x2: EAST,
      y2: spread(outs.length, Math.min(i, outs.length - 1)),
      amp: 1,
    });
  }
  return stems;
}

export function routingMapPath(chipType: string, args: Record<string, string>, host?: FaceHost): string {
  return routingStems(chipType, args, host)
    .map((s) => `M${s.x1.toFixed(2)} ${s.y1.toFixed(2)} L${s.x2.toFixed(2)} ${s.y2.toFixed(2)}`)
    .join(" ");
}
