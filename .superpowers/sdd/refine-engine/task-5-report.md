# Task 5 report — Send, Multiband Split, Width, Custom-Rename, Octaver, Sidechain-IN

**Status:** DONE  
**Commits:** `d7de469` (SC flag, ChipNode bottom ctrl/rename, wiring) + `aaf000f` (Send/msplit/rename/picker shared with Task 4)  
**Branch:** refine-engine  
**Version:** 0.4.8-alpha (unchanged)

## Contract

Send draws `in`/`out` plus a bottom `ctrl` jack and keeps `kanal` enum both/left/right/mid/side/env. Xover picker label is **Multiband Split** with jacks low/mid/high. Width params are width/delay/bass. Custom keeps `+ input` and `scriptAfterRename` rewrites the chip id in the script. Octaver has five south param jacks. Sidechain IN (io chip like IN) is visible only when `host.sidechainOn`.

## TDD

### RED (contracts first)

```
npx vitest run src/assemble/visualEdges.test.ts src/assemble/addBlock.test.ts src/assemble/flowFromAst.test.ts src/store/hostStore.test.ts src/assemble/grid.test.ts src/overlays/addPicker.test.ts
```

7 failed: missing send/msplit jacks, Xover label, scriptAfterRename, sidechainOn, ctrl face, Sidechain IN.

### GREEN

```
npx vitest run src/assemble/visualEdges.test.ts src/assemble/addBlock.test.ts src/assemble/flowFromAst.test.ts src/assemble/chipSpec.test.ts src/assemble/grid.test.ts src/overlays/addPicker.test.ts src/store/hostStore.test.ts src/presets/parseDslSketch.test.ts src/assemble/detailSchema.test.ts

Test Files  9 passed (9)
     Tests  54 passed (54)
```

Task 3 split/join contracts in `visualEdges.test.ts` / `flowFromAst.test.ts` stay green.

## Files

| File | Role |
|---|---|
| `web/src/assemble/visualEdges.ts` | Send + Multiband Split (`msplit`/`xover`) jacks; ctrl kind |
| `web/src/assemble/validateLink.ts` | `cableFace("ctrl") === "bottom"` |
| `web/src/assemble/chipSpec.ts` | Send `audioOuts: ["out"]` (ctrl is south visual) |
| `web/src/assemble/ChipNode.tsx` | Bottom/top face plugs; custom title rename |
| `web/src/assemble/flowFromAst.ts` | Inject/hide Sidechain IN from `sidechainOn` |
| `web/src/assemble/AssembleView.tsx` | Pass `host.sidechainOn` into `flowFromAst` |
| `web/src/assemble/addBlock.ts` | Multiband Split / Send / Width / Octaver picker; `scriptAfterRename` |
| `web/src/store/hostStore.ts` | `sidechainOn` from host snapshot |
| `src/bridge/HostSnapshot.cpp` | `hostVar.sidechainOn` from SC bus enabled |
| `tests/HostSnapshotTest.h` | Contract for `sidechainOn` |
| `web/src/theme/tokens.ts` | Labels for MB Split / SC IN / Width |
| `docs/DEVELOPMENT_STATUS.md` | WP5 note |

## Model

- Send: side audio `in`→`out`; `ctrl` is a bottom-face jack (`kind: "ctrl"` → yellow/param net).
- Multiband Split always exposes low/mid/high (picker still emits `xover`).
- Sidechain IN is a locked io node when the host SC bus is enabled; absent otherwise.
- Custom rename: click title → `scriptAfterRename` rewrites definition + whole-word tokens.

## Overlap

Concurrent Task 4 WIP already had Bus/Join Signal in `addBlock` / `visualEdges` / `parseDslSketch`. Those entries were left intact; Task 5 only added Send/Octaver/Multiband Split/Width/rename. Task 4 C++ (`BusGraph`, `GraphModel`, …) was **not** committed here.

## Not done (out of scope)

Task 4 emit roundtrip C++. Version bump. Full NeuroKoreTests binary rebuild (HostSnapshot change is small; web contracts prove the UI model).
