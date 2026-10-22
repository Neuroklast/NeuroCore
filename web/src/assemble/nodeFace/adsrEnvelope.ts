import { parseFaceNumber, type FaceHost } from "./faceArgs";

const VIEW_W = 100;
const VIEW_H = 40;

export type AdsrEnvelopeModel = {
  attack: number;
  decay: number;
  sustain: number;
  release: number;
};

export function adsrFaceModel(args: Record<string, string>, host?: FaceHost): AdsrEnvelopeModel {
  return {
    attack: parseFaceNumber(args.attack, 0.1, 0.001, 10, host),
    decay: parseFaceNumber(args.decay, 0.1, 0.001, 10, host),
    sustain: parseFaceNumber(args.sustain, 0.7, 0, 1, host),
    release: parseFaceNumber(args.release, 0.1, 0.001, 10, host),
  };
}

export function adsrEnvelopePath(args: Record<string, string>, host?: FaceHost): string {
  const m = adsrFaceModel(args, host);
  const hold = Math.max(0.02, (m.attack + m.decay + m.release) * 0.25);
  const total = Math.max(1e-4, m.attack + m.decay + hold + m.release);
  const x = (t: number) => (VIEW_W * t) / total;
  const y = (v: number) => (VIEW_H - VIEW_H * Math.max(0, Math.min(1, v))).toFixed(2);
  return [
    `M0 ${y(0)}`,
    `L${x(m.attack).toFixed(2)} ${y(1)}`,
    `L${x(m.attack + m.decay).toFixed(2)} ${y(m.sustain)}`,
    `L${x(m.attack + m.decay + hold).toFixed(2)} ${y(m.sustain)}`,
    `L${VIEW_W} ${y(0)}`,
  ].join(" ");
}
