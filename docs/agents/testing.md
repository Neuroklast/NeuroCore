# Testing

The suite has tens of thousands of `expect()` calls, many per-sample. That hides layout and CPU regressions. New work uses **contracts** (few asserts) and **visual dumps** (geometry, not PNGs).

## Contract tests

Circuit layout contracts live in `web/src/assemble/*.test.ts` (`boardStore` / elkjs / A*), not in C++.

A C++ contract is one named check with a dump on failure. Do **not** add another `expect` inside a 512-sample loop. Use `TestHelpers::countNonFinite` / `peakAbs`.

Hard contracts (must stay green):

- `cpuDisplayPercent(1.73f) == 100`
- GraphModel emit/parse + jack connect (AstJson / GraphOps)

## Visual tests

No golden PNG in CI (host scale, fonts, GPU). Circuit geometry is asserted in web Vitest. The web-layout workflow runs the production build and those Vitest contracts. It does not install or run Playwright. `npm run test:layout` stays a local browser check.

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

## Native board endpoint audit

`tests/dsp/NativeGraphExport.cpp` accepts the generated factory catalog JSON, parses each script with the production GraphModel, verifies its AstJson semantic round trip and writes `[{name,ast}]` JSON to stdout. Build it with GraphModel.cpp, DSLParser.cpp, AstJson.cpp and JUCE core using `tests/dsp/core-only/JuceHeader.h`. Newer JUCE releases also require juce_core_CompilationTime.cpp.

Set `NK_NATIVE_AST_AUDIT` to the exported JSON path and run `npx vitest run src/assemble/nativeAst.audit.test.ts` from `web`. This optional integration audit checks actual host AST endpoint resolution; it is skipped when the export is absent. The always-on factoryConnectionIntegrity contracts independently check the browser sketch.
