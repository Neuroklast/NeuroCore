# NEUROKORE

JUCE 8 / C++17 audio plugin (Standalone, VST3, AU). DSL signal chain. Product name NEUROKORE, code target NeuroKore.

## Commands

```bash
cmake -B build -S .
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe
```

Windows one-click: `build_release.bat` syncs living docs, builds `NeuroKore_All`, then runs `scripts/package_windows.ps1`. Debug: `build_debug.bat` (docs + Debug build, no package). Same tree (`build/`). The bats read `NEUROKORE_VERSION_LABEL` from CMake. Do not copy the label into the bats.

Version is **0.7.1-beta**. Do not invent a new one.

## Read before you touch code

| Task | Read |
|---|---|
| Any change | [workflow](docs/agents/workflow.md) |
| Circuit / cables / tidy / drag / connect | [ui-ux](docs/agents/ui-ux.md) |
| DSP / CPU / audio thread | [code-quality](docs/agents/code-quality.md) |
| Tests | [testing](docs/agents/testing.md) |
| Current truth | [DEVELOPMENT_STATUS](docs/DEVELOPMENT_STATUS.md) |

Do **not** read `docs/archive/` or the old session log unless you are hunting a specific past bug.

## Hard rules

1. One logical change. If Circuit is broken, do not also “fix” CPU, Settings, and docs in the same pass.
2. No patch on a patch. If the model is wrong, replace the model. Do not add `if (dest left)` or a new magic number.
3. Do not claim done unless a **contract test** failed first and now passes. Screenshots are the spec for UI.
4. Never allocate or copy `juce::String` on the audio thread.
5. Footer CPU is 0–100. Never print host-callback ratio as 173 %.
6. **Circuit is a headless graph.** `boardStore` holds nodes/ports/edges (ports are local centres). Chips are DOM; cables are one canvas. Camera is one matrix. Drag coordinates live in refs until pointerup. Connect is magnetic hitboxes, not React Flow Handles. After drop, Manhattan A* + 45° chamfer. DSL emit on commit only. Knob bind stays chrome, not graph cables.
7. User-facing change updates `docs/manual/NEUROKORE.md`, then `node scripts/sync_living_docs.mjs`. That file is the operator guide. `resources/UserManual_en.txt` is the same text. Do not edit the copy by hand.
8. **The VST3 class ID is frozen.** Never change `JUCE_VST3_CAN_REPLACE_VST2` from JUCE's default — it changes the class ID and breaks every host that cached the plug-in (FL Studio "failed to load", Cubase sessions). Silence the parameter-ID guard with `JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING` instead. The module binary inside `NEUROKORE.vst3/Contents/x86_64-win` is always `NEUROKORE.vst3` (no version). `moduleinfo.json` must list every class the factory exposes (component, controller, compatibility) with the CIDs `juce::VST3ClientExtensions` reports.
9. **UI layout.** If you change CSS, chrome, Face/Unit HUD, or visible text, obey `docs/agents/ui-ux.md` Layout Stability Contracts. Write a failing geometry contract first (Playwright `getBoundingClientRect` for overlap/shift). Do not treat `npm test` alone as UI-correct.
