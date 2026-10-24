# Factory listening prep, 2026-10-24

Machine render of every factory preset through `NeuroKoreAudioProcessor` (48 kHz, 512-sample blocks, 56 blocks, skip first 32). Probe is 440 Hz sine + pink, no MIDI, no sidechain. This sheet ranks what to listen to. It is not a DAW, LUFS, or ear sign-off.

Regenerate:

```
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe --factory-report docs/presets/2026-10-24-factory-render.json
py -3 scripts/factory_listening_prep.py docs/presets/2026-10-24-factory-render.json -o docs/presets/2026-10-24-listening-prep.md
```

## Totals

| Metric | Value |
|---|---|
| Presets rendered | 363 |
| Apply / nonfinite failures | 0 |
| Apply errors | 0 |
| Nonfinite output | 0 |
| Sample rate | 48000 |
| Block / measured blocks | 512 / 24 |
| Quiet inserts vs FactoryLoudness floors | 0 |

## Ear-pass queue

Start here. Factory scripts are unchanged; retune only after a human pass.

No insert is below the FactoryLoudness floors on this probe. MIDI-gated and sidechain presets still need a DAW ear pass; the probe has no notes and no sidechain.

## Hottest 15 (peak)

| Preset | Category | Peak | RMS |
|---|---|---|---|
| Lead Presence Stack | Guitar | 0.677 | 0.3003 |
| Ironclad Split Bass | Metal | 0.652 | 0.4842 |
| CTZ Kick Master | Club | 0.646 | 0.4126 |
| Brickwall Techno | Club | 0.642 | 0.4100 |
| Tight Gate Lead | Guitar | 0.639 | 0.2610 |
| Loudness Wall | Mastering | 0.634 | 0.3522 |
| Baritone Serrated Stack | Metal | 0.620 | 0.4390 |
| Scooped Rhythm Wall | Guitar | 0.619 | 0.3668 |
| Deathwall Tight Stack | Metal | 0.614 | 0.4324 |
| Rhythm Mid Scoop | Guitar | 0.614 | 0.2808 |
| Acid Hash | Club | 0.607 | 0.4862 |
| Impact Designer | Sound Design | 0.602 | 0.3397 |
| Lead Satin Clip | Distortion | 0.600 | 0.2939 |
| Blastbeat Front Edge | Metal | 0.600 | 0.3074 |
| Hard Groove Machine | EDM | 0.598 | 0.3267 |

## Quietest 15 (peak)

| Preset | Category | Peak | RMS |
|---|---|---|---|
| MIDI Bowed Resonator | Ambient | 0.024 | 0.0070 |
| Probability Break Chops | Drums | 0.082 | 0.0317 |
| Seeded Drift Filter | Sound Design | 0.100 | 0.0551 |
| Telephone Then Room | Lo-Fi | 0.115 | 0.0459 |
| Blackwall Space | Cinematic | 0.115 | 0.0420 |
| MIDI Swell Return | Ambient | 0.120 | 0.0529 |
| Envelope Shaper | Dynamics | 0.120 | 0.0610 |
| Bitcrush Glitch | Sound Design | 0.126 | 0.0520 |
| Motion Blur Rumble Send | Industrial | 0.136 | 0.0761 |
| Broken Speaker | Lo-Fi | 0.147 | 0.0866 |
| Broken Grid Chaos | Creative | 0.152 | 0.0702 |
| Bass Synth Stack | Pitch | 0.156 | 0.0792 |
| Airy Clean | Distortion | 0.157 | 0.0786 |
| Chopper | Modulation | 0.161 | 0.0626 |
| Ambient Swell Guitar | Guitar | 0.161 | 0.0501 |

## Categories

| Category | Count |
|---|---|
| Vocals | 43 |
| Guitar | 33 |
| Club | 30 |
| Distortion | 29 |
| Bass | 25 |
| Mastering | 18 |
| Dynamics | 17 |
| Sound Design | 15 |
| Cinematic | 14 |
| EDM | 14 |
| Drums | 13 |
| Creative | 11 |
| Metal | 11 |
| Modulation | 11 |
| Synth | 11 |
| Ambient | 10 |
| Delay | 10 |
| Industrial | 9 |
| Lo-Fi | 9 |
| Reverb | 9 |
| Filter | 8 |
| Mixing | 5 |
| Pitch | 5 |
| Psychoacoustic | 2 |
| Utility | 1 |

## Limits

- MIDI-gated and sidechain presets are under-driven by this probe. `FactoryUseCaseTest` still fails Vocal Presence Duck (open=0 / duck=0) because the presence-band duck needs a vocal-like SC, not a 3.2 kHz tone on the main input alone.
- `FactoryLoudnessTest` uses these same floors. The ear-pass queue is that quiet-insert list. Sends (Delay/Reverb and named send-likes) are excluded.
- Subjective listening, latency, LUFS, and DAW load remain open.
