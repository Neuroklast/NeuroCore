import { streamSpeed } from "./cablePaint";
export type CableSignal = {
  snapshot: object | null;
  age: number;
  sinceEmission: number;
  peak: number;
  freshness: number;
  packets: number[];
};
export const emptyCableSignal = (): CableSignal => ({
  snapshot: null,
  age: 0,
  sinceEmission: 1,
  peak: 0,
  freshness: 0,
  packets: [],
});
/** One bounded train per new source snapshot. No recycling, no audio-latency claim. */
export function advanceCableSignal(
  s: CableSignal,
  snapshot: object,
  fractions: number[],
  rms: number,
  peak: number,
  length: number,
  dt: number,
  moving = true,
): CableSignal {
  const step = Math.min(0.1, Math.max(0, dt));
  s.age += step;
  s.sinceEmission += step;
  const fresh = s.snapshot !== snapshot;
  if (fresh) {
    s.snapshot = snapshot;
    s.age = 0;
    if (
      rms > 0.001 &&
      peak > 0.001 &&
      fractions.length &&
      (s.sinceEmission >= 0.12 || peak > s.peak + 0.08)
    ) {
      const heads = fractions
        .filter((v) => Number.isFinite(v) && v >= 0 && v <= 1)
        .slice(0, 6)
        .map((v) => v * length);
      s.packets = [...s.packets, ...heads].slice(-12);
      s.sinceEmission = 0;
    }
    s.peak = peak;
  }
  s.freshness = Math.max(0, 1 - Math.max(0, s.age - 0.2) / 0.3);
  const travel = moving ? streamSpeed(rms, peak) * step : 0;
  s.packets =
    s.freshness > 0 && rms > 0.001
      ? s.packets.map((d) => d + travel).filter((d) => d < length + 24)
      : [];
  return s;
}
