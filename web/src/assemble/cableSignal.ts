import { streamSpeed } from "./cablePaint";

export type CablePacket = { distance: number; age: number; strength: number; lifetime?: number };
export type CableSignal = {
  snapshot: object | null;
  age: number;
  sinceEmission: number;
  peak: number;
  freshness: number;
  speed: number;
  packets: number[];
  impulses: CablePacket[];
};
export const emptyCableSignal = (): CableSignal => ({
  snapshot: null, age: 0, sinceEmission: 1, peak: 0,
  freshness: 0, speed: 0, packets: [], impulses: [],
});

/** Snapshot peaks trigger source emission, never positions on the cable. */
export function advanceCableSignal(
  s: CableSignal, snapshot: object, _fractions: number[], rms: number,
  peak: number, length: number, dt: number, moving = true, worldPerPixel = 1,
): CableSignal {
  const step = Number.isFinite(dt) ? Math.min(0.1, Math.max(0, dt)) : 0;
  const energy = Number.isFinite(rms) ? Math.max(0, rms) : 0;
  const level = Number.isFinite(peak) ? Math.max(0, peak) : 0;
  s.age += step;
  s.sinceEmission += step;
  const fresh = s.snapshot !== snapshot;
  if (fresh) { s.snapshot = snapshot; s.age = 0; }
  s.freshness = Math.max(0, 1 - Math.max(0, s.age - 0.2) / 0.3);
  const minSpeed = 48 * Math.max(0.25, worldPerPixel);
  const target = Math.max(48, streamSpeed(energy, level)) * Math.max(0.25, worldPerPixel);
  // Exact exponential speed integration keeps 30/60 Hz travel consistent.
  const decay = Math.exp(-step / 0.15);
  const initialSpeed = s.speed || target;
  const travel = moving ? target * step + (initialSpeed - target) * 0.15 * (1 - decay) : 0;
  s.speed = target + (initialSpeed - target) * decay;
  s.impulses = s.impulses.filter((p) => {
    p.distance += travel;
    p.age += step;
    // Silence and camera zoom can slow already-emitted light. Keep enough
    // lifetime for the remaining route at the current minimum travel speed.
    p.lifetime = Math.max(p.lifetime ?? 2, p.age + Math.max(0, length - p.distance) / minSpeed + 0.6);
    return p.age < p.lifetime && p.distance < length + 64;
  });
  if (fresh) {
    if (moving && length > 0 && energy > 0.001 && level > 0.001 &&
        (s.sinceEmission >= 0.12 || level > s.peak + 0.08) &&
        s.impulses.length < 12) {
      s.impulses.push({ distance: 0, age: 0, strength: Math.min(1, level), lifetime: Math.max(2, length / minSpeed + 0.6) });
      s.sinceEmission = 0;
    }
    s.peak = level;
  }
  s.packets = s.impulses.map((p) => p.distance);
  return s;
}

/** Emitted light has its own finite lifetime, independent of subsequent silence. */
export function cablePacketAlpha(packet: CablePacket): number {
  return (0.4 + 0.6 * packet.strength) * Math.max(0, Math.min(1, ((packet.lifetime ?? 2) - packet.age) / 0.6));
}
