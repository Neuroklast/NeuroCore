# UI / UX contracts

Source of truth for Circuit is the **script**. The board is a headless graph (`boardStore`: nodes, ports, edges) plus a dumb renderer (DOM chips, one canvas for cables). Ports store local centres only. `globalPort = node.xy + local`. Never `getBoundingClientRect` for routing.

## Headless board

| Need | Model |
|---|---|
| Chip drag | Pointer coords in a ref; store + DSL on `pointerup`, snap 32 |
| Camera | One `{tx,ty,scale}` matrix on the world layer and the canvas |
| Ports | Flush contact on the frame; 24 px hit; 30 px magnet. Equal pitch, inside AABB |
| Drag cable | Cubic bezier, east/west normals. No A* while the pointer is down |
| After drop | ELK places chips. A* Manhattan, 45° chamfer. Canvas paints `edge.route` |
| Compact | Rank columns, rail rows, wrap to `viewW`, no snake. Only init / Arrange / Compact — never after a user drag |
| Knob bind | Chrome knobs, not graph cables |

## Circuit — one board

```
IN ── main chain ──┬── parallel bus / split rows ──┐
                   └── …                           ├── OUT
LFO / env live on a row *below* IN, never on IN’s cell.
```

| Rule | Fail if |
|---|---|
| IN is the unique top-left terminal | Any chip `x <= inX` and `y <= inY` |
| OUT is the unique bottom-right terminal | Any chip `x >= outX` and `y >= outY`, or OUT sits in the middle of the chain |
| Flow is left → right, then down | A main-chain cable wraps around the whole board to re-enter from the left |
| No knob cables | Any `kind == knob` jack or `paintKnobCables` drawing |
| Knob use is a label | Bound `a`–`f` not printed on the chip that uses them |
| Expand works | Chevron hit ≠ painted chevron; height does not grow with extra outs |
| One visual out per destination | Two filters read `osc1` but share one east jack |
| Manhattan, few corners | Path cost = `cells + 12 * turns`. Long straight beats a 4-corner shortcut |
| 16 px grid | Jack or chip origin not a multiple of 16 |
| Chips do not overlap | `nodeRectsClash` after tidy |
| ≥ 2 grids air beside, 1 grid above/below | Two chips closer than `CHIP_AIR_X` (64) when stacked in a row, or `CHIP_AIR_Y` (32) when stacked in a column |
| Dest stub is 32 px east, never a U-turn | Any audio path point with `x > dest jack x`, or a west-then-east jog at the IN jack |
| IN/OUT body ≥ 96 so the jack is centered | IO chip shorter than 96, or a **single** side jack (OUT in, DSP in/out) not on that chip’s vertical midline. IN `out`/`sc` sit in the **body band** (below `TITLE_H`, above the foot), one-cell pitch around that midline — never in the hazard title or on the footer |
| Chip paint ⊂ AABB | Handle centres, titles, captions, lamps, or a face widget sit outside `chipBox`. Closed IN/OUT have no range/select. Text ellipsizes in the label column. Closed DSP: no TRG/barcode/grip/fake jacks |
| IN sources east only | Any target handle on IN (including React Flow’s default left handle). `sc` is an east **output** (host sidechain). OUT targets west only; gain is overlay-only |
| Camera fits the chain | After preset/Arrange the pane still shows the previous graph, or `<ReactFlow minZoom>` ≠ `fitView` minZoom |
| Overlay vs south rail | Open detail still paints south bind captions under the overlay |
| Stereo bus is L cyan + R accent, packets `[2, 5]`, ±4 px | One A* then `parallelOffset`. Split mid/side is not L/R. Stored route has no curves. Dashoffset source → dest |
| Port labels are `[ MID ]` / `[ SIDE ]` | Caption is `mid` or a fake UUID / `REV.45-B` |
| Chip foot is `node.id` + live dB | Invented UGID, barcode, TRG, or grip greeble |
| Cable beads follow that chip | **RMS** of the source lane sets packet **gap** (still `[2,10]` → hot `[2,2]`) and **dashoffset** speed. **Peak** sets alpha (0.4→1) and blur (2→15). Peak **> 1.0** is the 3-pass chroma glitch. ≤ −60 dBFS RMS = still. Do not drive glow from RMS or density from peak |
| Clip lamp and hazard are uncovered chrome | Peak LED sits **left of the title** with `CHIP_PAD_X` gap, not under the chevron. Hazard stripes clip on a `::before` layer; the band itself is `overflow: visible`. Clip warn is an outlined Δ + bang at the east jack (`left: 100%`), size `TITLE_H` |
| Param cables never cross knobs | Bind preview has a horizontal run inside the knob-card band |
| Param cables follow the board router | Bind path through a chip, no 32 px stubs, lightning, or dest jack above the knob with an empty path |

**Do not** “fix” a bad tidy with a special case for Phaser Lab. Fix `tidyLayout` ranks/rails so every factory preset obeys the table.

## Chrome

- Footer slots are fixed: MODE / CPU / LAT / SR / BUF / BPM / HOST\|USER / OS. Hover does not rewrite the bar.
- CPU text is `Config::cpuDisplayPercent` (0–100). SAFE is a mode word, not a fake 173 %.
- Hit targets ≥ 26 px. Settings License / Help must not collapse to a 2 px bar (`231939`).
- Overlays size to their content (`preferredHeight`). If the last row is clipped, the overlay is too short — do not hide the controls.
- One overlay at a time. Double-click a node → inspect **all** editable keys.
- Knob right-click (Unit and Circuit) opens `OsContextMenu`: Name / Min / Max / Unit / Note / MIDI Learn. Never the browser menu.

## Motion

- LFO traces: still ink, **pulses** travel at Hz, brightness = amplitude. No diamond, no oscilloscope on the wire.
- Drag-cable is a free line. PCB routing runs **after** drop only.

## Settings / scale

- Theme engine: `signal` (red/cyan/black/white, default), `gold` (yellow/cyan/black), `azure` (blue/cyan/black). Paint is `--nk-*` / `themeOf(id)` — no private hex in UI code. Settings → THEME.
- Window fit is drag-resize. No Settings zoom or formula-pt control. `getDesktopScaleFactor() == 1`. Fit snaps **down** so `fit * 16` is an integer pixel and the canvas never exceeds the host window.
