# Entwicklungsstand NEUROKORE

**Letzte Aktualisierung:** 2026-08-16  
**Version:** 0.4.7-alpha  
**Gesamtfortschritt:** ~91%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ✅ CpuProtect reset nach prepare/OS/IR; Session-State; ChangeBroadcaster | 91% | 2026-08-15 |
| Core/DspEngine | ✅ Idle skip OS/DSL nach Stille+Tail+OS-Flush | 99% | 2026-08-16 |
| Core/ScriptManager | ✅ Neu: Skript-Verwaltung, Variable Names, Preview, testFormulaStability | 85% | 2026-05-21 |
| Core/WaveformCapture | ✅ Neu: Lock-free Ring-Buffer für Input/Output-Waveform | 90% | 2026-05-21 |
| Core/MidiVariableMapper | ✅ Neu: midi_note/vel/gate/bend/mod/freq als DSL-Variablen (atomic) | 95% | 2026-05-21 |
| Core/PluginEditor | ✅ Fit-Snap auf 16-Grid; Host-Desktop-Scale = 1 | 99% | 2026-08-16 |
| UI/UiSettings | ✅ Persist motion, liveMode, scale, font, Host/User tempo | 97% | 2026-08-16 |
| Core/Config.h | ✅ CPU-Guard: Zeit-Hold / Probe-Fenster / keine Dry-EMA-0 | 99% | 2026-08-16 |
| DSL/DSLParser | ✅ + delay / reverb / ms + bus/send/out + eq + octaver + vocoder + gate / ngate | 99% | 2026-08-16 |
| DSL/GraphModel | ✅ Park-Rail `__park`; OUT bleibt rechts (kein Links-Klapp) | 99% | 2026-08-16 |
| DSL/PcbRouter | ✅ Pattern-Manhattan (HVH / Wrap-U), kein A* | 100% | 2026-08-16 |
| UI/GraphCanvas | ✅ Chips 176×64; Host-Scale-Fit rastet Rasterzellen ganzzahlig | 99% | 2026-08-16 |
| DSL/SignalChain | ✅ Delay control-rate; Gate/noisegate; per-Block Taps | 99% | 2026-08-16 |
| DSL/ExpressionEvaluator | ✅ Solide (SIMD, CSE, Const-Folding) + SIMD-Funktionspfade + Template-Block-APIs | 90% | 2026-05-21 |
| DSP/InputGain | ✅ Funktional | 80% | 2026-04-01 |
| DSP/WaveShaper | ✅ Funktional | 75% | 2026-04-01 |
| DSP/SignalPolisher | ✅ Funktional + DC-Blocker nach DSL integriert | 82% | 2026-05-19 |
| DSP/DSPUtils | ✅ autoGainCompensate optimiert (direkte Sample-Multiplikation) | 85% | 2026-05-19 |
| UI/DslTerminalEditor | ✅ Edit-Modus; IR-Button in Zeilenhöhe pro `irN` | 92% | 2026-08-16 |
| UI/FormulaDisplayComponent | ✅ Live-Eval; Script tab shows `[value]` again | 96% | 2026-08-16 |
| UI/WaveformDisplay | ✅ ScopeDeck: Wave + Field + Loudness default offen, 176 px | 94% | 2026-08-15 |
| UI/LoudnessMeter | ✅ Glitch cubisch mit Pegel; plus kompakte IN/OUT-Balken | 99% | 2026-08-14 |
| UI/ParameterComponent | ✅ Wert unter dem Zeiger; Drag hebt Knob-Kabel | 94% | 2026-08-16 |
| UI/MidiLearnManager | ✅ Neu erstellt, vollständig | 90% | 2026-04-01 |
| Preset-System | ✅ 526 Factory; Topology-Hash; Script-Match setzt den Namen | 99% | 2026-08-16 |
| Localiser | ✅ DE/EN; Gain/Mix Labels; CurrentPreset | 82% | 2026-08-11 |
| Licensing | ✅ Offline RSA-.lic; nach Aktivierung zeigt License den Inhaber | 93% | 2026-08-14 |
| Tests | ✅ Factory quality + loudness gate; jack-grid + LFO-LED | 99% | 2026-08-16 |
| CI/CD | ✅ Windows Tests+pluginval; macOS-Job baut AU + auval | 95% | 2026-08-14 |
| Build (Windows) | ✅ Standalone + VST3 Release unter VS2022 | 100% | 2026-06-29 |
| Dokumentation | ✅ Circuit/Terminal; noisegate; Host/User BPM; Tools-Zeile | 99% | 2026-08-16 |
| Installer | ✅ Kit `NEUROKORE-0.9.0/` im Repo-Root (VST3, Standalone, EULA, Docs) | 90% | 2026-08-15 |
| AU-Format | ✅ AUv2 `aumf`; CI-Job `AU (macOS)` liefert `.component` | 100% | 2026-08-14 |

---

## Aktive Checkliste

### 2026-08-16 – Circuit-Nodes = Code + Phaser-Lab CPU

- [x] Keine Knob-Kabel; Letter + Name stehen im Chip
- [x] Expand-Hit = Chevron; Höhe wächst mit Ausgängen / Args
- [x] IN oben links, OUT unten rechts; Mod-Zeile unten
- [x] LFO: ein Ausgangs-Jack pro Ziel; Impulse statt Neon-Lampe
- [x] Router wählt den Pfad mit `Länge + 12×Knicke`
- [x] Overlay/Keys: widen.delay, ott.f1/f2, volle Liste
- [x] CPU-Anzeige 0–100; SAFE hält nicht 173 % fest
- [x] LFO-Filter-Coeffs control-rate (Stride 8), nicht per Sample
- [x] Tests **28672 / 0**

### 2026-08-16 – Idle-CPU, Xover-Clicks, Host-Scale Nodes

- [x] Stille + Tail + OS-Flush → nasser OS/DSL-Pfad schläft (`DspEngine::isIdle`)
- [x] Signal weckt denselben Block (Peak-Gate, Mix/Switch-Ramp halten warm)
- [x] Xover/OTT: 4-Hz-Coeff-Gate raus; Update jedes Blocks aus 60-ms-Smoother
- [x] `getDesktopScaleFactor() = 1` — Host 125/150 % nicht doppelt auf Affine-Fit
- [x] `snapUiFitToGrid`: `fit * 16` ist ganzzahlig (Nodes/Jacks/Raster deckungsgleich)
- [x] Tests: Idle sleep/wake, Xover-Zipper, Fit-Snap — Suite **28640 / 0**

### 2026-08-16 – SAFE blinkte im 2-s-Takt ohne Input

- [x] Dry lässt die Last-EMA stehen (kein Fake-0 %)
- [x] Nach Retry: 250 ms nass messen, dann erst Recover/Stay
- [x] Soft-Trip erst nach 350 ms über Budget, nicht nach 8 Blöcken
- [x] Test: 1.8×-Probe nach Trip bleibt getrippt

### 2026-08-16 – Manhattan, no A* (Screenshot 220910)

- [x] Router is HVH (east stub / one vertical / west stub). Dest-west = U through the row gutter
- [x] A* / occupancy heap / reserved-edge search deleted
- [x] Chip 176×64 (title 2 rows + jack + bottom). No more 144×32 strips
- [x] Knob cables are the same Manhattan, not cubics

### 2026-08-16 – PCB-Board-Modell (Screenshot 204746)

- [x] Ports: Pin auf der Rasterkante, Escape = 1 Zelle in Facing-Richtung
- [x] Occupancy-A* auf dem Vertex-Gitter; Chips geschlossen blockiert
- [x] Board-AABB inkl. Halo; kein Waypoint off-board
- [x] `dropMicroJogs` / `forceJackStubs` / `wrapGutterY` / 2-px-Jack-Inset entfernt
- [x] `tidyLayout` klappt OUT nicht mehr nach links; schmales View wrappen oder Paper wächst
- [x] Routing in Entwurfs-px (`cell = 16`); Zoom skaliert nur den Path
- [x] Knob-Kabel = Control-Nets von der linken Board-Kante (rot, um Chips herum)
- [x] Tests: PcbRouter Port-Verträge + GraphModel narrow-OUT; Suite 26050 / 0

### 2026-08-16 – Tester-PDF umgesetzt (nicht neu geschrieben)

- [x] Footer = feste Slots (MODE/CPU/LAT/SR/BUF/BPM/HOST|USER/OS). Hover schreibt die Leiste nicht um
- [x] Empty-space add landet auf `__park` (kein Audio-Kabel). Drop auf eine Spur spliced wie bisher
- [x] Letztes Chip von OUT abziehen parkt es; anderes Chip per Kabel an OUT
- [x] Functions-Plots: OTT / Widen / Octaver / Vocoder haben eigene Glyphs
- [x] Stages: Up/Down, Pfeiltasten, Drag-Reorder
- [x] 125% Lesbarkeit: Footer 13 pt, Lockup 14/12 pt, SNAP-Hint 16 pt, kein Squash
- [x] Version **0.4.7-alpha**

