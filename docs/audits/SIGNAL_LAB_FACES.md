# Signal Laboratory functional faces (PR C–C4)

**Commit base:** `main` `1337f50` (#296). Version stays **0.6.10-beta**.

`NodeFace` receives `{ kind, type, args, id }`. Every FACE_KINDS family paints a calculated glyph from ChipSpec args. Layout wrap stays PR D. Terminal navigator stays PR E. Factory `decay = 2.6` is unchanged.

## Model

- Faces read ChipSpec strings. A finite number is used; a formula (`cutoff = b`) uses the spec default. No `eval`, no knob-store subscription on every chip.
- Filter (SVF): analog lowpass / highpass / bandpass magnitude, Q 0.1–4. Allpass is the engine one-pole (`|H| = 1`).
- EQ: Audio EQ Cookbook biquads at 48 kHz, same shapes as `juce::dsp::IIR::ArrayCoefficients`.
- Reverb: engine RT60 proxy `0.4 + size * 1.2 + decay * 4.5` seconds, clamped 0.2–10. Decay clamped 0.05–0.95 (process path). Mix scales amplitude. Damp is not in that RT60 formula.
- Delay: dry at 0, then wet · fb^(n−1) at n · time (engine write/read). Sync notes use 120 BPM. Time axis 0–2000 ms.
- LFO: one cycle of `lfoWave` scaled by depth. Header lamp still pulses at live Hz. No knobs-store subscribe on the plate.
- Dynamics: Comp `computeGrDb` + makeup + mix. Limit is a ceiling wall. Gate is threshold + range (−80 default). OTT/deesser/transient are ChipSpec-driven, not live GR.
- Env: linear attack, hold plateau, exponential release, then min/max/invert. Header lamp is the live follower.
- Pitch: octaver sub/dry/up stems (engine wet mix); pitch one stem at semitones −24…24; vocoder N=bands stems. No spectrum.
- Drive: leading named shaper only (softclip atan, tube 12AX7, diode asinh, hardclip n=24, fold 8-iter, bitcrush). Nested calls use the outer name. Unknown `y` is identity. No eval.
- Routing: ChipSpec ports as west→east stems. Width is two parallel lanes (side amp = width/1.4). Join uses mix. No decorative waveform.
- Modfx: one engine LFO cycle. Phaser/flanger sine (flanger invert flips sign; phaser depth clamped to 1 on the plot). Chorus averages voice sines × depth × 0.4. Header lamp stays clip.
- Cab: dry/wet mix stems. IR WAV name is `hostStore.irSlots` keyed by node id, never a formula.
- IO: ChipSpec contacts as a schematic. Real jacks stay on the frame. BoardChip always mounts NodeFace.
- Utility: gain vertical tick, pan on an L–C–R baseline. No invented meter.
- Closed AABB and jack locals stay put. The SVG sits in the existing plot well.

## Contracts

Vitest: `filterResponse.test.ts`, `reverbDecay.test.ts`, `delayTaps.test.ts`, `lfoWaveform.test.ts`, `dynamicsCurve.test.ts`, `envEnvelope.test.ts`, `pitchIntervals.test.ts`, `driveTransfer.test.ts`, `routingMap.test.ts`, `modfxWaveform.test.ts`, `cabMix.test.ts`, `ioJacks.test.ts`, `utilityParams.test.ts`, `NodeFace.test.ts`, `chipChrome.test.ts`, `faceKind.test.ts`.

Playwright `web/e2e/signal-lab-chrome.spec.ts` (chromium-1x): filter/reverb curves; Chorus Delay taps+waveform; Vocal Comp curve; Drone octaver; Vocal Comp diode; Warehouse Rumble env; Phaser Lab waveform; Mesa High Gain cab mix; Metal Gate width map; Drone IN/OUT jack.

## Screenshots

Reuse `docs/audits/signal-lab-chrome/` after the curves land (idle Drone, selected filter, Pad Cloud Hall).

## Out of scope

LFO travel motion (PR G). Serial wrap (PR D). Terminal navigator (PR E). Native DAW. Performance BASELINE.md fill.
