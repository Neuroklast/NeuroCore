# UI performance baseline

Fill this sheet from a real `?nkperf=1` session. Do not invent percents.

| Field | Value |
|---|---|
| Reference commit | |
| OS | |
| CPU | |
| GPU / driver | |
| Host | Vite preview / Standalone / VST3 in … |
| Window | 1280×860 CSS, DPR |
| Theme / motion / fps | |
| Test signal | |
| Duration | |

## Metrics

| View | Signal | Work P50 | Work P95 | Work P99 | Present P50 | Present P95 | Present P99 | Drops | GPU P50 | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| Unit | silence 60 | | | | | | | | | |
| Unit | sine 60 | | | | | | | | | |
| Unit | transients 60 | | | | | | | | | |
| Unit | music 60 | | | | | | | | | |
| Unit | music 30 | | | | | | | | | |
| Circuit | small 60 | | | | | | | | | |
| Circuit | dense + pan 60 | | | | | | | | | |
| Terminal | idle | | | | | | | | | |

Long tasks: list any >50 ms from the Performance panel.

Memory: heap after 60 s (Performance monitor or `performance.memory` when available).

Native WebView2 / DAW: not filled until a Standalone session with `nkperf=1` is recorded on the target machine.

Synthetic browser telemetry is not proof of native audio reactivity.
