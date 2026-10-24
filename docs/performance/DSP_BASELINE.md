# DSP / host-callback baseline

Measured 2026-10-24 on Windows, Release `NeuroKoreTests.exe`.
CPU: Intel Core i7-14700KF. Reference commit: `81e8bf5` plus this bench.

This is **offline**. No DAW, no WebView2, no ASIO. `budget_pct` is `p99 / (block/sr)` for the **host** callback. Footer CPU is still 0–100 via `cpuDisplayPercent`. Do not treat these rows as a Cubase/FL meter.

Script for `--bench-host`: one `stage1: y = tanh(x * a)`. Four instances run **serially on one thread** (one core servicing four plugins).

Raw CSV: `build/validation/bench-host.csv`, `build/validation/bench-dense.csv`.

## Host callback (`--bench-host`)

| Instances | SR | Block | OS | p50 µs | p99 µs | Budget µs | Budget % | Nonfinite |
|---|---|---|---|---|---|---|---|---|
| 1 | 48000 | 32 | 1 | 5.1 | 5.4 | 667 | 0.8 | 0 |
| 1 | 48000 | 32 | 8 | 36.1 | 40.3 | 667 | 6.0 | 0 |
| 1 | 48000 | 64 | 1 | 9.4 | 13.2 | 1333 | 1.0 | 0 |
| 1 | 48000 | 64 | 8 | 72.4 | 101 | 1333 | 7.6 | 0 |
| 1 | 48000 | 256 | 1 | 34.4 | 37.8 | 5333 | 0.7 | 0 |
| 1 | 48000 | 256 | 8 | 287.6 | 337.1 | 5333 | 6.3 | 0 |
| 1 | 96000 | 32 | 1 | 5.1 | 5.2 | 333 | 1.6 | 0 |
| 1 | 96000 | 32 | 8 | 36.7 | 41.3 | 333 | 12.4 | 0 |
| 1 | 96000 | 64 | 1 | 9.5 | 9.9 | 667 | 1.5 | 0 |
| 1 | 96000 | 64 | 8 | 71.7 | 75.3 | 667 | 11.3 | 0 |
| 1 | 96000 | 256 | 1 | 35 | 38.8 | 2667 | 1.5 | 0 |
| 1 | 96000 | 256 | 8 | 285.8 | 350.5 | 2667 | 13.1 | 0 |
| 4 | 48000 | 32 | 1 | 20.3 | 23.5 | 667 | 3.5 | 0 |
| 4 | 48000 | 32 | 8 | 145.4 | 180.8 | 667 | 27.1 | 0 |
| 4 | 48000 | 64 | 1 | 37 | 40.1 | 1333 | 3.0 | 0 |
| 4 | 48000 | 64 | 8 | 287.7 | 318.7 | 1333 | 23.9 | 0 |
| 4 | 48000 | 256 | 1 | 136.5 | 245.6 | 5333 | 4.6 | 0 |
| 4 | 48000 | 256 | 8 | 1132.5 | 1256.5 | 5333 | 23.6 | 0 |
| 4 | 96000 | 32 | 1 | 20.4 | 24.3 | 333 | 7.3 | 0 |
| 4 | 96000 | 32 | 8 | 144.4 | 171.6 | 333 | 51.5 | 0 |
| 4 | 96000 | 64 | 1 | 38.1 | 40.1 | 667 | 6.0 | 0 |
| 4 | 96000 | 64 | 8 | 290.3 | 375.7 | 667 | 56.4 | 0 |
| 4 | 96000 | 256 | 1 | 135.8 | 166.6 | 2667 | 6.2 | 0 |
| 4 | 96000 | 256 | 8 | 1147.3 | 1265.6 | 2667 | 47.5 | 0 |

Heaviest measured host row: 4 instances, 96 kHz, 64 samples, 8× OS, p99 56 % of that callback. Still finite. A dense factory graph in a DAW will cost more.

## Dense SignalChain (`--bench-dense`)

DSL-only, host block 256 at 48 kHz budget, OS domain simulated as 48k×factor. 200 serial stages at 8× exceed that budget (p99 ~308 %). That is the graph, not one tanh stage.

| Scenario | Nodes | OS | p50 µs | p99 µs | Budget % | Nonfinite |
|---|---|---|---|---|---|---|
| serial | 10 | 1 | 50.4 | 54 | 1.0 | 0 |
| serial | 10 | 8 | 394.3 | 482.7 | 9.1 | 0 |
| serial | 50 | 1 | 238 | 268.1 | 5.0 | 0 |
| serial | 50 | 8 | 1885.2 | 2037.1 | 38.2 | 0 |
| serial | 100 | 1 | 473 | 528.1 | 9.9 | 0 |
| serial | 100 | 8 | 3749.3 | 4054.1 | 76.0 | 0 |
| serial | 200 | 1 | 944.5 | 1058.2 | 19.8 | 0 |
| serial | 200 | 8 | 13249 | 16438.1 | 308.2 | 0 |
| modulators | 128 | 1 | 441.7 | 1234.3 | 23.1 | 0 |
| modulators | 128 | 8 | 3447.2 | 6161.9 | 115.5 | 0 |
| multibus | 80 | 1 | 648.4 | 1296.9 | 24.3 | 0 |
| multibus | 80 | 8 | 5271.5 | 7899.7 | 148.1 | 0 |

UI `docs/performance/BASELINE.md` is still empty: no `?nkperf=1` Standalone session on this machine.
