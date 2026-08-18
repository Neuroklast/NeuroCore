# Testing

The suite has tens of thousands of `expect()` calls, many per-sample. That hides layout and CPU regressions. New work uses **contracts** (few asserts) and **visual dumps** (geometry, not PNGs).

## Contract tests

Circuit layout contracts live in `web/src/assemble/*.test.ts` (React Flow / elkjs), not in C++.

A C++ contract is one named check with a dump on failure. Do **not** add another `expect` inside a 512-sample loop. Use `TestHelpers::countNonFinite` / `peakAbs`.

Hard contracts (must stay green):

- `cpuDisplayPercent(1.73f) == 100`
- GraphModel emit/parse + jack connect (AstJson / GraphOps)

## Visual tests

No golden PNG in CI (host scale, fonts, GPU). Circuit geometry is asserted in web Vitest.

## What to run

| You changed | Run |
|---|---|
| Graph / tidy / web circuit | `web` Vitest (`assemble/*.test.ts`) + `GraphModelTest` / `AstJsonTest` |
| One DSP block | That block’s test file + CrackleFixes if it can click |
| Factory JSON | FactoryLoudness + semantic parse only |
| Unrelated | Do not wait 30 s for 28k expects unless you touched `main.cpp` includes |

Full suite before a user-facing binary.

## Writing a new test

1. Name the contract in one sentence.
2. Arrange the smallest script that can violate it.
3. `expect (contract, dump)`.
4. If it is still an open screenshot bug, put it in the soft list and in DEVELOPMENT_STATUS — do not mark the Circuit checklist `[x]`.
