/** Consume each telemetry snapshot once. Scope windows overlap and must not be
 * rescanned as consecutive audio, otherwise a held sub tone retriggers itself. */
export type ParticleAudioState = { tick: number; seq: number; bass: number; bassHold: number };
export const PARTICLE_AUDIO_IDLE: ParticleAudioState = { tick: -1, seq: 0, bass: 0, bassHold: 0 };

export type ParticleSpawn = { amp: number; band: "low" | "mid" | "high" };

export function particleAudio(
  previous: ParticleAudioState,
  frame: {
    tick: number;
    seq?: number;
    bass: number;
    mid?: number;
    high?: number;
    onsetCount: number;
    onsetAmp: number;
    live: boolean;
  },
  dt: number,
): { state: ParticleAudioState; impulse: number; spawn: ParticleSpawn | null } {
  const clamp = (v: number) => Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : 0;
  const hold = Math.max(0, previous.bassHold - Math.max(0, dt));
  if (!frame.live) return { state: { ...PARTICLE_AUDIO_IDLE }, impulse: 0, spawn: null };
  const seq = frame.seq ?? frame.tick;
  if (previous.tick === frame.tick || (seq > 0 && previous.seq === seq)) {
    return { state: { ...previous, bassHold: hold }, impulse: 0, spawn: null };
  }
  const bass = clamp(frame.bass);
  const rise = Math.max(0, bass - previous.bass);
  const bassHit = hold === 0 && rise > 0.08 ? clamp(rise * 2) : 0;
  const nativeHit = frame.onsetCount > 0 ? clamp(frame.onsetAmp) : 0;
  const impulse = Math.max(nativeHit, bassHit);
  let spawn: ParticleSpawn | null = null;
  if (impulse > 0) {
    const mid = clamp(frame.mid ?? 0);
    const high = clamp(frame.high ?? 0);
    const band = bassHit >= nativeHit || bass >= mid && bass >= high
      ? "low"
      : mid >= high ? "mid" : "high";
    spawn = { amp: impulse, band };
  }
  return {
    state: { tick: frame.tick, seq, bass, bassHold: bassHit > 0 ? 0.08 : hold },
    impulse,
    spawn,
  };
}
