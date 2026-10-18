/** Consume each telemetry snapshot once. Scope windows overlap and must not be
 * rescanned as consecutive audio, otherwise a held sub tone retriggers itself.
 * A native onset is the hit; bass rise is the fallback when the ring is silent. */
export type ParticleAudioState = {
  tick: number;
  seq: number;
  bass: number;
  bassHold: number;
  lastOnsetPos: number;
};
export const PARTICLE_AUDIO_IDLE: ParticleAudioState = {
  tick: -1, seq: 0, bass: 0, bassHold: 0, lastOnsetPos: -1,
};

export type ParticleSpawn = { amp: number; band: "low" | "mid" | "high"; balance: number };

/** Unsigned 32-bit serial: a click after wrap is still newer than 0xffffff00. */
export function onsetIsNew(samplePos: number, lastPos: number): boolean {
  if (! Number.isFinite(samplePos)) {
    return false;
  }
  if (! Number.isFinite(lastPos) || lastPos < 0) {
    return true;
  }
  const d = ((samplePos >>> 0) - (lastPos >>> 0)) >>> 0;
  return d > 0 && d < 0x80000000;
}

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
    onsets?: { samplePos: number; amp: number; balance?: number }[];
    live: boolean;
  },
  dt: number,
): { state: ParticleAudioState; impulse: number; spawn: ParticleSpawn | null; spawns: ParticleSpawn[] } {
  const clamp = (v: number) => Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : 0;
  const hold = Math.max(0, previous.bassHold - Math.max(0, dt));
  if (!frame.live) return { state: { ...PARTICLE_AUDIO_IDLE }, impulse: 0, spawn: null, spawns: [] };
  const seq = frame.seq ?? frame.tick;
  if (previous.tick === frame.tick || (seq > 0 && previous.seq === seq)) {
    return { state: { ...previous, bassHold: hold }, impulse: 0, spawn: null, spawns: [] };
  }
  const bass = clamp(frame.bass);
  const rise = Math.max(0, bass - previous.bass);
  const bassHit = hold === 0 && rise > 0.08 ? clamp(rise * 2) : 0;
  const mid = clamp(frame.mid ?? 0);
  const high = clamp(frame.high ?? 0);
  const energyBand = (): ParticleSpawn["band"] =>
    bass >= mid && bass >= high ? "low" : mid >= high ? "mid" : "high";
  const spawns: ParticleSpawn[] = [];
  let lastOnsetPos = previous.lastOnsetPos;
  const queued = frame.onsets ?? [];
  if (queued.length > 0) {
    for (const e of queued) {
      if (! onsetIsNew(e.samplePos, lastOnsetPos)) {
        continue;
      }
      const amp = clamp(e.amp);
      if (amp > 0) {
        const b = Number.isFinite(e.balance) ? Math.max(-1, Math.min(1, e.balance as number)) : 0;
        spawns.push({ amp, band: energyBand(), balance: b });
      }
      lastOnsetPos = e.samplePos;
    }
  } else if (frame.onsetCount > 0) {
    const amp = clamp(frame.onsetAmp);
    if (amp > 0) {
      spawns.push({ amp, band: energyBand(), balance: 0 });
    }
  }
  if (bassHit > 0 && spawns.length === 0) {
    spawns.push({ amp: bassHit, band: "low", balance: 0 });
  }
  const impulse = spawns.reduce((m, s) => Math.max(m, s.amp), 0);
  return {
    state: {
      tick: frame.tick,
      seq,
      bass,
      bassHold: spawns.length > 0 ? 0.08 : hold,
      lastOnsetPos,
    },
    impulse,
    spawn: spawns[0] ?? null,
    spawns,
  };
}