### 2026-08-16 – Grid routing rules (Screenshot 204457 + spec)

- [x] 16-grid; ports and interior segments snap
- [x] Chips are blocked cells; wrap runs in the 32 px row gutter
- [x] East stub before first turn; west stub into the jack
- [x] Axis-only + turn penalty; parallel tracks; 90° crossings only
- [x] IN leftmost / OUT rightmost; fan-in merges one cell before the plug
- [x] Narrow chips (144 px); circle = audio, square = mix/bus
- [x] Mix/bus stroke thicker; fold rebuilds the grid
- [x] Version **0.4.5-alpha**

### 2026-08-16 – Feedback sheet P0–P4 + graph overview

- [x] Save-validation uses SafePointer (British Plexi Bark crash)
- [x] Position-only emit does not rebuild the audio chain
- [x] Xover coeff updates are less twitchy
- [x] One overlay at a time + toggle; `?` in the top bar
- [x] Footer brighter / less squash; Mix % has a min width
- [x] Settings motion applies while the overlay is open
- [x] Right-click = node menu (duplicate/delete); double-click = inspect
- [x] Add-on-cable splices; chips never overlap (16 px gap)
- [x] Knob cables are direct curves, not square PCB traces
- [x] Host context menu on knobs; MIN/MAX labels
- [x] IR Play preview; Functions Copy + Nodes lookup
- [x] Plexi copy no longer leaks the “no Marshall name” prompt
- [x] Idle backdrop skips when Motion=Off or the meter is silent
- [x] Version **0.4.4-alpha**

### 2026-08-16 – PCB lanes keep a full-cell offset

- [x] Shared tracks: lane 0 on jack axis, others ±N cells (not ±3 px)
- [x] `dropMicroJogs` no longer eats intentional lane risers after `routeAll`
- [x] Same-Y bus restores stubs after `collapseColinear`
- [x] Version **0.4.3-alpha**

### 2026-08-16 – PCB rows + no micro-jogs + cyber LFO

- [x] Chip height = (1 title + N jack) units; jack i in the centre of row 1+i
- [x] Manhattan + mandatory horizontal stubs
- [x] Parallel lanes: equal gap, stubs stay on the jack axis
- [x] LFO light: neon bolt + cyan diamond + red trail (not a generic blob)
- [x] Version **0.4.2-alpha**

### 2026-08-16 – Orthogonal jacks + LED + grid sizes

- [x] Knob/LFO/audio traces enter jacks on the axis (no cubic diagonals)
- [x] One LFO LED, constant px/s, pulse = Hz
- [x] Empty jack = socket; patched jack = plug in the socket
- [x] Chip W/H/IO/jack pad/pitch are 16-grid; jack Y = pad + slot×pitch
- [x] tidyLayout col/row pitch snapped to the same grid

### 2026-08-16 – Factory leftovers + IR mix align

- [x] Mixbus Soft Clip: recovery LPF after hardclip
- [x] Quiet inserts get makeup (Acoustic Sim, Subs, Leslie, Green Boost, SC Pad, Horror, Sub Drop)
- [x] Dry/wet delay = OS + IR latency (`setDryAlignLatency`)

### 2026-08-16 – Live path never splices dry on lock/CPU

- [x] Lock-miss / CPU-hold replay last wet (decay), fade back in
- [x] `evaluateFormula` no longer takes `processLock`
- [x] Overlay inset includes the tools row; fold hit uses 40 px gutter
- [x] Settings motion callback is wired before `show()`

### 2026-08-16 – Version 0.4.1-alpha on binaries

- [x] `PLUGIN_VERSION` / CMake / Inno / pack script = **0.4.1-alpha**
- [x] Standalone + VST3 (+ AU) `OUTPUT_NAME` = `NEUROKORE-0.4.1-alpha`

### 2026-08-16 – Metal Gate / Acid Line / preset reset

- [x] Metal Gate: widen after IR+limit; Width default 0.55; gate hold/release stable
- [x] IR load/clear under processLock
- [x] Env::clearRuntimeState; Widen AP buffers reset
- [x] Acid Line: Res 1.9 / max 2.6, attack 4 ms

### 2026-08-16 – Drag preview is a free line

- [x] Rubber-band is jack → pointer; PCB A* only after drop

### 2026-08-16 – Circuit node overlay, knobs, presets, Acid Line

