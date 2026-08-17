# Testing

The suite has tens of thousands of `expect()` calls, many per-sample. That hides layout and CPU regressions. New work uses **contracts** (few asserts) and **visual dumps** (geometry, not PNGs).

## Contract tests

File: `tests/CircuitContractTest.h`  
Helpers: `tests/CircuitContracts.h`

A contract is one named check with a dump on failure:

```cpp
expect (CircuitContracts::inIsTopLeft (hint, doc),
        CircuitContracts::dump (hint, doc));
```

Hard contracts (must stay green):

- No `kind == knob` jacks
- Chips snap to 16 and do not overlap
- Simple chain `stage → filter`: IN left of first chip, OUT right of last
- `cpuDisplayPercent(1.73f) == 100`
- Fold hit contains the painted chevron
- Router: same-row east-west has 0 turns; `pathCost` prefers 2 turns over 4

Soft / open (logged, listed in DEVELOPMENT_STATUS until the tidy refactor):

- Phaser Lab: IN unique top-left, OUT unique bottom-right, no board-wrapping main cable
- Settings: License / Help height ≥ 26 px

Do **not** add another `expect` inside a 512-sample loop. Use `TestHelpers::countNonFinite` / `peakAbs`.

## Visual tests

No golden PNG in CI (host scale, fonts, GPU). Visual = **text board**:

```
IN(16,16)  stage1(240,16)  filter1(464,16)  OUT(688,16)
```

`CircuitContracts::dump` prints that. On failure the agent sees the board, not “Test 8 failed”.

When you change tidy or the router, update the simple-chain dump if the positions are still legal (IN left, OUT right, no overlap). Do not snapshot Phaser Lab’s broken board as the desired image.

## What to run

| You changed | Run |
|---|---|
| Graph / tidy / router / canvas | `NeuroKoreTests` is enough; read CircuitContract + GraphModel + PcbRouter output |
| One DSP block | That block’s test file + CrackleFixes if it can click |
| Factory JSON | FactoryLoudness + semantic parse only |
| Unrelated | Do not wait 30 s for 28k expects unless you touched `main.cpp` includes |

Full suite before a user-facing binary.

## Writing a new test

1. Name the contract in one sentence.
2. Arrange the smallest script that can violate it.
3. `expect (contract, dump)`.
4. If it is still an open screenshot bug, put it in the soft list and in DEVELOPMENT_STATUS — do not mark the Circuit checklist `[x]`.
