import { expect, it } from 'vitest';
import { particleAudio, PARTICLE_AUDIO_IDLE } from './particleAudio';

it('consumes a native transient only once even if it spans several display frames', () => {
  const frame = { tick: 4, bass: 0, onsetCount: 1, onsetAmp: .8, live: true };
  const first = particleAudio(PARTICLE_AUDIO_IDLE, frame, 1/60);
  expect(first.impulse).toBe(.8);
  expect(particleAudio(first.state, frame, 1/60).impulse).toBe(0);
});

it('sub attack lifts even without a broadband/native transient and does not pump a held tone', () => {
  let hit = particleAudio(PARTICLE_AUDIO_IDLE, { tick: 1, bass: .7, onsetCount: 0, onsetAmp: 0, live: true }, 1/60);
  expect(hit.impulse).toBeGreaterThan(.6);
  let repeats = 0;
  for (let i=2;i<120;i++) {
    hit = particleAudio(hit.state, { tick: i, bass: .7, onsetCount: 0, onsetAmp: 0, live: true }, 1/60);
    repeats += hit.impulse;
  }
  expect(repeats).toBe(0);
  const quiet = particleAudio(hit.state, { tick: 121, bass: 0, onsetCount: 0, onsetAmp: 0, live: true }, 1/60);
  expect(particleAudio(quiet.state, { tick: 122, bass: .7, onsetCount: 0, onsetAmp: 0, live: true }, 1/60).impulse).toBeGreaterThan(.6);
});

it('a native hit is one impulse at 30 fps and at 60 fps for the same tick', () => {
  const frame = { tick: 9, bass: 0.1, onsetCount: 1, onsetAmp: 0.7, live: true };
  const a = particleAudio(PARTICLE_AUDIO_IDLE, frame, 1 / 30);
  const b = particleAudio(PARTICLE_AUDIO_IDLE, frame, 1 / 60);
  expect(a.impulse).toBe(b.impulse);
  expect(particleAudio(a.state, frame, 1 / 30).impulse).toBe(0);
  expect(particleAudio(b.state, frame, 1 / 60).impulse).toBe(0);
});

it('offline frames discard stale native hits and detector state', () => {
  const hit = particleAudio(PARTICLE_AUDIO_IDLE, { tick: 3, bass: 1, onsetCount: 1, onsetAmp: 1, live: false }, 1/60);
  expect(hit.impulse).toBe(0);
  expect(hit.spawn).toBeNull();
  expect(hit.state).toEqual(PARTICLE_AUDIO_IDLE);
});

it('a native seq is consumed once even when the display tick advances', () => {
  const frame = { tick: 1, seq: 9, bass: 0, onsetCount: 1, onsetAmp: 0.6, live: true };
  const first = particleAudio(PARTICLE_AUDIO_IDLE, frame, 1 / 60);
  expect(first.spawn?.amp).toBeCloseTo(0.6);
  expect(particleAudio(first.state, { ...frame, tick: 2 }, 1 / 60).spawn).toBeNull();
});
