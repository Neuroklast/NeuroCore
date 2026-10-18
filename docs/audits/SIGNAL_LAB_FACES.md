# Signal Laboratory functional faces (PR C)

**Commit base:** `main` `118c768` (#293). Version stays **0.6.10-beta**.

`NodeFace` receives `{ kind, type, args }`. Filter/EQ, reverb, delay, LFO, and dynamics paint calculated glyphs from those args. Env, pitch, drive, routing, modfx, cab stay empty pads. Layout wrap stays PR D. Terminal navigator stays PR E. Factory `decay = 2.6` is unchanged.

## Model

- Faces read ChipSpec strings. A finite number is used; a formula (`cutoff = b`) uses the spec default. No `eval`, no knob-store subscription on every chip.
- Filter (SVF): analog lowpass / highpass / bandpass magnitude, Q 0.1–4. Allpass is the engine one-pole (`|H| = 1`).
- EQ: Audio EQ Cookbook biquads at 48 kHz, same shapes as `juce::dsp::IIR::ArrayCoefficients`.
- Reverb: engine RT60 proxy `0.4 + size * 1.2 + decay * 4.5` seconds, clamped 0.2–10. Decay clamped 0.05–0.95 (process path). Mix scales amplitude. Damp is not in that RT60 formula.
- Delay: dry at 0, then wet · fb^(n−1) at n · time (engine write/read). Sync notes use 120 BPM. Time axis 0–2000 ms.
- LFO: one cycle of `lfoWave` scaled by depth. Header lamp still pulses at live Hz. No knobs-store subscribe on the plate.
- Dynamics: Comp `computeGrDb` + makeup + mix. Limit is a ceiling wall. Gate is threshold + range (−80 default). OTT/deesser/transient are ChipSpec-driven, not live GR.
- Closed AABB and jack locals stay put. The SVG sits in the existing plot well.

## Contracts

Vitest: `filterResponse.test.ts`, `reverbDecay.test.ts`, `delayTaps.test.ts`, `lfoWaveform.test.ts`, `dynamicsCurve.test.ts`, `NodeFace.test.ts`, `chipChrome.test.ts`.

Playwright `web/e2e/signal-lab-chrome.spec.ts` (chromium-1x): filter/reverb curves; Chorus Delay taps+waveform; Vocal Comp curve.

## Screenshots

Reuse `docs/audits/signal-lab-chrome/` after the curves land (idle Drone, selected filter, Pad Cloud Hall).

## Out of scope

Env/pitch/drive/routing/modfx/cab faces. LFO travel motion (PR G). Serial wrap. Terminal navigator. Native DAW. Performance BASELINE.md fill.
