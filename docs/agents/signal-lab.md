# Industrial Signal Laboratory

Binding visual contracts for Circuit and Terminal. Product version stays **0.6.10-beta**. Circuit stays a headless graph (`boardStore` + DOM chips + one cable canvas). One Monaco document (`PATCH_MODEL_URI`).

Read this after `docs/agents/ui-ux.md` when changing chip faces, Terminal panes, or board tokens.

## Depth

| Level | Use |
|---|---|
| 0 workspace | Near-black board / Terminal well. Grid may exist; it must not compete. |
| 1 idle module | Matte lift from the board. Outline. No standing glow. |
| 2 focused / selected | Higher surface, stronger type, incident cables and neighbours. |
| 3 interaction | Drag, bind, connect. Gone when the pointer is up. |

Tokens: `SIGNAL_LAB` in `web/src/theme/signalLab.ts`. Colours stay `themeOf()` / `--nk-*`.

## NodeFace

`faceKindForType` in `web/src/assemble/nodeFace/faceKind.ts` maps `ChipSpec` → family. Renderers live under `nodeFace/`, not as a switch in `BoardChip.tsx`.

| Family | Closed glyph (target) | Data |
|---|---|---|
| filter (filter, eq) | frequency response | ChipSpec type/cutoff/Q. Label as a **calculated** curve until a native tap exists. |
| reverb | decay envelope | size, decay, damp, mix. Not a measured IR. |
| delay | echo taps | time, sync, feedback. Host-sync when `sync` is on. |
| lfo | waveform | existing `lfoLamp` / native shape + Hz. |
| env | envelope | attack, hold, release. Parameter curve vs live activity are separate. |
| pitch (octaver, pitch, vocoder) | interval mix | sub / up / mix. No fake spectrum. |
| dynamics | transfer curve | threshold, ratio, knee, ceiling from ChipSpec. GR meter only with a real tap. |
| drive | transfer curve | no `eval` of DSP expressions in the UI. |
| routing | split / join map | ports and destinations. No decorative waveform. |
| modfx | compact waveform | phaser / flanger / chorus. |
| cab | mix | IR slot name when present. |
| io | jacks | IN / OUT / SC. |
| utility | params | readable keys, no invented meters. |

Closed geometry does not change with meter ticks. Semantic zoom changes paint, not jack locals.

## Semantic zoom

Camera range stays `BOARD_MIN_SCALE` 0.4 … `BOARD_MAX_SCALE` 1.

| Level | Scale | Paint |
|---|---|---|
| overview | `< 0.55` | type, id, ports, reduced glyph. No unreadably small param text. |
| normal | `0.55 … 0.85` | glyph + primary params. |
| focused | `≥ 0.85` or selected | extra readouts in the chip or Source inspector. Chip AABB unchanged. |

## Layout

Signal order is the compiled graph, not source-line order.

- Serial audio: `IN → processors → OUT` with few wraps.
- Manual `xy` is user-owned. Selection, hover, Source resize, window resize, and tab change do not Arrange.
- Compact / Arrange / new preset may pack.

A short serial chain that wraps because DSP plates are 256×224 with 128 px air is a layout bug to fix in the packer or the plate size, not with a per-preset special case.

## Terminal panes

Monaco remains the source of truth. Optional structure list comes from `patchStructure(ast)` (same hydrate as Circuit). Context / diagnostics fill leftover space and collapse on a narrow pane. No second graph store. Macro knobs stay on the south rail.

Draft labels: quiet when clean; `UNAPPLIED`, `VALIDATING`, `APPLYING`, `INVALID`, `CONFLICT` when those states are live. Apply only after native confirm.

## Honest data

Real node ids, parameters, and taps only. DSP chips with no waveform tap stay **No waveform tap**. No TRG codes, barcodes, or invented meters.

## PR sequence

A baseline / contracts (this file) → **B shared module chrome** (`ChipShell` + `NodeFace` registry; filter/reverb reference wells; `data-zoom` on `.nk-board`) → C functional faces → D layout / cables → E Terminal workspace → F Circuit×Terminal UX → G motion → H layout/perf hardening → I native DAW.
