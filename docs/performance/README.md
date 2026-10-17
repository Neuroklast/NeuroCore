# UI performance marks

Opt-in only. Release builds do nothing unless `?nkperf=1` or `localStorage.nkperf=1`.

```
http://localhost:5173/?nkperf=1
```

Chrome DevTools → Performance. Filter measures named `nk:ball`, `nk:cables`, `nk:bands`.

| Measure | Work |
|---|---|
| `nk:ball` | One drawn Unit WebGL frame |
| `nk:cables` | One Circuit canvas paint |
| `nk:bands` | One telemetry-tick FFT hop (cache misses only) |

Record P50 / P95 / P99 of those measures. FPS alone is not a budget.

Native WebView2 / DAW numbers are **not measured** until Standalone is profiled the same way. Vite preview is not proof of native audio reactivity.
