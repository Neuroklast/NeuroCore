# Factory listening prep, 2026-10-24

Machine render of every factory preset through `NeuroKoreAudioProcessor` (48 kHz, 512-sample blocks, 40 blocks, skip first 16). Probe is 440 Hz sine + pink, no MIDI, no sidechain. This sheet ranks what to listen to. It is not a DAW, LUFS, or ear sign-off.

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
| Quiet inserts vs FactoryLoudness floors | 3 |

## Ear-pass queue

Start here. Factory scripts are unchanged; retune only after a human pass.

| Preset | Peak | RMS | Why |
|---|---|---|---|
| Ambient Swell Guitar | 0.136 | 0.0379 | Quiet insert on the sine+pink probe. Ear-check swell/gate vs a real guitar. |
| MIDI Swell Return | 0.113 | 0.0458 | Quiet insert. Needs MIDI gate; confirm swell return in a DAW with notes. |
| Vocal Presence Duck | 0.000 | 0.0000 | Zero output without sidechain. Confirm presence-band duck with a vocal on SC. |

## Hottest 15 (peak)

| Preset | Category | Peak | RMS |
|---|---|---|---|
| Lead Presence Stack | Guitar | 0.664 | 0.3057 |
| Brickwall Techno | Club | 0.661 | 0.4088 |
| Ironclad Split Bass | Metal | 0.652 | 0.4336 |
| CTZ Kick Master | Club | 0.648 | 0.4116 |
| Loudness Wall | Mastering | 0.646 | 0.3517 |
| Tight Gate Lead | Guitar | 0.644 | 0.2576 |
| Scooped Rhythm Wall | Guitar | 0.633 | 0.3679 |
| Baritone Serrated Stack | Metal | 0.620 | 0.4430 |
| Deathwall Tight Stack | Metal | 0.617 | 0.3883 |
| Rhythm Mid Scoop | Guitar | 0.614 | 0.2808 |
| Acid Hash | Club | 0.607 | 0.4863 |
| Lead Satin Clip | Distortion | 0.595 | 0.2940 |
| Hardstyle Sub Pummel | EDM | 0.592 | 0.2543 |
| Metal Scoop | Guitar | 0.592 | 0.3452 |
| Impact Designer | Sound Design | 0.592 | 0.3389 |

## Quietest 15 (peak)

| Preset | Category | Peak | RMS |
|---|---|---|---|
| Vocal Presence Duck | Mixing | 0.000 | 0.0000 |
| MIDI Bowed Resonator | Ambient | 0.031 | 0.0104 |
| Probability Break Chops | Drums | 0.084 | 0.0318 |
| Seeded Drift Filter | Sound Design | 0.100 | 0.0548 |
| MIDI Swell Return | Ambient | 0.113 | 0.0458 |
| Telephone Then Room | Lo-Fi | 0.113 | 0.0467 |
| Envelope Shaper | Dynamics | 0.124 | 0.0613 |
| Motion Blur Rumble Send | Industrial | 0.129 | 0.0710 |
| Bitcrush Glitch | Sound Design | 0.133 | 0.0521 |
| Drone Low Bed | Ambient | 0.133 | 0.0593 |
| Ambient Swell Guitar | Guitar | 0.136 | 0.0379 |
| Vibe Rotary Guitar | Guitar | 0.140 | 0.0511 |
| Broken Grid Chaos | Creative | 0.141 | 0.0704 |
| Broken Speaker | Lo-Fi | 0.148 | 0.0874 |
| Bass Synth Stack | Pitch | 0.154 | 0.0790 |

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
