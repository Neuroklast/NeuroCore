# Signal Laboratory functional faces (PR C)

**Commit base:** `main` `118c768` (#293). Version stays **0.6.10-beta**.

`NodeFace` receives `{ kind, type, args }`. Filter/EQ and reverb paint calculated curves from those args. Delay, LFO, dynamics, and the rest stay empty pads. Layout wrap stays PR D. Terminal navigator stays PR E. Factory `decay = 2.6` is unchanged.

## Model

- Faces read ChipSpec strings. A finite number is used; a formula (`cutoff = b`) uses the spec default. No `eval`, no knob-store subscription on every chip.
- Filter (SVF): analog lowpass / highpass / bandpass magnitude, Q 0.1–4. Allpass is the engine one-pole (`|H| = 1`).
- EQ: Audio EQ Cookbook biquads at 48 kHz, same shapes as `juce::dsp::IIR::ArrayCoefficients`.
- Reverb: engine RT60 proxy `0.4 + size * 1.2 + decay * 4.5` seconds, clamped 0.2–10. Decay clamped 0.05–0.95 (process path). Mix scales amplitude. Damp is not in that RT60 formula.
- Closed AABB and jack locals stay put. The SVG sits in the existing plot well.

## Contracts

Vitest: `filterResponse.test.ts`, `reverbDecay.test.ts`, `NodeFace.test.ts`, `chipChrome.test.ts`.

Playwright `web/e2e/signal-lab-chrome.spec.ts` (chromium-1x): filter and reverb `path[data-curve]` present and distinct; Drone reverb `data-decay=0.95`.

## Screenshots

Reuse `docs/audits/signal-lab-chrome/` after the curves land (idle Drone, selected filter, Pad Cloud Hall).

## Out of scope

Delay taps, LFO/env motion, dynamics curves. Serial wrap. Terminal navigator. Native DAW. Performance BASELINE.md fill.
