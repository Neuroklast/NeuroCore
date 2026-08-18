# UI / UX contracts

Source of truth for Circuit is the **script**, then `tidyLayout` + `PcbRouter`. The canvas only maps. Screenshots `screenshots/Screenshot 2026-08-16 231501.png` (Phaser Lab) and `231939.png` (Settings) are the current visual debt.

## React Flow first

The board is `@xyflow/react`. Check the library before writing geometry. **Dragging chips and connecting jacks are RF jobs.** Read `nodesDraggable`, `onConnect`, `Handle`, `connectionLineType`, `getStraightPath`, `ConnectionMode` before you add a pointer handler or an SVG overlay.

| Need | RF already has | Do not invent |
|---|---|---|
| Chip drag | `nodesDraggable` + `onNodeDrag` / `onNodeDragStop` | A second pointer-drag model, own xy while RF is dragging |
| Drag snap | `snapToGrid` + `snapGrid={[BOARD_GRID, BOARD_GRID]}` | A second snap model for live drag |
| Audio connect | `onConnect` + `isValidConnection` + `Handle` + `ConnectionMode.Loose` | `elementsFromPoint` connect, homemade jack hit-test instead of Handle |
| Drag cable (preview) | `connectionLineComponent` + `getStraightPath` — product: **free line** until drop | `audioStepPath` or A* while the pointer is down |
| Knob → param bind preview | RF `getSmoothStepPath` via `bindSmoothPath` (knobs are chrome, not RF nodes) | A second A* / PCB router for bind |
| Dot / line grid, pan/zoom | `<Background variant={Dots\|Lines\|Cross} gap={BOARD_GRID} />` — stack a second `Background` for the 4×4 block | SVG `pattern` + `useViewport` |
| Handle side | `Handle` + `Position.Left/Right/Top/Bottom` | Fake extra plugs, CSS that fights RF handle coords |
| Layout + PCB route (after drop) | `elkjs` (layered RIGHT) + A* in `layout.worker.ts`; RF paints `data.route` | A third packer, or `routeBoard` on every pointer move |

We write our own path only **after drop**, for constraints RF cannot express: N-cell stubs, chip AABB as no-go, parallel `BOARD_RAIL`, same air to chips. ELK places chips. A* writes the stored SVG `d`. `StaticGridEdge` only paints that string. Live drag and live connect stay RF.

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
| ≥ 1 grid air on every side | Two chips closer than `BOARD_GRID` (32) on the overlapping axis, even with no cable between them |
| Param cables never cross knobs | Bind preview has a horizontal run inside the knob-card band |

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

- Window scale 100 / 125 / 150. `getDesktopScaleFactor() == 1`. Fit snaps **down** so `fit * 16` is an integer pixel and the canvas never exceeds the host window.
