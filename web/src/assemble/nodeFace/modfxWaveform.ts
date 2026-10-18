import { chipSpec } from "../chipSpec";
import { parseFaceNumber, type FaceHost } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;
const FACE_POINTS = 48;
const RATE_RATIO = [1, 1.07, 0.93, 1.13];
const PHASE0 = [0, 2.09439516, 4.18879032, 1.04719758];

export type ModfxMode = "phaser" | "flanger" | "chorus";

export type ModfxFaceModel = {
  mode: ModfxMode;
  rate: number;
  depth: number;
  delay: number;
  mix: number;
  stages: number;
  voices: number;
  invert: boolean;
  center: number;
  feedback: number;
  width: number;
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

export function modfxMode(chipType: string): ModfxMode {
  const id = chipSpec(chipType).id;
  if (id === "flanger") {
    return "flanger";
  }
  if (id === "chorus") {
    return "chorus";
  }
  return "phaser";
}

export function modfxFaceModel(chipType: string, args: Record<string, string>, host?: FaceHost): ModfxFaceModel {
  const mode = modfxMode(chipType);
  const rateDef = mode === "flanger" ? 0.25 : mode === "chorus" ? 0.6 : 0.4;
  const delayDef = mode === "chorus" ? 18 : 2;
  return {
    mode,
    rate: parseFaceNumber(args.rate, rateDef, 0, 8, host),
    depth: parseFaceNumber(args.depth, mode === "chorus" ? 0.5 : 0.7, 0, mode === "phaser" ? 1.5 : 1, host),
    delay: parseFaceNumber(args.delay, delayDef, 0.1, 40, host),
    mix: parseFaceNumber(args.mix, 0.5, 0, 1, host),
    stages: Math.round(parseFaceNumber(args.stages, 6, 2, 12, host)),
    voices: Math.round(parseFaceNumber(args.voices, 3, 2, 4, host)),
    invert: parseOnOff(args.invert, mode === "flanger"),
    center: parseFaceNumber(args.center, 800, 80, 4000, host),
    feedback: parseFaceNumber(args.feedback, mode === "flanger" ? 0.45 : 0.3, 0, 0.95, host),
    width: parseFaceNumber(args.width, 0.7, 0, 1, host),
  };
}

/** One cycle of the engine LFO. Phaser/flanger sine; chorus averages voice sines. */
export function modfxSample(chipType: string, args: Record<string, string>, phase01: number, host?: FaceHost): number {
  const m = modfxFaceModel(chipType, args, host);
  if (m.mode === "chorus") {
    let s = 0;
    for (let v = 0; v < m.voices; v += 1) {
      s += Math.sin(phase01 * Math.PI * 2 * RATE_RATIO[v]! + PHASE0[v]!);
    }
    s /= m.voices;
    return 0.5 + 0.5 * m.depth * 0.4 * s;
  }
  let s = Math.sin(phase01 * Math.PI * 2);
  if (m.mode === "flanger" && m.invert) {
    s = -s;
  }
  const depth = m.mode === "phaser" ? Math.min(1, m.depth) : m.depth;
  return 0.5 + 0.5 * depth * s;
}

export function modfxWaveformPath(chipType: string, args: Record<string, string>, host?: FaceHost): string {
  const last = FACE_POINTS - 1;
  const parts: string[] = [];
  for (let i = 0; i < FACE_POINTS; i += 1) {
    const v = modfxSample(chipType, args, i / last, host);
    const x = (VIEW_W * i) / last;
    const y = VIEW_H - VIEW_H * Math.min(1, Math.max(0, v));
    parts.push(`${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`);
  }
  return parts.join(" ");
}