- [x] Node inspect lists every block argument (delay sync/damp/pingpong, filter +/*, env source…)
- [x] Empty fields show placeholders; overlay scrolls
- [x] MS DEC jack labels stay in a gutter — title/summary no longer overlap
- [x] Knob Set Min/Max writes `param a = Name [min, max]` (APVTS stays 0–1)
- [x] Preset arrows walk category then name, then the next folder
- [x] Top chip shows the loaded preset name
- [x] Env literals skip per-sample evaluate; env-only filters stride coeffs (Acid Line)

### 2026-08-16 – LFO cables are a running light

- [x] Trace stays static (no perpendicular wave, no size pulse)
- [x] Beads travel along the path; speed = osc Hz (1 Hz = 1 trip/s, clamped 0.05–16)
- [x] Brightness = peak |LFO|; `copyLfoHz` is atomic
- [x] Env cables stay a static trace (no chase)

### 2026-08-16 – PCB-style circuit cables

- [x] `dsl::PcbRouter` is UI-free: A* on a 4-neighbour grid, turn penalty 12, Manhattan heuristic
- [x] Node bounding boxes are obstacles (start/end chips stay passable so the jack can leave)
- [x] Sharp 90° corners become quadratic Beziers with a fixed radius
- [x] Shared tracks get a parallel lane offset (`routeAll`)
- [x] GraphCanvas maps chips → `PcbRect` and cmds → `juce::Path`; drag/zoom/fold rebuild routes

### 2026-08-16 – Periodic crackle (gate/phaser/space/rumble)

- [x] Env peak clamped 0–1 (no `1-env*k` polarity flip)
- [x] Freq `+osc` is unipolar + `max(fmin,…)` (no HP/LP invert)
- [x] Phaser Lab/Sweep: HP stays below LP; no `y_prev` feedback
- [x] Cinematic Space: delay is true predelay (`mix=1`, `feedback=0`)
- [x] Far Plane / Score Hall: same — wet-bus predelay, no 95 ms series slap
- [x] visualRail: named bus wins over channel (Wide Canvas)
- [x] setScript does not inherit positions from a previous preset by name
- [x] Preset overlay closes on the next message turn; inspect closes on script change
- [x] LFO cables: static PCB trace + running light (speed = Hz, brightness = amplitude)
- [x] Kick/Warehouse/Hardcore/Gabber: softclip before hardclip; env-filter Q 0.85
- [x] Octaver: free-running sub, silent until lock (no period-reset click)
- [x] Rumble body LPF stays on Floor ~48–88 Hz (no env-open to 400 Hz)
- [x] Scream is env-gated (floor 0.08), not a square tail
- [x] Widen: no Haas slap; |side| ≤ 0.92 |mid| (no L/R flip)
- [x] OTT minimum attack/release so time≈0 cannot click

### 2026-08-16 – Factory library 500+

- [x] Keep 191; add wave2 jobs (526 total)
- [x] Distinct topologies (hash on blocks/notes/literals), not knob clones
- [x] All 22 categories filled; search/tags remain the index

### 2026-08-16 – Auto-arrange at current zoom

- [x] Context menu label is only **Auto-arrange**
- [x] `tidyLayout` wraps each rail (Trailer Impact smash no longer stays one long row)
- [x] Factory apply leaves `@x,y` out; Circuit arranges at the current viewport/zoom on load
- [x] Pending arrange after first resize if the canvas had no size yet

### 2026-08-16 – Alpha 0.9.1-alpha kit

- [x] Version string **0.9.1-alpha** (CMake 0.9.1, PLUGIN_VERSION, Inno, pack folder)
- [x] Release Standalone + VST3
- [x] Pack `NEUROKORE-0.9.1-alpha/` (VST3, Standalone, Docs, zip)
- [ ] Inno Setup 6 fehlt weiter — kein `Setup.exe`

### 2026-08-16 – Release kit refresh

- [x] Release Standalone + VST3
- [x] Pack `NEUROKORE-0.9.0/` (VST3, Standalone, Docs, zip)

### 2026-08-16 – Tidy, meters, sidechain, full Add menu

- [x] `tidyLayout` setPosition only; factory apply leaves `@x,y` to Circuit Auto-arrange
- [x] Meter pass-through (loudness/peak/RMS); Sidechain block (host extra in)
- [x] Context Add is hierarchical; all DSL blocks listed
- [x] Expand hit matches chevron; OUT edit keys include every mix jack

### 2026-08-16 – Circuit beads are loudness, not always-on

- [x] Dots from `getLoudnessDb()` (`loudnessToCableLevel`), gate 0.022
- [x] Silence hides beads; loudness lights and moves them
- [x] Wave shape may use the scope ring; gate stays loudness

### 2026-08-16 – Space Echo CPU + live cables

- [x] Delay control-rate once per block (no per-sample evaluate lock)
- [x] Heavy tape/delay presets: 8x sine finite, no block click, no CPU trip
- [x] Circuit beads/waves from WaveformCapture (same as IN/OUT scopes)

### 2026-08-16 – Circuit cables back to ink

- [x] Audio traces `ink` / `inkMuted` (not rose)
- [x] Beads only when input/tap energy is above the gate (same as before waveforms)
- [x] Wave mode uses the same gray ink

### 2026-08-16 – Split language + guitar stereo + kit

- [x] `split { mid/side | left/right | crossover | parallel }` expands to existing DSP
- [x] Octaver/vocoder extra param jacks; tooltips on Presets/Functions/Stages/Bypass/OS/Polisher
- [x] Guitar Width on amp presets; silent-side copy; Stereo Guitar Wall via leftright split
- [x] Release Standalone + VST3 + `NEUROKORE-0.9.0/`

### 2026-08-16 – Circuit UX + mono guitar stereo

- [x] Circuit cables: Settings Dots / Wave; white/gray traces; beads only with signal
- [x] Chips fold; jacks stay laid out; octaver keys + knob jacks
- [x] Knob cables behind chips at 50%, 25% across a card
- [x] No rail-drop red line while placing chips
- [x] LFO output + filter-mod smoothing (no zipper/crackle)
- [x] BOTH seeds a silent side from the live side; guitar Width knob
- [x] Brand left-aligned; unified padding; no assemble on resize

### 2026-08-16 – Review, release build, kit

- [x] Overlay hint is a layout Label (no overlap with a-f)
- [x] Host BPM 0 falls back to 120; noisegate defaults without hold
- [x] Manuals: Circuit/Terminal, tools row, footer, inline IR
- [x] Release Standalone + VST3; pack `NEUROKORE-0.9.0/`

### 2026-08-16 – Circuit polish, noisegate, tempo source

- [x] Overlay button gaps; chip labels do not overlap; live knob values in red
- [x] Ctrl+wheel zoom; cables draw the post-block waveform
- [x] Bus header wired IN -> BUS -> send
- [x] `ngate` / noisegate: optional threshold, attack, release
- [x] Tempo: Host or User BPM in Settings; footer shows HOST / USER
- [x] Mix / OS / Polisher sit above the scopes

### 2026-08-16 – Circuit overlay, footer, no accidental reorder

- [x] Drag snaps X/Y only; order/routing stay put
- [x] Node overlay for every block (ASCII, readable fields)
- [x] Circuit / Terminal names; footer SR / 32f / BUF / BPM / CPU / LAT / OS
- [x] Knob hover lights traces; IR button on the formula line
- [x] Mix glitch stays in the slider; loudness grain scales with motion

### 2026-08-16 – Graph board: chips + rose crosses

- [x] Cards snap to a 16 px grid (rounded); load and add write snapped `@x,y`
- [x] Background is sparse rose crosses, not a full line grid
- [x] Blocks read as IC packages (chamfer, inner die, notch, pin-1, DIP pads)

### 2026-08-16 – Graph routing + readable menus

- [x] Popup menu 18 pt / 28 px rows
- [x] Add Bus, Mid-Side Split, L/R Split, Crossover; Widen labeled Stereo Width
- [x] Bus headers visible; MS forks MID/SIDE rails like a bus
- [x] Cable drop lights the target jack; moving a card highlights the rail

### 2026-08-16 – 8× Tape Echo Dirt periodic artifacts

- [x] Studio 8× always runs the host-Nyquist AA LPF (FIR is not enough after tube+delay)
- [x] Delay time/fb/mix/damp follow knob lanes every sample
- [x] Hermite read stays ≥ 8 samples behind the write head
- [x] Tests: 8× wrap silence, Tape Echo Dirt ticks, block-boundary continuity

### 2026-08-16 – Chrome, preset name, readable cards

- [x] L/Both/R same width as knobs, same row as Graph/Script, 3 px gaps
- [x] Toolbar buttons even; brand lockup gets the freed width/height
- [x] Untitled script that matches a factory preset shows that name
- [x] Graph cards: chips on the title row, no A/B/C labels over the formula
- [x] Turning a knob highlights its cables; value sits under the disc

### 2026-08-16 – Script live knob values

- [x] Script tab no longer forces Edit (DslTerminalEditor has no live `[value]`)
- [x] Edit / Save back in the Script action row
- [x] Knob rename in Script live view writes `param a = …` into the processor script
- [x] Tests: annotated `a[5.000]` / `=> 5`; Script workspace stays in live view

### 2026-08-16 – Knob rename + overlay scale + script param names

- [x] Name-Label `setText` nicht während Edit (Timer hat das Feld sofort geschlossen)
- [x] Min/Max-Text unabhängig vom Live-Wert (Integer bleibt Integer)
- [x] Overlays: Design-Pixel, nicht Host-`getWidth`; Optimize füllt wie Help
- [x] Script ↔ Knob: `param a = Name` live, Rename schreibt die Zeile zurück

### 2026-08-16 – Multi-jack nodes

- [x] `GraphJack` + `jacksFor` / `jacksForInput` / `jacksForVirtualOut`
- [x] OUT: ein Mix-Eingang pro Bus (MAIN, DIRT, …)
- [x] Filter/Stage: Audio in/out + Knob-Buchsen A–F + referenzierte LFO-Namen
- [x] Comp/Gate/Vocoder/Env: extra SC; Osc: nur MOD-Out
- [x] Resolve-Drag landet auf der getroffenen Buchse (`connectJack`)
- [x] Undo: jede Formel/Rename-Aktion ist eine eigene Transaction
- [x] Tests: 5468 passed / 0 failed

### 2026-08-16 – Overlay scale + script colour + chrome

- [x] Overlays sit on `scaledRoot` so they follow window scale
- [x] Tighter overlay margin (16)
- [x] Functions / Stages back in the toolbar; License / Help in Settings
- [x] Script tokeniser: comments muted, keywords/knobs accent

### 2026-08-16 – Graph is the editor

- [x] Compact cards (152×56) / IN-OUT terminals (64×32); one caption line
- [x] Double-click edits in the card (formula + a–f). No AlertWindow
- [x] Audio cables dim gray, no packets; knob/LFO lines only on hover
- [x] Action row is Graph | Script | Edit; Script shows live knob values; Edit opens the text editor
- [x] Functions / Stages / Copy / Optimize live in More
- [x] Scope extras start folded

### 2026-08-16 – Deep-research UI leftovers

- [x] Body/chrome/default sans is JetBrains Mono; Apex only via `brandFont`
- [x] Preset/Functions/Help rows: ink + tick, no red-on-red
- [x] Tags / IR captions drop `·` (Apex missing glyph)
- [x] Overlays leave HUD + toolbar + tools (Mix/OS/status) uncovered
- [x] Graph cards show the full `y` formula (3 lines)
- [x] Script marks the parser line; `setError` no longer replaces the live formula

### 2026-08-16 – Settings overlay + LIVE mode

- [x] Knob-Kabel nicht durch ModalOverlays (License/Settings/…)
- [x] Graph Add/Remove entfernt (Rechtsklick bleibt)
- [x] AUDIO-Button aus der Tools-Zeile; Gerät sitzt in Settings
- [x] Settings-Overlay: Animation Full/Reduced/Off, Studio/Live, Scale, Formeltext, Standalone-Audio
- [x] LIVE: min-phase IIR-OS, Studio: linear-phase FIR; Latenz gemeldet
- [x] Audio-Thread: knobLanes in prepare voralloziert
- [x] Tests: Graph ohne Add/Remove, Motion-Persist, Live-Latenz < Studio

### 2026-08-15 – Graph is a circuit, not a list

- [x] Horizontal MAIN LINE + named BUS rails; send tap; out mix traces
- [x] Drag-and-drop reorder / `assignNodeToBus`; ghost + drop highlight
- [x] Knob traces a–f → blocks that reference them (`paintPatchCables`)
- [x] One knob column (6); IN/OUT wave + stereo field + loudness restored
- [x] Preset load stays in Graph/Script; assemble no longer covers the view
- [x] Tests: `assignNodeToBus` + send stays on a named bus

### 2026-08-16 – Node context menu + Graph/Script translation

- [x] Right-click empty: add block at cursor
- [x] Right-click node: Edit parameters / formula, Knobs a–f per arg, Remove
- [x] Graph → Script emits DSL; Script → Graph parses; layout kept if semantics match
- [x] `setNodeArg` / `editableArgKeys` + tests
- [x] Knob letters on nodes + traces from left knobs; hover highlights
- [x] Graph→Script does not `applyFormula` when semantics match; timer uses loudness not waveform copies

### 2026-08-16 – Preset switch crash + live cables

- [x] Virtual OUT (`kOutIndex`) is a real OUT, never `nodes[(size_t)-2]`
- [x] Rebuild detaches children before delete; no re-entrant rebuild
- [x] Cables pulse with IN/OUT waveform peak
- [x] Test: Shimmer Drive → Wide Motion paintEntireComponent

### 2026-08-15 – Free node patcher

- [x] GraphCanvas: freie Nodes (Children), Ports, Bezier-Kabel, Body-Drag ≠ Port-Drag
- [x] GraphModel: `@x,y`, `audioEdges`, `connectAudio`, `disconnectAudio`, `insertOnEdge`
- [x] Auto-Layout nur wenn alle Positionen fehlen
- [x] Editor-Spaghetti-Kabel entfernt
- [x] Tests: Layout-Roundtrip + connect/insert

### 2026-08-15 – Graph readable: no spaghetti, no Untitled

- [x] LFO/env on MOD rail; OUT at the end of MAIN; bus return hooks to OUT
- [x] Lane titles do not sit on IN/TAP; cards are one title + compact params
- [x] Knob cables clip to knobs ∪ graph inner; gutter routing, lower alpha
- [x] Preset name set before `applyFormula` notify
- [x] Status HUD is LIVE / CPU / latency / OS — rest in tooltip

### 2026-08-15 – GraphModel parse ↔ emit

- [x] `src/dsl/GraphModel.h/.cpp`: parse via DSLParser + `#`-Kommentare, emit, semanticallyEqual, moveNode
- [x] CMake SOURCE_FILES + `tests/GraphModelTest.h`
- [x] Roundtrip: einfache Kette, Bus-Skript, moveNode, Factory Mesa / Side Delay / OTT Smash
- [x] GraphCanvas + Graph/Script im Editor; Formel bleibt editierbar

### 2026-08-15 – CpuProtect: 8× OS must not mute on spikes

- [x] EMA-Last; Soft-Trip nach aufeinanderfolgenden Hits; Hard-Trip nur EMA ≥ 3× × 4
- [x] Zeit-Warmup 3s; Retry 2s; Reset nach prepare / OS / IR
- [x] Tests: Warmup-8×, einzelner Spike, sustained, Recover, Hard-Trip
- [ ] Manuell: 8× in FL — ~32% bleibt nass, kein „wet path paused“-Stotter

### 2026-08-15 – Contrast: chrome red, body ink

- [x] `canvas` / `ink` / `inkMuted` / `comment` / `warningMark` in LookAndFeel
- [x] Formula + IR-Buttons + CPU-Banner: ink on canvas, not red-on-red
- [x] Preset-Header und Labels nicht accent-on-dark-red
- [ ] Manuell: Formel, Help, Preset-Tabelle, CPU-SAFE in FL — Text lesbar

### 2026-08-15 – Persist Calm UI + instant overlays

- [x] `UiSettings` PropertiesFile unter NEUROKLAST/NeuroKore (`calmUi`, `uiScalePercent`)
- [x] `CyberMotion::Off`: kein Boot, kein Glitch, Director-tick No-Op
- [x] ModalOverlay Off/Reduced: `show()` landet sofort in Shown
- [x] Tests: shouldPlayBoot(Off), Director triggerGlitch, Overlay Shown
- [ ] Parent: Calm-Button an `UiSettings` + `cyberDirector.setMotion` verdrahten

### 2026-08-15 – Factory cabinet IRs

- [x] Acht Amp-Presets mit `ir1` mappen auf American / British / Medium / Vintage
- [x] WAV in `resources/irs/`; BinaryData; keine Dritt-Lizenz
- [x] `applyPreset` lädt IR nach der Formel, räumt fremde Slots weg
- [x] Formel bleibt ohne Dateipfad
- [ ] Manuell: Mesa / TS / JCM / AC30 in der DAW — Cab hörbar, Clear macht dry

### 2026-08-15 – Functions folders + docs

- [x] Functions-Sidebar wie Presets: All / Core / Drive / Crush / Blocks
- [x] README, USER_MANUAL, Help (`UserManual_en.txt`)
- [x] Catalog: ott, widen, vocoder, octaver

### 2026-08-15 – Widen + vocoder + OTT

- [x] `widen` Block: Allpass + Haas, Bass bleibt mono, Mid = Original
- [x] Factory **Mono to Stereo**
- [x] Vocoder: breitere Bänder, leere Sidechain fällt auf Self-Vocode, Bus default an
- [x] `ott` Block + Factory **OTT Smash**
- [ ] Manuell: Mono-DI → Stereo; Vocoder mit Voice-Pin; OTT auf einem Bus

### 2026-08-15 – Preset chip prev/next

- [x] `<` / `>` links und rechts vom Preset-Namen
- [x] `stepPreset` wrappt Factory, dann User
- [ ] Manuell: Chip durchklicken, Name und Formel wechseln

### 2026-08-15 – Analog octaver

- [x] Mid-Clock + Flip-Flop −1 (Pitch = Zero-Cross, kein freier Sinus)
- [x] Detector 28–650 Hz, Periode nur 22–700 Hz
- [x] +1 = Gleichrichter; Sub mono; 12 dB Tone
- [x] Precision Octaver / Bass Sub: Track vor Tube
- [ ] Manuell: Bass/Gitarre gegen OC-2 — kein Wobble

### 2026-08-15 – Effect-block artifact harden

- [x] Delay: Hermite-4, min 4 Samples vom Write-Head, kein Crossfeed, Denorm-Flush
- [x] Reverb: Mono-Summe ins Freeverb, 4 Allpässe, Comb-Denorm
- [x] Filter/EQ: Coeff nur bei echter Änderung, kein Dummy-Smoother-Burn
- [x] Comp/Gate/Limit: Denorm, Limit mit 80 µs Attack statt Instant-Slam
- [x] Xover 3-Band: High = HP(f2)² von HP(f1)²
- [x] Octaver/Vocoder: Coeffs hoisten, Pointer-Loop, Denorm
- [x] hardclip: pow() nur ab |x| ≥ 0.8 L
- [ ] Manuell: Delay/Reverb ohne Tick; Gabber/Hardcore ohne Haken

### 2026-08-15 – Hotpath + stereo wall

- [x] Clip-Stages: Functor einmal binden, kein Inject ohne Env/t
- [x] Mod-Filter: setCutoff nur bei merklicher Änderung
- [x] Mono-IR → Stereo; Stage/SVF immer 2 Kanäle
- [ ] Manuell: Gabber/Hardcore ohne Haken; Guitar Wall L+R

### 2026-08-15 – Gold blocks + 4× OS + release kit

- [x] Delay: Hermite-4 (Lagrange-6 klingelte), 1-Pol-Dämpfer, kein Crossfeed
- [x] Reverb: 4 Allpass, Mono-Summe ins Freeverb (Stereo-In kämmte bei 520 Hz)
- [x] hardclip n=24; Default-OS **4×**
- [x] `NEUROKORE-0.9.0/` mit VST3, Standalone, EULA, Docs, optional Setup.exe
- [ ] Manuell: Inno Setup kompilieren wenn ISCC fehlt; 4× CPU in der DAW

### 2026-08-14 – Club split (no hidden post-EQ)

- [x] Main = Click+Mids (HPF ~90, Dip 320 Hz, Air-Shelf)
- [x] Scream höher (2–7 kHz), Body enger (LPF ~130–145)
- [ ] Manuell: Kick Rumble auf 909 — Click getrennt vom Floor

### 2026-08-14 – Transient fidelity (Serum-knackig)

- [x] hardclip n=5 → n=16 (am Ceiling ~96 % statt ~87 %)
- [x] Env/osc-Filter + EQ: 0.8 ms Smoothing statt 20 ms
- [x] Extra-IIR-AA nur noch ohne Oversampling (FIR macht den Rest)
- [ ] Manuell: Kick Rumble @ 2× vs 4× OS, Click gegen Serum 2

### 2026-08-14 – Club from hardcore one-shot refs

- [x] Refs: Disorder / Noitification / Wow — 375 ms @ 160, crest ~1.4, held clipped sine + sub
- [x] Kick/Warehouse/Hardcore/Gabber: wow-LPF + tuned delay + octaver, no hall duck
- [x] Scream-Bus + env-Drive auf dem Hit (helle Distortion + Dynamik bleiben)
- [ ] Manuell: Kick Rumble Tune ~15 ms auf eine 909, vergleich Wow/Disorder

### 2026-08-14 – Club ballern pass

- [x] Alle 8 Club-Presets: Defaults sitzen oben im Range (Drive 11–14, Clip-Ceiling 0.24–0.58)
- [x] Kick / Warehouse: Dry-Brick + Punch-EQ + Rumble-Send ~1.1 + Duck 0.8+
- [x] Gabber: env-Drive `a*(1.05+e*env)`, Ceiling 0.24, Sub-Default 1.05
- [ ] Manuell: Kick Rumble / Gabber auf eine 909 — muss sofort knallen

### 2026-08-14 – Club rumble rewrite

- [x] Kick / Warehouse: distort + dark reverb + duck
- [x] Gabber: env-Drive auf den Mids + Sub-Bus
- [x] 8×: Buffer auf 8× vorreserviert; Index erst in prepare
- [ ] Manuell: Gabber-Kick, Sub/Dyn; 2×→8× ohne Knack

### 2026-08-14 – 8x / gabber

- [x] Gabber Drive: dynamic clip + sub
- [x] OS bank never shrink; no audio-thread grow on 8×

### 2026-08-14 – Explorer type + rust comments + sound line

- [x] Preset-Ordner 16.5 pt / 30 px Zeile
- [x] Formel-Kommentare rostrot (`comment()`), nicht Grün
- [x] Jedes Factory-Skript: `# How it sounds:`

### 2026-08-14 – IN scopes follow L/BOTH/R

- [x] `pushInput` nach `InputRouter` (nicht mehr der rohe Host)
- [x] Mix 0 bleibt ungeroutet
- [x] Help: L/R → senkrechter Stereo-Strich

### 2026-08-14 – Overlay follows editor resize

- [x] `resized()` legt offene Overlays auf `getLocalBounds()` (schwarzer Scrim)
- [x] Preset Explorer: Panel füllt das Fenster minus 24 px und wächst mit
- [x] Andere Overlays: Scrim wächst, Panel bleibt am gesetzten Max

### 2026-08-14 – Full identity NEUROKORE 0.9.0

- [x] Plugin-Code `NRKO`, Bundle `com.NEUROKLAST.NeuroKore`, PLUGIN_ID `nrko01`
- [x] AppData `NEUROKLAST/NeuroKore`, Datei `neurokore.lic`
- [x] CMake: `NeuroKore` / `NeuroKoreTests` / `NeuroKoreIssuer`
- [x] Version 0.9.0 (Tester-Stand, 1.0 ist Verkauf)
- [x] Alte NeuroKore-.lic werden beim Import noch akzeptiert
- [ ] Manuell: Lizenz neu importieren (neuer Ordner); DAW-Scan auf NEUROKORE / NRKO

### 2026-08-14 – Club presets + explorer

- [x] Glitch Laboratory: kein Ping-Pong, kein LFO-Filter, Note-Grid Delay
- [x] +11 Club/Cyber: Neon Clip, Chrome Fold, Data Mosher, Kick/Warehouse Rumble, Hardcore, Gabber, Acid Hash, Tekno Comb, Industrial Gate, Hoover Dirt
- [x] Preset-Explorer: Folder-Liste + All/Factory/User-Chips + Trefferzahl
- [ ] Manuell: Club-Ordner, Kick Rumble auf eine 909, Fold weitet die Wave nicht (anderes Feature)

### 2026-08-14 – IN/OUT stereo field + loudness

- [x] ScopeDeck: Wave + Goniometer + L/R-Loudness, gleiche Zeilenhöhe
- [x] Wave schmaler wenn Extras offen; `<<` / `>>` klappt Field+Loudness ein
- [x] Stats aus WaveformCapture (kein Audio-Thread-Alloc)
- [x] Help: Bottom-Zeile beschreibt Field / LU / Fold
- [ ] Manuell: IN/OUT Field folgt Width-Presets; Fold weitet die Wave wieder

### 2026-08-14 – Compact header + edge-cropped NK

- [x] `resources/img/nk_logo.png` = `screenshots/NK Logo Red Bold.png` (1495×774, edge-cropped)
- [x] Toolbar `0.09` → `0.045`, clamped 32–38 px (half of ~72 px)
- [x] BrandLockup: logo 26 px, wordmark 11–15 / 9–11.5, tight inset
- [x] Preset-Chip 28 pt → 15 pt
- [ ] Manuell: NK sitzt in der Button-Zeile, HUD bleibt frei

### 2026-08-14 – DSL gate + dynamics plan

- [x] `gate1:` stereo-linked, hyst/hold/range, optional `source = sidechain`
- [x] `comp` knee / makeup / hpf / sidechain
- [x] `limit1:` in-chain peak limiter, no lookahead
- [x] `xover1:` LR4 → buses low/mid/high
- [x] several `irN` slots; full-width editor button → drop / change / clear
- [x] IR panel: Load (file chooser) + drop + Clear, one window per slot

### 2026-08-14 – Note-range LFO + AMS RMX knackt

- [x] `osc freq` / `osc sync` auf Note-Knob = ein Zyklus pro Note (nicht ms als Hz)
- [x] Sidechain Pump: `Rate [1/1, 1/16]` + `sync = a`
- [x] AMS RMX Nonlin: feste Size, dunkleres Damp, Recovery-LPF; min sizeScale 0.50
- [ ] Manuell: Pump auf 1/4 bei 120 BPM; Snare durch AMS ohne Knacken

### 2026-08-14 – Stereo Guitar Wall + Cyberpunk Drive

- [x] Factory: `Stereo Guitar Wall` — Mesa L / 5150 R, two DI takes, `channel = left|right`
- [x] Factory: `Cyberpunk Drive` — guitar chain with bitcrush/fold/metal comb + Level
- [x] `Glitch Laboratory` rewritten: HPF, shorter delay, makeup Level (was quiet wet-delay)
- [x] Tests: requireBlock + stereo independence + loudness
- [ ] Manuell: zwei DI-Takes hard-pan in BOTH, Cyberpunk Drive vs. alte Glitch-Klage

### 2026-08-14 – AU ohne lokalen Mac (GitHub Actions)

- [x] Job `AU (macOS)`: `NeuroKore_AU`, ad-hoc codesign, Install nach Components
- [x] `auval -v aumf NRCO NRKL` + pluginval auf dem `.component`
- [x] Artifact `NeuroKore-AU-macOS`; `workflow_dispatch` für manuellen Lauf
- [ ] Ersten CI-Lauf auf `macos-latest` prüfen (juceaide/auval/UI)

### 2026-08-14 – AU plugin (Logic / GarageBand)

- [x] CMake: `FORMATS … AU`, `AU_MAIN_TYPE kAudioUnitType_MusicEffect`, `AU_SANDBOX_SAFE`
- [x] Projucer: `buildAU` + `aumf` + MIDI in; kein `{2,2}` Channel-Lock (Sidechain-Bus)
- [x] macOS: Copy nach `~/Library/Audio/Plug-Ins/Components/` + resources am AU-Target
- [x] Tests: Stereo/Mono + optionale Sidechain, `acceptsMidi()`
- [ ] Manuell auf einem Mac: Artifact in Logic laden

### 2026-08-13 – OS crackle / Acid Line / AUDIO / Meter / comments

- [x] Oversampling: integer latency im JUCE-Ctor, `roundToInt`, Sidechain+IIR+DSL-Ringe reset
- [x] 8×→4×: scriptBuffer logische Länge kürzt (kein Silent-Half-Block-Glitch)
- [x] Acid Line: Filter-Coeffs alle 8 Samples; Q-Default 1.8 (max 2.6)
- [x] AUDIO-Button nur `isStandaloneApp()` (Cubase/VST nie)
- [x] OS-Wechsel ohne Device-Suspend; Factory-Kommentare aus BinaryData
- [x] Quick-Template-Insert entfernt
- [x] Preset-Sterne 1–5 + Export/Import `.nrk`
- [x] Kein Copy nach Common Files
- [x] Loudness-Meter: schnellere VU + angular Hull wie Mix-Slider
- [x] CpuProtect Dry-Pfad + SAFE in der Statuszeile
- [x] SAFE: Overrun-Schwelle + Auto-Retry (kein Dauer-Dry)
- [x] Factory-Scripts: `#` Kommentar pro Preset / Param / Block (Generator)
- [ ] Manuell: OS 2×→4×→8×→1× ohne Dauerknacken; Acid Line ohne Haken

### 2026-08-13 – Offline license + HUD lockup

- [x] NK-Logo hart auf 20 px; Layout nie über die HUD-Zeile
- [x] RSA-signierte `.lic` Datei; Issuer-App nur E-Mail
- [x] Unlizenziert: Mix nach 20 min auf 0 (Signal bleibt dry)
- [x] License-Button importiert die Datei nach AppData

### 2026-08-13 – Meter hang, note knobs, status columns

- [x] Loudness-Meter: Dry/SAFE/Lock-Miss rufen `publishOutputMeter`
- [x] `param a = Time [1/1, 1/16]` rastet auf Zählzeiten; Formelwert = ms
- [x] Statuszeile: feste Feldbreiten (CPU/MIX/LAT/SR/Mode), kein Schieben

### 2026-08-13 – EQ + external sidechain

- [x] `eq1:` peak / notch / lowcut / highcut / lowshelf / highshelf (freq, Q, gain)
- [x] Optionaler Stereo-Eingang Sidechain; DSL `sc` / `sc_l` / `sc_r`
- [x] `env1: source = sidechain` folgt dem Extra-Input

### 2026-08-13 – Tooltip / preset type / meter / amps

- [x] Ein TooltipWindow am Editor (Scopes ohne eigenes)
- [x] Preset-Chip 28 pt, zentriert
- [x] Meter-Glitch cubisch ab ca. −36 dBFS, unten ruhig
- [x] Native `octaver` / `vocoder` Blöcke
- [x] +16 Factory: JCM/SLO/Orange/AC30/5150/Deluxe, SVT/Porta/Glass/Bassman/B15/Trace, Octaver, Vocoder

### 2026-08-13 – EQ retrofit + hardware comps/rooms

- [x] Bandpass-as-EQ in TS/Plexi/Vox/Bass Architect/Vocal Chain/Studio Strip → peak/shelf/notch
- [x] Multiband Glue + Envelope Shaper
- [x] 1176 / All-In / LA-2A / SSL / Fairchild / dbx 160 / CL-1B / Neve / Distressor
- [x] Space Echo, Memory Man, Echoplex, 2290 grid, EMT 140, Lexicon hall, AMS Nonlin, Spring
- [x] Rhythmic Gate Delay: Dry+Echo-Bus, Duck nur nass (kein lerp nach mix=1)

### 2026-08-13 – Preset table tags + default sort

- [x] Default-Sort Name A–Z (Header-Pfeil)
- [x] Spalte Tags zwischen Category und Source

### 2026-08-13 – Editor autocomplete, Help, Bypass Mix lock

- [x] Autocomplete nur Ctrl/Cmd+Space
- [x] Help-Schrift 16 pt, Kapitel + Tutorials (Drive, Delay, Duck, Sidechain, Vocoder, Amp)
- [x] Bypass sperrt den Mix-Slider und hält Mix auf 0

### 2026-08-13 – Factory preset honesty (a–f)

- [x] Audit: 116 factory all pass quality gate; 6 scripts still declare g/h
- [x] Trim Studio Channel Strip / Rhythmic Gate Delay / Cinematic Space / Phaser Lab / Vocal Chain Pro / Glitch Laboratory to a–f
- [x] Dummy paramC/paramD metadata stop
- [x] Doubler AM + Shimmer Drive get real delay/reverb
- [x] Cinematic Space gets ms encode/decode
- [x] templates.json drop 7–8 knob names
- [x] Bitcrush lo-fi quick template recovery LPF
- [x] +14 Mix-Desk Presets (Side Delay/Hall, Vocal Send, NY Drum Bus, Mono Below, MS Imager, …)
- [x] Loudness-Meter: VU attack/release, kein OpenGL-VSync-Zucken
- [x] Hilfe: nur Bedienung, keine Build-/Framework-Details
- [x] Preset-Tags + Suche nach Formel/Tags (delay, mid side, tape)
- [x] NK-Logo klickt zu https://neuroklast.net
- [x] +11 Psychoacoustic / Cinematic Factory (Haas, Score Hall, Trailer Impact, …)
- [x] NK-Logo unter HUD, kleiner; Assemble nutzt chromeBounds
- [x] Preset-Spalten sortierbar (Name/Category/Source/Author)
- [x] L/BOTH/R + Combos 32 px mittig, gleiche Platte
- [x] HUD/Boot: Neuroklast OS (kein Netrunner)

### 2026-08-13 – Cinematic Cyber-UI FX (`feat/cinematic-ui-fx`)

- [x] `CyberFxDirector` / `CyberSequence` / `decodeGlitchText` + Unit-Tests
- [x] Cached Backdrop (kein 480× Hex-Text pro Frame)
- [x] Modal enter/exit via VBlank, kein Host-Fokus-Diebstahl
- [x] Fenster-Assemble wie Neuroklast-Modals (Clip-Reveal ~340 ms), kein Scanline-Boot
- [x] Help: Kapitel-Liste + Schnellsuche
- [x] Preset-Browser merkt Auswahl und scrollt hin
- [x] Autocomplete nur kontext-sichere Vorschlaege
- [x] Formel-Check haelt Overlay sichtbar (min. 1.2s) mit Live-Status
- [x] Input L/BOTH/R Dreier-Switch statt zweier Toggles
- [x] Formel-Check bricht bei NaN/Inf ab
- [x] UI English-only (kein Language-Switch, kein de.txt)
- [x] Formel-Live-View scrollbar; Zeilenabstand 1.1
- [x] NK Red Bold Logo als App-Icon (Standalone ICON_BIG/SMALL + in-plugin `nk_logo.png`)
- [x] Hilfe zeigt nur das gewählte Kapitel (kein Sprung im Volltext)
- [x] Hilfe-Body ohne Roh-Markdown (** / ### / ---)
- [x] Gain-Slider entfernt; Mix cyber-Slider mit Drag-Glitch
- [x] Preset-Kategorie/Scope bleibt beim Schliessen erhalten
- [x] Factory + leere User-Author = NEUROKLAST
- [x] Status: LIVE/BYPASS + LAT smp/ms + AUDIO (Standalone SR)
- [x] Delay-Preset-Wechsel: OS/DC/Sidechain reset (kein klebendes Knacken)
- [x] Hilfe-Body: JetBrains Mono + `applyFontToAllText` (kein Apex-ALL-CAPS)
- [x] Hilfe-Tabellen als Definitionen, nicht `AREA — PURPOSE`-Wand
- [x] Hilfe-Text operator-tauglich (Quickstart / UI-Map / Troubleshooting)
- [x] NK-Logo crop + BrandLockup optisch mittig mit PRESETS
- [x] L/BOTH/R angular wie die anderen Chrome-Buttons
- [ ] Manuell in Standalone: Boot, Preset auf/zu, FX aus

### 2026-08-13 – DSL Multi-Bus (Send-DAG)

- [x] Parser: `bus name:` / `send:` / `out:`; implicit `main`; reserved names
- [x] `BusGraph` DAG (kein Forward-Send)
- [x] `SignalChain` per-bus Buffer + Mixdown; serial Fast-Path
- [x] Stages-Tag + Autocomplete-Snippet
- [x] Docs: DSL_REFERENCE + ARCHITECTURE
- [ ] Mixer-UI / Feedback-Matrix / N>4 — bewusst nicht in v1

### 2026-08-12 – Crackle-Fixes (Signal Chain)

- [x] ADAA: per-channel State, **kein** Reset pro Audio-Block (nur Formel-Load)
- [x] Reverb: Size ohne Buffer-Clear (feste Ringe, variable Länge)
- [x] OutputSanitizer: Delay/Reverb-Tails bei stillem Dry behalten
- [x] AutoGain: Tails nicht auf 0 ducken
- [x] Delay: sichere Read-Wraps, Peak-Soft-Limit, Time-Smoothing
- [x] Regressionstests `CrackleFixesTest` — 46002 total grün

### 2026-08-12 – RT-Architektur-Verträge (kein Magic-Number-Whack-a-Mole)

- [x] `LatencyAlignedSidechain` single owner (Dry-Timeline == OS/DryWet-Latenz)
- [x] Eine Residual-Policy: nur Sanitizer; AutoGain **nie** muten
- [x] NoiseGate strukturell `setBypassed` (kein -120 dB Workaround)
- [x] RT-Verträge in `docs/ARCHITECTURE.md` + Architektur-Tests (Impulse-Delay, Misalign-Beweis)
- [x] Prinzip: Crackle → Timeline/State härten, nicht Thresholds tunen

### 2026-08-12 – Non-UI Hardening (TDD A1–A10)

- [x] Diagnostics default off; OS default 2×; AutoGain APVTS default 0
- [x] Dual-chain audio entfernt; `mixDryWetContinuous` + aligned dry
- [x] Peak-Ownership: Sanitizer Peak iff Polisher None
- [x] Delay quiet-bleed entfernt; feedback pole + DC/damp only
- [x] Stage silence-leak nur bei feedback; NoiseGate aus Chain
- [x] `ArchitectureHardeningTest` + full suite grün

### 2026-08-12 – Mix0 / 8 Knobs / Content

- [x] Mix 0%: pure dry early-out (kein DSL/OS auf Dry-Path)
- [x] 8 User-Params a–h (historical Mix0 peak: Engine, APVTS, UI 4×2, Formula colors); later cut to 6 (`kNumUserParams=6`)
- [x] Bare `e` = Knob (nicht Euler); Factory 116 + complex Templates
- [x] Functions-Docs: param a–h, lerp, clip→LPF, MS

### 2026-08-12 – UI polish + Cubase delay crash + Manual

- [x] Encoding: ASCII chrome (no bullet/emdash/ellipsis on Apex); mono for formula
- [x] Embedded JetBrains Mono; editor default **18 pt**; **+/-** size controls
- [x] Max **6 knobs** a–f, layout 2×3 left, larger formula editor
- [x] Assemble removed; Quick templates in editor
- [x] User preset **Author** + Category (artist packs)
- [x] English `docs/USER_MANUAL.md` + in-plugin Help (embedded)
- [x] RT `processLock` for formula swap vs processBlock (Cubase delay crash)

### 2026-08-12 – Test-Suite Hang/Crash behoben + optimiert

- [x] Hang/AV an `PresetManagerTest` / Processor-Tests: `ScopedJuceInitialiser_GUI` in `tests/main.cpp`
- [x] Use-after-free: `cancelPendingUpdate()` im Processor-Dtor (AsyncUpdater)
- [x] Factory apply: kein `callAsync` mit Stack-Referenz
- [x] Factory-Load im Ctor: Singleton-Skip wenn schon geladen; Diagnostics-Log nur wenn enabled
- [x] Expect-per-sample reduziert (`TestHelpers`); Factory quality sparsely (≈ alle 20.)
- [x] Delay-FB-Tail-Test misst nach 2. Recirc (nicht 1. Echo) — flaky Assertion behoben
- [x] Full suite: **TOTAL: 1057 passed, 0 failed** (~0.2–2 s Release)

### 2026-08-12 – Delay / Reverb / Mid-Side (voll)

- [x] DSL-Block **`delay`**: Ringpuffer bis 2 s, `time` ms, `sync=1/8`, feedback, damp LPF, mix, ping-pong
- [x] DSL-Block **`reverb`**: Freeverb-Stil (8 Combs + 4 Allpass), size/decay/damp/mix/width
- [x] DSL-Block **`ms`**: encode/decode; `channel=mid|side` auf Stage + Filter
- [x] Templates + Factory: echte Delay/Reverb (keine y_prev-Fakes mehr als „Echo“)
- [x] 108 Factory-Presets (5 Delay, 5 Reverb, 3 MS/Mastering-Erweiterungen)
- [x] Unit-Tests `DelayReverbTest`; alle Factory load+apply grün
- [x] `docs/DSL_REFERENCE.md` aktualisiert

### 2026-08-12 – AudioDiagnostics (NaN / Sprung / Kratzen)

- [x] `AudioDiagnostics`: RT-sicherer Ringpuffer + Message-Thread File-Log
- [x] Scans in `DspEngine`: **Input** (Dry nach Gain), **PostDsl** (OS wet), **FinalOut**
- [x] Erkennt: NaN/Inf, harte Sample-Sprünge (`|Δ| ≥ 0.28`), Crackle-Cluster (≥4 Soft-Jumps)
- [x] Kontext pro Event: Preset, Formel-Kopf, a–d, Gain/Mix, SR/BS/OS, blend/ramp/lim, Input L/R
- [x] Tag `dsp-introduced` vs `input-sourced` (Vergleich Input- vs Output-Jumps)
- [x] Log-Datei: `%AppData%/NEUROKLAST/NeuroKore/audio_diagnostics.log`
- [x] Schalter: `Config::kAudioDiagnosticsEnabled` (+ Thresholds)
- [x] Unit-Tests `AudioDiagnosticsTest` (3307 total grün)

### Phase 3 – God-Class + Musikalische Features ✅

- [x] `PluginProcessor` God-Class aufgelöst in DspEngine + ScriptManager + WaveformCapture
- [x] DSL-Variable `t` (Zeit in Sekunden), `sr` (Samplerate), `pi`, `ch` implementiert
- [x] Feedback-Schutz: `kFeedbackLeakFactor = 0.9999f` für `y_prev`/`x_prev` in Stage
- [x] Kanal-Routing: `channel = left|right|both` pro Stage
- [x] Mid/Side-Encoding: `ms_encode = true`, `ms_decode = true`
- [x] MIDI-Variablen: `midi_note`, `midi_freq`, `midi_vel`, `midi_gate`, `midi_bend`, `midi_mod`
- [x] Tempo-Sync für Osc-Blöcke: `sync = 1/4` etc.
- [x] `getTailLengthSeconds()` delegiert an `SignalChain::getMaxTailTime()`
- [x] MIDI-Trigger für Env-Blöcke: `trigger = midi_gate`
- [x] `MidiVariableMapper`: Klasse mit `std::atomic<float>` (RT-safe)
- [x] Tests für alle neuen Features in `NeuroKoreExtrasTest.h`
- [x] UserManual EN + DE aktualisiert
- [x] `ValidationTypes.h` extrahiert (keine zirkulären Includes)

### Phase D Audit-Fixes ✅ (2026-06-29)

- [x] Preset-Load → Editor-Sync (`ChangeBroadcaster` + `syncFromProcessor`)
- [x] Session-State: `varName0`–`varName3`, `language`
- [x] Bypass-Button: toggelt `dryWet` (0 ↔ gespeicherter Mix)
- [x] Toten Parameter `modFrequency` entfernt
- [x] Legacy-DSP aus Plugin-CMake entfernt (bleibt in Tests)
- [x] CI: pluginval ohne `|| true`
- [x] `StagesContentComponent`: Signalkette-Overlay für `stagesButton`

### Phase C Performance ✅ (2026-06-29)

- [x] `DspEngine`: Dry/Wet + Output-Gain blockweise (kein per-sample `getSubBlock`)
- [x] `processBlockSmoothed` Fast-Path: SIMD `Stage::processBlock` + `Filter`/`Comp` blockweise
- [x] `Filter`/`Comp`: `processBlock` + Coefficient-Updates nur jedes 8. Sample (Slow-Path)
- [x] `Stage`: SIMD `x_prev` pro Lane; `usesTimeVariable`/`usesFeedback` Flags
- [x] `LookupTables`: exp/log bei Init; erweitertes `prepareFromScript`

### Phase B Audit-Fixes ✅ (2026-06-29)

- [x] LPF auf `osSpec` (korrekte Cutoff nach Oversampling)
- [x] RT: `oldSignalChain = signalChain` auf Audio-Thread entfernt
- [x] `SignalChain::scriptLock` (TryLock im Audio-Thread, Lock in `loadScript`)
- [x] `testFormulaStability` nutzt `processBlockSmoothed` (Production-Pfad)
- [x] `factory_presets.json` auf Zeilen-Syntax migriert (75 Factory-Presets, 15 Kategorien)
- [x] UI-Modernisierung: `NeuroKoreLookAndFeel`, WeightedLayout-Grid, Preset-Tabelle mit Kategorie-Spalte
- [x] `FactoryPresetLibrary` + Unit-Test (Load/Apply-Sampling)
- [x] `docs/ARCHITECTURE.md` Signalkette korrigiert

### Phase A Audit-Fixes ✅ (2026-06-29)

- [x] Knob-Routing: `Stage` liest `a`–`d` aus `variables` (Production-Pfad)
- [x] `processBlockSmoothed`: Smoother-Advancement sample-major (Stereo-Smoothing-Fix)
- [x] `docs/DSL_REFERENCE.md` auf reale Zeilen-Syntax umgeschrieben
- [x] `tests/main.cpp`: Exit-Code bei Failures
- [x] `PresetManagerTest` / `SignalChainTest` repariert + `processBlockSmoothed`-Test
- [x] `Resources/` → `resources/` (Case für Linux/macOS)

### 2026-08-11 – Factory-Presets ladbar + UI/UX Slider-Fix (master)

- [x] `FactoryPresetLibrary` lädt `resources/factory_presets.json` (Pfad-Fallbacks)
- [x] 83 Factory-Presets in 16 Kategorien — alle `loadScript` + `applyPreset` grün (anti-alias clip)
- [x] DSL: param-Ranges, Stage-Map, Osc-Freq-Formeln, Filter `+`/`*` Modulation
- [x] UI: größere Knobs, längere Mix/Gain-Slider, größerer Preset-Browser, Double-Click Load
- [x] Unit-Test deckt alle Factory-Presets ab

### 2026-08-11 – y_prev Perf + Gain/Mix + softclip

- [x] `y_prev`/`x_prev` force no longer whole-chain sample path (Stage scalar only)
- [x] Osc/Env modulation path: advance once per sample, not per channel
- [x] `softclip` → smooth algebraic (less HF alias)
- [x] UI: only Gain + Mix (no Output Gain)
- [x] Current preset name + table highlight
- [x] Auto-gain boost cap 3.0; linear dry/wet; quieter-on-engage fix
- [x] Factory outputGain → 0 dB; resonance caps on heavy presets

### 2026-08-11 – Factory-Presets Anti-Alias Clip + Clipper-Templates

- [x] Alle Factory-Presets: kein bare `hardclip` mehr — immer `hardclip(softclip(...), ceiling)`
- [x] LPF nach starkem Clip / Fold / Bitcrush (Tone/Cab/AA-Recovery)
- [x] Drive-/Resonanz-Caps auf knisteranfälligen Presets
- [x] 5 neue Utility-Clipper-Presets (Soft Clip Tone, Soft-Knee Ceiling, Parallel Soft Clip, Hard Clip Pedal, Diode Clip Stack)
- [x] `resources/templates.json`: Clipper-Best-Practice-Templates (inkl. Full-Script-Rezepte)
- [x] Generator `scripts/generate_factory_presets.mjs` als Single Source of Truth (83 Presets)
- [x] `docs/DSL_REFERENCE.md`: Clipper-Best-Practice-Tabelle + Beispiele

### 2026-08-11 – Hybrid-Pfad + ADAA + OS/Preset-Switch

- [x] Osc/Env pre-render (`modLane`) — Filter bleiben blockweise (kein whole-chain sample path)
- [x] Stage nur lokal sample-weise bei mod/feedback/nonlinear (ADAA)
- [x] 1st-order ADAA für softclip/hardclip/tube/diode
- [x] tube weicher + drive cap; hardclip knee 12%
- [x] Preset-Wechsel: kurzer Crossfade, kein OS-Reset, switchRamp 0→1
- [x] OS-Index aus `AudioParameterChoice::getIndex()` (normalisierter Cast-Bug)
- [x] OutputSanitizer: soft asymptotic ceiling

### 2026-08-11 – Optimize smart + robust

- [x] Multi-Line-DSL Optimizer (param/stage/filter erhalten)
- [x] Built-in Rewrites + optimizations.txt; kein totes `saturate()`
- [x] Safety: Re-Parse + FormulaQuality-Regression-Reject
- [x] Structural: mildes LPF nach bare hardclip
- [x] Unit-Tests `FormulaOptimizeTest`

### 2026-08-11 – Goldstandard-Pass (Wellen 0–2)

- [x] AutoGain: Target nicht vor-blenden (Tests + korrektes Loudness)
- [x] FormulaQuality: hardclip/fold/bitcrush/y_prev ohne Recovery-LPF = **error**
- [x] Factory: Drone Layer + LPF; 93 Presets quality-gate
- [x] NoiseGate de facto off (Sanitizer only) — kein Dual-Gate-Chatter
- [x] Script-Swap: kein Audio-Drop bei TryLock-Miss
- [x] Soft-Ceiling nur außerhalb ±1.15 (kein Always-On-Softclip-Gain-Bug)
- [x] templates.json Best-Practice only; functions_en/de 35 Funktionen
- [x] DSL_REFERENCE Knee/ADAA/OS; Version 0.2.2; SpectralSmokeTest
- [x] **3267 Tests grün**

### Nächste Schritte (Priorität)

- [x] `stagesButton` → `StagesContentComponent` (Signalkette-Übersicht)
- [x] Factory-Presets tatsächlich im UI laden (nicht nur JSON-Datei)
- [x] Licensing: Offline RSA-.lic + Issuer (kein Server in der Testphase)
- [x] Windows installer (Inno Setup) + zip; macOS .pkg still open
- [ ] UI: Progressive Disclosure (Settings-Panel für Oversampling/Language)
- [ ] UI: Immediate Feedback (Syntaxfehler inline-highlighting)
- [ ] UI: 8pt-Grid konsequent anwenden
- [ ] SLEEF/vDSP für echtes SIMD bei transzendenten Funktionen
- [ ] Polyphoner Betrieb / Note-per-Channel
- [ ] Preset Drag-and-Drop Import/Export
- [x] `autoGainCompensate()` optimieren
- [x] Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
- [x] AU-Format für macOS aktivieren
- [x] `NeuroKore.jucer` um neue Core-/UI-Dateien aus PR #195 ergänzt (Projucer-Build wieder konsistent)
- [x] CMake JUCE-Einbindungslogik für `juceaide` robust gemacht (`JUCE_BUILD_HELPER_TOOLS` + FetchContent/Add-Subdirectory-Pfad)
- [x] `curl` aus `NeuroKoreTests` entfernt (Windows-Linking vermeiden)
- [x] Windows-Buildskripte ergänzt: `build_debug.bat`, `build_release.bat`
- [x] Windows-Buildskripte auf Ninja Multi-Config umgestellt (VS-Generator `juce::juceaide` Custom-Command-Bug umgangen)
- [x] Windows installer (Inno Setup) + zip; macOS .pkg still open
- [x] NRK-Presetformat: DSCR-Chunk für rohes DSL-Skript ergänzen
- [x] ExpressionEvaluator-Hotpath: Template-Block-Evaluierung + SIMD-Funktionspfade für sin/cos/tanh/exp/abs/clamp

### Kürzlich abgeschlossen

- [x] Phase 2 Features implementiert (Resizable UI, MIDI Learn, Undo/Redo, Oversampling ComboBox)
- [x] CI/CD Pipeline mit pluginval
- [x] `kEnableLicensing = false` für Dev-Builds gesetzt (Config.h)
- [x] CMake doppelte Sources gefixt
- [x] `getScript()` thread-safe gemacht (SpinLock-Guard)
- [x] `getChain()` atomic getter zu `SignalChain.h` hinzugefügt
- [x] `tests/DSLParserTest.h` erstellt (13 Tests)
- [x] `tests/SignalChainTest.h` und `tests/LookupTableSmootherTest.h` in CMake Test-Target aufgenommen
- [x] `PresetManager` auf NRK v2 mit `DSCR`-Chunk erweitert (STAT-Fallback kompatibel)
- [x] `PresetManagerTest` erweitert: DSCR-Chunk write/read validiert
- [x] `PresetManager` Chunk-Parsing gehärtet (Entry-Limits + Read-Checks + Negative-Offset/Length-Guards)
- [x] `PresetManagerTest` erweitert: DSCR-Chunk-Priorität gegenüber `STAT` verifiziert
- [x] `docs/AGENTS.md` erstellt (maschinelle Agent-Guidelines)
- [x] `resources/factory_presets.json` mit Factory-Presets erstellt
- [x] `resources/templates.json` auf 15+ Kurzformeln erweitert
- [x] Vollständige Codebase-Analyse durchgeführt
- [x] Dokumentationsstruktur erstellt (`docs/` Verzeichnis)
- [x] `AGENTS.md` / `docs/AGENTS.md` / `README.md` um verpflichtende Session-Abschluss-Schritte ergänzt

---

## Bekannte kritische Probleme

> Diese Tabelle enthält nur aktive, noch nicht behobene kritische Issues.

| # | Problem | Datei | Priorität |
|---|---|---|---|
| 1 | Licensing-Backend noch Placeholder (Produktionsserver fehlt) | `Config.h` / `LicenseManager.cpp` | 🟡 Hoch |
| 2 | Slow-Path noch per-sample (Osc/Env/Feedback/t) | `SignalChain.cpp` | 🟡 Mittel |


Vollständige Analyse: `docs/ANALYSIS.md`

---

## Wie man dieses Dokument aktualisiert

**JEDE** Coding-Agent-Session MUSS dieses Dokument aktualisieren:

1. `Letzte Aktualisierung` Datum aktualisieren
2. Geänderte Module in der Status-Tabelle anpassen
3. Abgeschlossene Aufgaben in `Kürzlich abgeschlossen` verschieben
4. Neue Aufgaben in `Nächste Schritte` eintragen
5. Behobene kritische Issues aus der Tabelle entfernen

Siehe auch: `docs/AGENT_WORKFLOW.md`
