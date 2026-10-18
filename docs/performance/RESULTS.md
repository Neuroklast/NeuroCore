# UI performance results

Branch `perf/ui-optimization`. Version 0.6.8-beta. Reference: current `main` plus this branch.

## Measured and improved (contract / CPU-side)

| Change | Evidence |
|---|---|
| Opt-in work/present/GPU percentiles, counters, HUD | `web/src/perf/*.test.ts` |
| Spectrogram hop is a 256-point rectangular FFT, peak bin unchanged | `scopeModel.test.ts`, `audioAnalyze.test.ts` `rectBinMags` |
| `cachedBands` invalidates on sample-rate change | `vizSnap.test.ts` |
| Decode path has no mapped readings array | `telemetry.test.ts` |
| No `while (getError)` and no `getError` in the BallGl frame loop | `containmentModel.test.ts` |
| Containment layout/theme uniforms only on change; shards packed in place | `containmentModel.test.ts` `packOrbitShards` |
| Circuit gesture paints coalesce; clock skips a pending rAF | `cablePaint.test.ts` |
| Diagnostics clock does not `setTick` | `coreDiagModel.test.ts` |
| Native onset still one impulse per tick at 30 and 60 fps | `particleAudio.test.ts` |
| Telemetry wait is interval minus fetch time | `workspace.test.ts` `telemetryWaitMs` |

Frame-time percents are **not invented**. Fill `docs/performance/BASELINE.md` from a `?nkperf=1` session on the target machine.

## Technically improved, not timed on GPU

- Field/dust/stir static uniforms set once per program bind.
- Scene FBO completeness checked at create; backing store resize noted only when dimensions change.
- One `getBoundingClientRect` per ball frame (passed into `fitCanvas`).
- UnitAnalyzer no longer re-renders ScopeCanvas/StereoField on every telemetry tick (stable typed arrays, clock reads the store).
- Face meters subscribe to `corr` instead of gonio arrays.
- BoardChip meters (lamp, foot dB, overload) are isolated subscriptions.
- `vizClock` records work vs present when nkperf is on.

## Investigated, not done

- Offscreen copper traces: skipped; compositing vs glow not proven pixel-identical.
- Keep BallGl GPU objects alive across Unit/Circuit: skipped; Unit still unmounts and `loseContext()` on tab leave (memory vs tab-switch latency; multiple plugin instances).
- CSS filter / backdrop-filter / CRT layer promotion: not changed without a native compositing profile.
- Scissor of containment fullscreen passes: not applied; bloom/glow can land outside a tight ball AABB.
- Replacing Hann `analyzeFrame` with a different FFT size: rejected; visual bins would move.

## Skipped for pixels

- Particle count, DPR, shader quality, dropping JS or native onset, lowering fps.

## Native

Not run. Vite preview is not proof of audio-thread or WebView2 cost. No extra audio-thread allocations or locks were added (web-only change).
