# Preset/control validation — 2026-10-22

Base: main d07c0ba1c3dc; branch feat/preset-curation-modulation; PR #328.

## Presets

340 originals individually inventoried/reviewed; 353 active after curation (307 keep, 21 revised, 12 deprecated, 25 new). 33 complete originals archived. Mesa protected-object hash unchanged. Full per-entry decisions and measurements: docs/presets/2026-10-22-review.json/.md. The three MSEG examples are Curved Sync Duck, MIDI Bowed Resonator and Bipolar Formant Arc.

| Final native profile | Entries | Parse/nonfinite errors | Maximum sample peak |
| --- | ---: | ---: | ---: |
| Default broadband | 353 | 0 | 1.43003 |
| Transients | 353 | 0 | 1.48373 |
| 4× hot input | 353 | 0 | 1.60884 |
| Silence | 353 | 0 | 0.02079 |
| Macro extremes | 353 | 0 | 1.71475 |

Production SignalChain, 48 kHz stereo, 256-sample blocks, running transport/120 BPM, requested MIDI/sidechain. All final-engine entries rendered; the final corrected bandpass example was re-rendered on default/transient/hot and its measurements substituted by name, while silence/stress used the final catalog directly. Silence output includes explicitly documented Dark Atmosphere noise generation. These are offline test signals and sample peaks, not human listening, perceptual uniqueness proof, true peak or LUFS certification.

## Contracts

- Node curation: 5 pass (all current control sources represented, exact Mesa object, exact deprecated originals, idempotence).
- Focused Vitest: 62 pass (metadata, point model, control pins, offline sketches, Add, factory signal layout/catalog, resolved Inspect defaults and valid runtime binding targets).
- Production TypeScript/Vite build: pass.
- Full web suite: 1020 pass / 10 fail. Clean main comparison: 1001 pass / the identical 10 failures. Baseline exported from git and run with the same installed dependencies. No new failing case. Existing failures: boardView regression source assertion, chipSpec comp snapshot, OTT visualEdges snapshot, hostStore cable default, chromeSpec, circuitCssInventory, crtFx, signalLab, theme tokens and NodeFace source assertion.
- Native --control-integration: pass. Audio Join GraphOp A/B + AST restore; control/audio rejection; nested source shaping; ModMix A/B/cancellation/offset/normalize/clamp/restore; MSEG duration/BPM/MIDI/audio/restore/buffer independence; allocation probe reports zero allocations across 32 warmed processing blocks.
- Portable MsegContract: pass (curved interpolation, sustain/release, loops, retrigger, invalid/range/nonfinite data, bounded extreme increment).
- Complete headless FX regression: zero failures.
- Realtime source guard: 2 pass.
- Playwright: ModMix live output/geometry, real cable magnetization for both A/B inputs on ModMix vs audio Join, and MSEG add/drag/keyboard/delete/curve editing at 1× and 2×. The final drag rerun passes after awaiting the layout progress indicator; initial race at 2× used stale pin coordinates. Initial face/editor contracts failed before implementation. Expanded-block default controls and binding targets have failing-before/passing-after model checks; the browser checks default Offset value and its bounds.

## Native host acceptance

A branch Windows workflow builds the VST3 and full native suite, runs pluginval strictness 5, and retains the binary and logs as an Actions artifact. Result pending. No real Windows DAW listening/MIDI/UI acceptance is claimed. MSEG is the initial C08 implementation; BPM synchronization is rate-based and does not lock to PPQ/transport.
