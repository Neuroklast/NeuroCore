# UI performance results

Branch `perf/ui-baseline`. Version 0.6.8-beta.

## Measured and improved

Contract tests only. No invented frame-time percents. Native WebView2 / DAW not profiled.

| Change | Evidence |
|---|---|
| Opt-in `nk:ball` / `nk:cables` / `nk:bands` | `web/src/perf/marks.test.ts` |
| FFT hop reuses re/im/unwrap/Hann | `audioAnalyze.test.ts` (bit-identical + no `new Float32Array` in hop) |
| No per-frame containment `getError` | `containmentModel.test.ts` |
| Containment bind once, four draws | same |
| Cable gesture vsync coalesce | `cablePaint.test.ts` `shouldSkipClockPaint` |
| Hidden document does not tick viz | `vizClock.test.ts` `clockShouldTick` |

## Technically improved, not timed

- Footer / BoardChip / FaceSignalMeters / Knob local spin (React fan-out).
- Demo Circuit clips 8 Hz.
- Peak CSS unsubscribed when motion is off.

## Investigated, not done

- Face FFT already one hop per tick (`cachedBands`). No double DFT on the Unit ball.
- Offscreen copper traces: skipped; compositing risk vs glow.
- BallGl keep-alive across Unit/Circuit: skipped; unmount still `loseContext()`.

## Skipped for pixels

- Particle count, DPR, shader quality, spectrogram DFT→FFT, dropping JS or native onset.

## Native

Not run. Vite preview is not proof of audio-thread or WebView2 cost.
