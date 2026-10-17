/** Consume each telemetry snapshot once. Scope windows overlap and must not be
 * rescanned as consecutive audio, otherwise a held sub tone retriggers itself. */
export type ParticleAudioState = { tick: number; bass: number; bassHold: number };
export const PARTICLE_AUDIO_IDLE: ParticleAudioState = { tick: -1, bass: 0, bassHold: 0 };

export function particleAudio(
  previous: ParticleAudioState,
  frame: { tick: number; bass: number; onsetCount: number; onsetAmp: number; live: boolean },
  dt: number,
): { state: ParticleAudioState; impulse: number } {
  const clamp = (v: number) => Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : 0;
  const hold = Math.max(0, previous.bassHold - Math.max(0, dt));
  if (!frame.live) return { state: { ...PARTICLE_AUDIO_IDLE }, impulse: 0 };
  if (previous.tick === frame.tick) return { state: { ...previous, bassHold: hold }, impulse: 0 };
  const bass = clamp(frame.bass);
  const rise = Math.max(0, bass - previous.bass);
  const bassHit = hold === 0 && rise > 0.08 ? clamp(rise * 2) : 0;
  const nativeHit = frame.onsetCount > 0 ? clamp(frame.onsetAmp) : 0;
  return {
    state: { tick: frame.tick, bass, bassHold: bassHit > 0 ? 0.08 : hold },
    impulse: Math.max(nativeHit, bassHit),
  };
}
