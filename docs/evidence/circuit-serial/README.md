# Directed serial composition

2026-10-20, NEUROKORE 0.7.1-beta. The housing, material, dotted field, names, typecodes and metal pins are retained. This change addresses composition and readability.

A structurally verified single IN→OUT path now has its own placement strategy. Arrange and Compact both place it on a continuous eastbound rail. OUT is placed directly from its predecessor and jack. Each hop has a 64 px gutter with aligned local ports. View width changes camera fit, not signal direction. Branched, modulation, bus, cyclic or incomplete graphs use the general graph strategy. Rerouting user placements never applies automatic placement.

DSP housings start at 320×256 and grow for real captions and controls. Drone Low Bed uses Octaver 352, Filter 320, Reverb 320. Parameter captions use 15 px text and contact-aligned guide ticks. The bounded dot field uses a 1.1 px radius with 40% ink so it survives camera scaling. Idle function plots use 0.58 opacity, focused 0.78. There is no new glow or material redesign.

## Measurements

Both sets use the real factory script, board hydration, placement and A* production pipeline. Baseline is an isolated checkout of unchanged `f18103c8cac5c35f7b78e99ab95b12475aaef91c`. Distances are logical board pixels, not screenshot pixels. Turn counts include the rendered chamfer segments.

| Metric | Before, 960 | After, 960 | Before, 1440 | After, 1440 |
|---|---:|---:|---:|---:|
| Total cable length | 1600 | 256 | 2656 | 256 |
| Longest hop | 1216 | 64 | 1568 | 64 |
| Westward travel | 736 | 0 | 1088 | 0 |
| Turns | 6 | 0 | 6 | 0 |
| Widest / narrowest DSP | 1.571 | 1.100 | 1.571 | 1.100 |
| Chip collisions | — | 0 | — | 0 |
| Cable/body collisions | — | 0 | — | 0 |
| Cable crossings/overlaps | — | 0 | — | 0 |

Raw data: [before 960](before-960.json), [after 960](metrics-960.json), [before 1440](before-1440.json), [after 1440](metrics-1440.json). Every hop is measured. The old same-row exemption was removed from `factorySignalFlow.test.ts`. The Web layout workflow now runs these signal-flow contracts explicitly and uploads their generated metrics alongside browser evidence.

## Browser views

- [960×860 screenshot](serial-960-1x.png), [DOM geometry](dom-960-1x.json)
- [1440×860 screenshot](serial-1440-1x.png), [DOM geometry](dom-1440-1x.json)
- [Effective caption and dot sizes at both densities](browser-metrics.json)

These are captures of the production bundle with the existing browser demo telemetry, not native DAW captures. At 960 px the smallest caption is 8.48 screen pixels and the dot diameter is 1.24 screen pixels. At 1440 px they are 11.30 and 1.66 screen pixels. Plots retain their calculated amplitude, including the wet mix of the reverb envelope.

## Validation

- New geometry contracts and browser DOM contracts reproduced the prior failures first.
- Production build including TypeScript passed. The exact new CI signal-flow command also passed all 11 contracts.
- Complete assemble suite: 429 passed, 2 failed across 79 files. The two failures were independently reproduced on unchanged main: `boardView.regression.test.ts` checks the existing knobs-array subscription, and `chipSpec.test.ts` expects an outdated catalog parameter count. No new failures remain.
- The broad browser matrix passed 18 tests, with 2 existing density duplicates skipped. It covers Drone Low Bed, Pad Cloud Hall, Bass Fractionator, four themes, resize/zoom, selection, modulation drag, Source geometry and camera lock. The new browser contract uses actual DOM rectangles at 960×860 and 1440×860, checks directed order, rail alignment, relative widths, caption/pin alignment, and at least 8 screen pixels of effective parameter text size. Both pixel densities are covered. The final six browser contracts passed after the dot-size correction. A separate failing contract reproduced the subpixel dot issue before it was corrected.

The camera still fits an entire serial graph. Very long chains therefore need user zoom/pan for detailed editing. This is the explicit one-row strategy, not an attempt to solve directed multirow composition by hiding a return cable.

Native DAW/WebView2 review and native GraphModel/AstJson binaries were not run in this environment. No DSP or version changes and no native binary rebuild.
