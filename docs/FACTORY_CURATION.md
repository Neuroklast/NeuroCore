# Factory curation — 0.6.4-beta

The catalog currently contains 317 presets: 301 original entries, one consolidated near-duplicate, and 17 additions. Categories and control ranges describe intended use; preset loudness is not a substitute for level-matched comparison on the source recording.

## New programs

| Programs | Signal design / use |
| --- | --- |
| Blastbeat Front Edge, Snare Steel Crack | Fast/slow envelope contrast and parallel upper-mid saturation; close drums and dense percussion |
| Growl Iron Throat | Compressed dry vocal plus a band-limited distorted formant path |
| Deathwall Tight Stack, Baritone Serrated Stack | Two distinct DI amp architectures; pre-distortion bass control, supplied cabinet IRs, post-cab tone and peak control |
| Ironclad Split Bass | Clean compressed low band and distorted cabinet-filtered upper band |
| Forge Kick Overdrive | Crossover-separated fundamental and transient-shaped distorted upper body |
| Concrete Kick Fracture | Parallel wavefolding and saturation with DC/fizz filtering |
| Schranz Percussion Furnace | Compressed and saturated percussion highs with retained low band |
| Industrial Bass Excision | Moving midrange emphasis and wavefolding above an intact sub band |
| Motion Blur Rumble Send | Wet-only diffusion, moving low-mid emphasis, saturation and input-envelope ducking |
| Gated Rumble Machine Send | Wet-only rhythmic rumble with optional external detector ducking |
| Sub Harmonic Translation | Upper harmonics derived from bass for small-speaker audibility; does not create a sub-octave |
| Drum Crush Parallel | Controlled parallel sustain with low-frequency cleanup |
| Low Mid Congestion Control | Compression confined to the crossover middle band |
| Master Transient Glue | Gentle, slow-attack compression with conservative makeup |
| Master Side Low Cleanup | Side-only bass cleanup and optional side high-shelf adjustment |

## Existing catalog corrections

- Vocal De-Ess and De-Ess Wide retain the low-frequency voice while compressing the intended sibilance band. Vocal Presence and Vocal Warmth use broad EQ instead of filtering away the rest of the voice.
- Side Hall and Wide Canvas no longer feed anti-phase stereo into a mono-summing reverb input. Added side ambience cancels in mono; the dry center remains. Wide Canvas has a deliberately longer, darker range and separate center control.
- Dual Throw Lattice regains its wet paths through the corrected compiled bus-gain engine.
- Acid Line uses canonical arithmetic instead of the supported legacy `+` / `*` argument syntax. Vowel Filter now has parallel formants with meaningful spacing and resonance controls. The old Res macro was unused.
- Dark Atmosphere's Dark control now sets the input and noise/output filtering. Pulsing Electro's former Noise control now controls actual pulse depth and is named accordingly.
- Ring Then Verb removes DC before the hall. Vocal leveling thresholds now cover useful vocal input levels. Reverb channel selection and Widen mix are implemented in the DSP.
- Kick Rumble incorporates Warehouse Rumble's lower/longer settings. The established Kick Rumble name is retained; host projects containing previous scripts remain self-contained.
- Artist references were replaced with descriptive names. Downtuned Stack Cab explicitly selects its cabinet IR. Hardstyle Sub Pummel's former phase-angle claim is replaced with the actual Sub Delay control in milliseconds.
- Redundant generated comments were removed; structured macro metadata is authoritative.

## Validation

`tests/test_factory_curation.py` checks unique names, artist-reference removal, used macros, valid default ranges and noncanonical arithmetic arguments. The production-DSP audit (`NeuroKoreDspContracts --factory` / `--factory-stress`) loads selected cabinet IRs and renders stereo probes, macro endpoints and silence/tail transitions. The catalog has no parse failures or nonfinite samples in these probes. This is a numerical and routing check, not a listening-panel rating.

`NeuroKoreTests --factory-report` re-rendered all 363 factory presets on 2026-10-24: 0 apply errors, 0 nonfinite samples. The loudness floor still flags three inserts (Ambient Swell Guitar, MIDI Swell Return, Vocal Presence Duck). That queue is `docs/presets/2026-10-24-listening-prep.md`. pluginval Windows VST3 strictness 10 is recorded separately. Packaged macOS/Linux and a human listening pass remain for the release checkpoint. Existing global output sanitation remains responsible for final host-output peaks; internal floating-point bus peaks may exceed 0 dBFS.
