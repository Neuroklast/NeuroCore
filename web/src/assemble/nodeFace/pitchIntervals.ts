import { parseFaceNumber } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;

export type PitchMode = "octaver" | "shift" | "vocoder";

export type PitchFaceModel = {
  mode: PitchMode;
  sub: number;
  up: number;
  mix: number;
  semitones: number;
  bands: number;
};

function defaultMix(chipType: string): number {
  if (chipType === "pitch") {
    return 1;
  }
  if (chipType === "vocoder") {
    return 0.85;
  }
  return 0.3;
}

function pitchMode(chipType: string): PitchMode {
  const t = chipType.trim().toLowerCase();
  if (t === "pitch") {
    return "shift";
  }
  if (t === "vocoder") {
    return "vocoder";
  }
  return "octaver";
}

export function pitchFaceModel(chipType: string, args: Record<string, string>): PitchFaceModel {
  return {
    mode: pitchMode(chipType),
    sub: parseFaceNumber(args.sub, 1, 0, 1.5),
    up: parseFaceNumber(args.up, 0, 0, 1.5),
    mix: parseFaceNumber(args.mix, defaultMix(chipType), 0, 1),
    semitones: parseFaceNumber(args.semitones, 0, -24, 24),
    bands: Math.round(parseFaceNumber(args.bands, 16, 3, 32)),
  };
}

export type PitchStem = { x: number; y1: number };

function stem(x: number, amp: number): PitchStem {
  return { x, y1: VIEW_H - VIEW_H * Math.min(1, Math.max(0, amp)) };
}

/** Sub / dry / up mix, or a semitone marker, or vocoder band count. No spectrum. */
export function pitchStems(chipType: string, args: Record<string, string>): PitchStem[] {
  const m = pitchFaceModel(chipType, args);
  if (m.mode === "shift") {
    const x = VIEW_W * (m.semitones + 24) / 48;
    return [stem(x, m.mix)];
  }
  if (m.mode === "vocoder") {
    const n = m.bands;
    const stems: PitchStem[] = [];
    for (let i = 0; i < n; i += 1) {
      stems.push(stem((VIEW_W * (i + 0.5)) / n, m.mix));
    }
    return stems;
  }
  const dry = 1 - m.mix;
  return [
    stem(18, m.sub * m.mix / 1.5),
    stem(50, dry),
    stem(82, m.up * m.mix / 1.5),
  ];
}

export function pitchIntervalsPath(chipType: string, args: Record<string, string>): string {
  const stems = pitchStems(chipType, args);
  if (stems.length === 0) {
    return "M0 40 L100 40";
  }
  return stems.map((s, i) => `${i === 0 ? "M" : "L"}${s.x.toFixed(2)} ${s.y1.toFixed(2)}`).join(" ");
}
