import { parseFaceNumber, type FaceHost } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;

export type UtilityFaceModel = {
  gain: number;
  pan: number;
  polarity: boolean;
};

function parseOnOff(raw: string | undefined, fallback: boolean): boolean {
  const t = (raw ?? "").trim().toLowerCase();
  if (t === "on" || t === "true" || t === "1") {
    return true;
  }
  if (t === "off" || t === "false" || t === "0") {
    return false;
  }
  return fallback;
}

export function utilityFaceModel(args: Record<string, string>, host?: FaceHost): UtilityFaceModel {
  return {
    gain: parseFaceNumber(args.gain, 0, -24, 24, host),
    pan: parseFaceNumber(args.pan, 0, -1, 1, host),
    polarity: parseOnOff(args.polarity, false),
  };
}

export type UtilityMark = { x: number; y: number };

/** Gain as a vertical tick, pan as a horizontal marker. No invented meter. */
export function utilityMarks(args: Record<string, string>, host?: FaceHost): { gain: UtilityMark; pan: UtilityMark } {
  const m = utilityFaceModel(args, host);
  return {
    gain: { x: 28, y: VIEW_H * 0.5 - (m.gain / 24) * 14 },
    pan: { x: VIEW_W * 0.5 + m.pan * 36, y: 28 },
  };
}

export function utilityParamsPath(args: Record<string, string>, host?: FaceHost): string {
  const m = utilityMarks(args, host);
  const mid = VIEW_H * 0.5;
  return `M28 ${mid.toFixed(2)} L${m.gain.x.toFixed(2)} ${m.gain.y.toFixed(2)} M14 28 L86 28 M${m.pan.x.toFixed(2)} 22 L${m.pan.x.toFixed(2)} 34`;
}
