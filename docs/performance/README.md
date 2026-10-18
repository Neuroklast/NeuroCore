# UI performance marks

Opt-in only. Release builds do nothing unless `?nkperf=1` or `localStorage.nkperf=1`.

```
http://localhost:5173/?nkperf=1
```

Chrome DevTools → Performance. Filter measures named `nk:ball`, `nk:cables`, `nk:bands`.
An on-canvas HUD prints work / present / GPU percentiles twice a second.

| Measure | Work |
|---|---|
| `nk:ball` | One drawn Unit WebGL frame |
| `nk:cables` | One Circuit canvas paint |
| `nk:bands` | One telemetry-tick FFT hop (cache misses only) |
| work P50/P95/P99 | CPU time of one `vizClock` subscriber turn |
| present P50/P95/P99 | Wall interval between presented frames |
| gpu P50/P95/P99 | `EXT_disjoint_timer_query_webgl2` elapsed ms (async; disjoint results discarded) |

Two budgets:

1. **Work** — how long the actual rendering work takes.
2. **Smoothness** — how evenly frames are presented (drops = gap > 1.8× the fps budget).

FPS alone is not a budget. 60 FPS means 16.67 ms is a ceiling, not a target.

GPU queries never spin-wait. If the extension is missing, GPU percentiles stay at 0.

Native WebView2 / DAW numbers are **not measured** until Standalone is profiled the same way. Vite preview is not proof of native audio reactivity.

## Test matrix

Record the same window size, DPR, theme, motion, and fps cap for each row.

**Unit:** silence, constant sine, bass, wide stereo, fast transients, music, 30 and 60 FPS.

**Circuit:** short chain, dense graph, parallel buses, animated cables, pan, zoom, chip drag, knob bind, hover, cables+meters together.

**Terminal:** idle editor, typing, autocomplete, tab switch, large DSL.

Browser preview and native WebView2 are separate columns.
