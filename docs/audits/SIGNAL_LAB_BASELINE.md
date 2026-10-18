# Signal Laboratory baseline (PR A)

**Commit base:** `main` `bb13ffd` (PR #290 Pad Cloud Hall). User start SHA `eb8e61f` (#288) is two merges behind: #289 Source resize, #290 Pad Cloud Hall.

No cosmetic paint in this PR. Numbers below come from Vitest layout contracts on `feat/signal-lab-pr-a`.

## What already works (do not rebuild)

- Headless Circuit: `boardStore` + DOM chips + one `CableCanvas`.
- Shared Monaco document (`PATCH_MODEL_URI`) and caret/scroll session (#288).
- Shared selection (`publishSelection`) (#282).
- Source inspector, seam resize, honest `nodeMeter` / `No waveform tap` (#283–#289).
- Versioned compile tx and CONFLICT on preset+dirty draft (#284, #287).
- LFO closed plate already draws `lfoLamp` shape + Hz.
- Themes stay `themeOf()` / `--nk-*`. Motion Full / Reduced / Off.

## Current Circuit plate

Every DSP chip is the same 256×224 (grid-snapped) plate: hazard title, typecode (`FL-LP-1000`), M/S, empty pad field, 8 px foot (`id  —`). `faceParamKeys` is `[]`; parameters live in Inspect overlay. Family is a 6 px west hatch (`data-kind`), not a job-specific glyph.

| Family | Closed glyph now | Target |
|---|---|---|
| filter / eq | empty-pad | response curve |
| reverb | empty-pad | decay envelope |
| delay | empty-pad | echo taps |
| lfo | lfo-shape | waveform (keep `lfoLamp`) |
| env | lamp | envelope |
| octaver / pitch | empty-pad | interval mix |
| dynamics | empty-pad | static curve |
| drive | empty-pad | transfer (no UI `eval`) |
| routing | empty-pad | split/join map |

Type: title inherits 13 px (no own `font-size`); typecode 11 px; foot 8 px at `rgba(255,255,255,0.4)` (not a theme token). Port caps 10 px. Semantic zoom does not exist: camera 0.4–1.0 scales the same plate.

## Layout causes (not a new router)

DSP plate 256 px + compact gap 128 px. Compact wraps to the Circuit pane (`896×520` in the contract). `compactRowTarget` is dead code; wrap is `wrapFits(viewW)`.

Measured **Drone Low Bed** compact `896×520`: `IN@32,96 octaver1@416,32 filter1@32,352 reverb1@416,352 OUT@800,352`. No overlap. Fill 0.412. Max cable detour 1.20 (Manhattan). IN shares its x with `filter1` on the wrapped row; `octaver1` sits above and to the right of IN. The compiled graph is serial; the packer draws two rows. Path length is not the main defect — rank/row placement is.

Measured **Pad Cloud Hall** compact: `IN@32,96 reverb1@416,32 delay1@32,352 osc1@416,416 OUT@800,416`. Fill 0.368. Max detour 1.667. LFO is off IN’s cell.

Measured densest factory **Dynamic Space Former** arrange `1440×640`: 27 edges, fill 0.325, hull width past 4500 px.

**Existing factory mismatch:** Drone Low Bed still ships `decay = 2.6` while `chipSpec("reverb").ranges.decay` is 0–1 (Pad Cloud Hall was corrected to 0.88). Do not silently rewrite that preset in a visual PR.

## CSS debt

`web/src/theme/tailwind.css` still contains many `.react-flow*` rules from the old node editor. Chip fill is `--nk-black`, same as the workspace, so idle modules have almost no depth. Terminal view frame uses `rgba(200,200,200,0.28)`.

## Terminal

`HackView` is a tool bar + one Monaco. Short scripts leave a black well. No structure navigator. Draft banner: `UNAPPLIED` / `INVALID` / `CONFLICT`. `VALIDATING` / `APPLYING` are silent. Clean is unlabeled (acceptable as quiet ACTIVE).

`patchStructure(ast)` now derives Input / audio / parallel / mod / Output from the same hydrated board. UI for that list is PR E.

## Performance

PR A does not change paint. Do not invent FPS. Fill `docs/performance/BASELINE.md` when PR B/C add faces. Hidden Circuit must keep `shouldSkipClockPaint`; do not add per-chip rAF.

## Target contracts (`it.fails` until the matching PR)

- Distinct closed-face glyphs per family (PR B/C).
- Chip title/foot at `SIGNAL_LAB` type scale (PR B).
- Drop leftover React Flow CSS; theme-backed foot colour (PR B).
- IN unique leftmost and one serial row for Drone Low Bed compact on the Circuit pane (PR D). Cable Manhattan detour on that preset is already 1.20.
- Terminal `VALIDATING` / `APPLYING` labels and structure navigator (PR E).

## Screenshots

Playwright `web/e2e/signal-lab-baseline.spec.ts` (chromium-1x, 1280×860) waits for `[aria-label=Layout]` to leave the board, then writes:

- `docs/audits/signal-lab-baseline/circuit-drone-low-bed.png` — serial wrap, identical empty pads, IN not unique leftmost
- `docs/audits/signal-lab-baseline/circuit-drone-low-bed-filter.png` — selected filter, Source toggle, incident cables
- `docs/audits/signal-lab-baseline/terminal-drone-low-bed.png` — short script, `decay = 2.6`, empty well
- `docs/audits/signal-lab-baseline/circuit-pad-cloud-hall.png` — LFO off IN, reverb/delay empty pads
- `docs/audits/signal-lab-baseline/terminal-pad-cloud-hall.png` — short script, `decay = 0.88`, empty well

Capture hydrates while Circuit is mounted. Hydrating on Terminal can leave `layoutBusy` until Circuit remounts Compact; that overlay is existing board behaviour, not a restyle in this PR.
