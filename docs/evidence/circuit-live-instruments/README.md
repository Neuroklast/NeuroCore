# Circuit live instruments verification

2026-10-22, version 0.7.2-beta. Branch `feat/circuit-live-instruments`, based on main `276b5d4` after #326. No native binary or installer rebuilt.

## User story and data boundary

The existing JUCE bridge supplies AST and host events. The board renders StepSeq position and each meter's own Peak/RMS. Inspect resolves implicit IN/OUT terminals and reads their plugin input/output taps. CableCanvas consumes source snapshots once, then animates measured energy on cached routes using the shared visual clock.

Browser fixtures mock the native bridge, not the production stores/components/renderer. They do not prove live DAW telemetry or native performance.

## Regressions reproduced before implementation

- The sequencer renderer used filled cyan bars, rather than a theme LED matrix.
- Meter rendered the generic gain/pan glyph rather than measured levels.
- OUT Inspect returned `No node selected`.
- Inspect measured 1260 px wide in a 1280 px viewport because the global max-width rule won.
- Long-path pulses expired at two seconds; another contract reproduced travel stopping at 374/1000 px after silence slowed the stream.
- Disabling motion retained old in-flight pulses after the route-based lifetime change. The final contract verifies discarding those pulses.

## Validation

- Production web build and TypeScript pass.
- Final focused Vitest: 53 passed across live instruments, board legend, cable signal/paint/motion and Inspect.
- Browser Circuit suite: 12 passed, Chromium at 1× and 2×, including all four themes, 64 LEDs in eight rows, active theme glow, fixed Inspect width/dial geometry, OUT measurements, close-button stability, meter source isolation and geometry, and moving Dots at 8/100/200 modules.
- Existing StepSeq editing browser suite: 2 passed at 1×/2×, covering dragging, mute and live step updates.
- Broad Circuit/overlay regression run before the final extra motion contracts: 551 passed, 4 failures. The same four failures reproduce unchanged on base main: `boardView.regression` knobs-array source scan, `chipSpec` catalog expectation, `visualEdges` OTT detector/input expectation, `NodeFace` exact host-object source scan.
- Screenshots were inspected for the inset meter, bounded Inspect and directional glowing traces. Playwright writes screenshots and timing JSON to its test results.

## Paint timing

Production CableCanvas performance marks. 1280×860 viewport, shared clock requested at 60 Hz, repeated bridge snapshots with constant Peak/RMS. After 400 ms warm-up, collect 750 ms of paint measures and compare canvas pixels between samples. Moving pixels must be detected despite the constant input energy. These are JS/Canvas submission times, not a GPU/DAW FPS guarantee.

| Modules | Density | Frames sampled | Median paint | p95 paint |
| ---: | ---: | ---: | ---: | ---: |
| 8 | 1× | 45 | 0.3 ms | 0.5 ms |
| 100 | 1× | 44 | 1.1 ms | 1.9 ms |
| 200 | 1× | 44 | 1.7 ms | 2.6 ms |
| 8 | 2× | 47 | 0.2 ms | 0.4 ms |
| 100 | 2× | 46 | 0.7 ms | 1.4 ms |
| 200 | 2× | 46 | 1.4 ms | 2.4 ms |

The steady-flow timings precede only the final motion-pause clearing guard, which has its own failing/passing contract. Final production rebuild and a two-density Dots smoke check are run after that guard.

## Reproduce

From `web`:

```sh
npm ci
npm run build
npx vitest run src/assemble/liveInstruments.test.ts src/assemble/boardLegend.test.ts src/assemble/cableSignal.test.ts src/assemble/cablePaint.test.ts src/assemble/cableMotion.test.ts src/overlays/inspectModel.test.ts src/overlays/inspectTerminal.test.ts
npx playwright test e2e/circuit-live-instruments.spec.ts e2e/stepseq-editor.spec.ts
```

Install Playwright Chromium or set `NK_CHROMIUM` to a compatible local Chromium executable. Native GraphModel/AstJson and DAW/WebView2 checks remain unrun. Rebuild the plugin to embed the new web assets.

## Connector evaluation

See [CIRCUIT_CONNECTOR_EVALUATION](../../design/CIRCUIT_CONNECTOR_EVALUATION.md). No port placement, router or chip-size change is included.
