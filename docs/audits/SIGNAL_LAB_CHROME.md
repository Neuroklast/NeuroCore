# Signal Laboratory shared chrome (PR B)

**Commit base:** `main` `7026f14` (PR #291 signal-lab A). Version stays **0.6.10-beta**.

Shared chip frame (`ChipShell`) plus a `NodeFace` registry. Filter and reverb are the reference families. Functional curves stay PR C. Layout wrap stays PR D. Terminal navigator stays PR E.

## Model

- `BoardChip` composes `ChipShell` + `NodeFace`. Faces register in `FACES`; `BoardChip` does not switch on effect names.
- Closed AABB and jack locals are independent of select, hover, and camera scale.
- Depth is paint only: idle `data-depth=1` (`--nk-surface`), selected or sharp `2` (`--nk-surface-high`), bind-open `3` (`--nk-well-high`). Hover writes `data-focus` on the DOM (existing DoF); fill follows that attribute.
- Semantic zoom writes `data-zoom` on `.nk-board` from `applyLiveCamera`. Chips do not subscribe to the camera.
- Filter face: `data-glyph=response`, `data-axis=log-freq`, label **calculated filter response**.
- Reverb face: `data-glyph=decay`, `data-axis=time`, label **calculated decay envelope**.
- Remaining families keep the empty pad in this PR. PR C–C4 fill every `TARGET_CLOSED_FACE` glyph (`closedFaceGlyph` now matches).
- Leftover `.react-flow` CSS is gone. Chip foot and Terminal view frame use theme tokens.

## Type scale (`SIGNAL_LAB`)

| Slot | Size |
|---|---|
| title | 13 px |
| typecode | 11 px |
| foot | 10 px (`--nk-ink-muted`) |

Overview (`scale < 0.55`) hides typecode, foot, and bind captions. The selected chip keeps typecode and foot.

## Contracts

Vitest: `chipChrome.test.ts`, `NodeFace.test.ts`, `faceKind.test.ts`, `semanticZoom.test.ts`, `signalLab.test.ts`, `circuitCssInventory.test.ts`.

Playwright `web/e2e/signal-lab-chrome.spec.ts` (chromium-1x, 1280×860):

- Filter AABB and jack box unchanged on select; `data-depth` 1 → 2.
- Filter ≠ reverb glyphs / axes.
- Title 13 px, foot 10 px.
- `data-zoom` on `.nk-board`; overview hides unselected typecode.

## Screenshots

- `docs/audits/signal-lab-chrome/circuit-drone-low-bed.png`
- `docs/audits/signal-lab-chrome/circuit-drone-low-bed-filter.png`
- `docs/audits/signal-lab-chrome/circuit-pad-cloud-hall.png`

## Out of scope

Functional FR/IR curves (PR C). Serial wrap on 256×224 plates (PR D). Terminal structure list (PR E). Pre-existing `tsc` in `SourceInspector` / `addBlock` / `boardCommit` / `dslSourceMap`. Drone Low Bed factory `decay = 2.6`.
