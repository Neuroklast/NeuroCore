# Code quality

## Audio thread

- No `new`, no `juce::String` / `std::string` copy, no heap lock in `processBlock` or anything it calls.
- Prepare in `prepareToPlay`. No per-sample `ProcessContext` / `getSubBlock` / `evaluate()` (locked).
- Modulated filters: control-rate coeffs (`kFilterCoeffStride`). A 0.35 Hz LFO is not a reason to rebuild TPT every sample at 8×.
- Dry/CPU-hold: never splice raw input. Last wet or silence. Host PDC still applies.

## CPU meter

- `observe` measures `secondsUsed / blockBudget`. That can be > 1.
- UI must call `Config::cpuDisplayPercent`. SAFE-hold calls `noteHoldDisplay()` so the bar does not freeze on the last wet overrun.

## Models, not patches

| Smell | Do this instead |
|---|---|
| `if (dest.x < src.x)` in the router | Port facing + one pattern (HVH / wrap-U) |
| `dropMicroJogs(4)` | Pins on the grid |
| Knob Bezier + audio Manhattan | No knob nets. Labels on the chip |
| `tidyLayout` special-case “Phaser Lab” | One rank/rail function for every script |
| Duplicate keys in `editableArgKeys` | One table per block type + existing args |

## Where code lives

- `src/dsl/GraphModel.*` — document, jacks, tidy, emit/parse. Geometry contracts belong here, not in the canvas.
- `src/dsl/PcbRouter.*` — orthogonal paths. No JUCE types.
- `src/ui/GraphCanvasComponent.*` — paint + hit. If you need a new constant for “looks better on Phaser Lab”, you are in the wrong file.
- `src/core/DspEngine.*` / `CpuProtect.h` — runtime. Do not hide DSP cost by lying in the footer.

## Version

`PLUGIN_VERSION`, CMake `NEUROKORE_VERSION_LABEL`, Inno, pack script: one string. Do not “also bump” during a UI session.
