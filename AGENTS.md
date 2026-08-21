# NEUROKORE

JUCE 8 / C++17 audio plugin (Standalone, VST3, AU). DSL signal chain. Product name NEUROKORE, code target NeuroKore.

## Commands

```bash
cmake -B build -S .
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe
```

Windows one-click: `build_release.bat` (Debug: `build_debug.bat`). Same tree (`build/`) and target (`NeuroKore_All`). If you bump `NEUROKORE_VERSION_LABEL` or the Release target, update those `.bat` files in the same change.

Version is **0.5.1-alpha**. Do not invent a new one.

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
6. **Drag and connect are React Flow.** Chip move = `nodesDraggable` + `snapToGrid`. Audio jack connect = `onConnect` + `Handle` + `getStraightPath` while dragging. Knob bind lives outside the graph (chrome knobs) — preview is RF `getSmoothStepPath` via `bindSmoothPath`, not a second A*. If RF already does it, use it.
