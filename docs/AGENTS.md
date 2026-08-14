# NEUROKORE – Agent Guidelines

## Project Overview
- Product: NEUROKORE by Neuroklast (code targets stay NeuroCore)
- Stack: JUCE 8.0.6, C++17, CMake
- Formats: VST3, AU, Standalone
- Core idea: runtime-programmable DSL signal chain ("ShaderToy for audio")

## Critical Rules for Agents

### Audio Thread Safety (NON-NEGOTIABLE)
- NEVER allocate heap memory in `processBlock()` or audio-thread callpaths
- NEVER create/copy `juce::String`/`std::string` in audio thread
- NEVER block audio thread with mutexes/SpinLocks around heap-allocated state
- Use atomics/lock-free patterns for shared state between UI and audio thread
- Keep DSP objects prepared in `prepareToPlay()`; no per-sample object construction
- `variableNames` + `variableLock` are UI-thread concerns only; no usage in `processBlock()`

### UI Thread Rules
- All JUCE component access on Message Thread only
- Network operations must be async (`LicenseManager` async API)
- Use `juce::MessageManager::callAsync()` for UI callbacks from background work

### DSP Coding Standards
- No per-sample `getSubBlock()`/`ProcessContext` construction
- Recompute filter coefficients in `prepareToPlay()` when sample rate changes
- Always call `setLatencySamples()` after oversampling changes
- Keep `dryWetMixer.setWetLatency(latency)` aligned with oversampling latency
- Remove DC offset after DSL chain output (DC blocker high-pass ~20 Hz)

## Architecture Map

### Processing Pipeline
`Input -> InputRouter -> InputGain -> Oversampling Up -> DSL SignalChain -> DC Blocker -> NoiseGate -> SignalPolisher -> Lowpass -> Oversampling Down -> DryWetMixer -> AutoGain -> OutputGain -> Output`

### Main Modules
- `src/core/PluginProcessor.*`: APVTS, full DSP orchestration, preset/language integration
- `src/core/PluginEditor.*`: UI wiring and overlays
- `src/dsl/DSLParser.*`: parses DSL to blocks/params/aliases
- `src/dsl/SignalChain.*`: executes stage/filter/comp/env/osc blocks
- `src/utils/ExpressionEvaluator.*`: AST evaluator with constant folding/CSE/SIMD
- `src/dsp/*`: gain/router/filters/polisher/utils helpers
- `src/licensing/LicenseManager.*`: online/offline activation, machine binding
- `src/utils/PresetManager.*`: encrypted user preset load/save

## Known Issues to Avoid Reintroducing
- Blocking HTTP requests on Message Thread
- Audio-thread locks around `juce::String` data
- Per-sample heavy object creation in DSP hot paths
- Missing latency reporting after oversampling changes
- Missing wet-path latency compensation for dry/wet mix
- AU listed in CMake `FORMATS` but built only on Apple (JUCE drops it on Windows/Linux)
- AU type must stay `kAudioUnitType_MusicEffect` (`aumf`) so Logic routes MIDI
- AU binaries come from the `AU (macOS)` GitHub Actions job, not from a Windows build

## Build & Test
```bash
# Linux deps
./scripts/install_linux_deps.sh

# Configure/build
cmake -B build -S .
cmake --build build --config Release

# Tests
cmake --build build --target NeuroKoreTests
ctest --test-dir build --output-on-failure
```

## Required Session Workflow
1. Read: `docs/AGENT_WORKFLOW.md`, `docs/DEVELOPMENT_STATUS.md`, `docs/LESSONS_LEARNED.md`, `docs/ANALYSIS.md`
2. Plan first, then implement focused changes
3. Run tests for changed areas
4. Update `docs/DEVELOPMENT_STATUS.md` and `docs/LESSONS_LEARNED.md` in every session

## Mandatory PR Completion Steps (every session)
1. Optimize touched code paths where safe and measurable.
2. Perform legacy cleanup in directly touched code (remove dead/duplicate logic, reduce magic numbers).
3. Increase test coverage for touched behavior (new or extended unit tests).
4. Resolve and answer online PR comments before handoff.
5. Update docs and `README.md` for all behavior/format/workflow changes.

## DSL Quick Notes
- Blocks: `param`, `stage`, `filter`, `comp`, `env`, `osc`
- Common vars: `x`, `x_prev`, `y_prev`, `a..d`, `env1..N`, `osc1..N`, `t`, `sr`, `pi`
- Common funcs: `tanh`, `hardclip`, `fold`, `bitcrush`, `quantize`, `noise`, `clamp`, `smoothstep`
