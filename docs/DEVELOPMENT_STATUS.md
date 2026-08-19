# Entwicklungsstand NEUROKORE

**Stand:** 2026-08-19 (phase-vocoder `pitch` + gate/comp/chainwide ceilings)  
**Version:** 0.4.8-alpha  
**Branch:** `master`

Alte Tages-Checklisten: `docs/archive/DEVELOPMENT_STATUS_HISTORY.md`.

## Jetzt

Web-UI-Umbau (Strangler): DSL + `SignalChain` bleiben der Compiler. JSON-AST ist das UI-Dokument.

| WP | Stand |
|---|---|
| WP0/WP1 AstJson + GraphOps + UI_READY-Latch + Telemetrie-Frame | **grün** — `tests/AstJsonTest.h` |
| WP2 WebView-Shell | **grün** — `tests/WebShellTest.h`. Web only. |
| WP3 Zustand + compile/ast | **grün** — `tests/WebCompileTest.h`, `web` Vitest. |
| WP4 Hack (Monaco) | **grün** — lint ohne apply; Edit/Save; 250 ms Marker. View = muted frame + `a[0.34]` inlays + header `# name` / sound line; Edit = accent + caret. Autocomplete compile-safe (after `type =` only enums). Save/Optimize-Apply runs `validateOnSave`. Validate overlay lists Parse/Braces/Enums/Ranges/MS/LR/Bus/Join/Jacks. Xover serial `out` is not a jack — mix is low/mid/high to OUT. Optimize empty → `Script already optimal`, no Apply. |
| WP5 Assemble | **grün** — React Flow: easy-connect, proximity-to-jack (not chip origin), pulse-edges, DnD grab/crosshair/pointer chrome, Delete/Backspace removes selected cable (`scriptAfterDisconnect` → `bus __park:` / graphOp disconnect) with formula undo, context menu, zoom-tiers, validation, L→R. ChipSpec catalog (`chipSpec` / `typeCode` / `minBodyPx`); expand no longer grows the chip. Inspect: enum `<select>` from `chipSpec.enums`, numeric clamp to `ranges`, blurb + bound knobs (unit, 2 dp). Board cell is 32 px. Audio tubes and jacks are 16 px on cell midlines. Height change is a 45° of at least one cell (`√2 × 32`), never a 90° H/V corner. Background: stacked React Flow `Background` (dots on the 32 cell, lines on the 128 block). Chips stay one cell off every tube. Drag snaps to the cell. Layout: elkjs layered RIGHT in a WebWorker (`ARRANGE` 64/128, `COMPACT` 32/32), then grid A* writes stored SVG `d`; `StaticGridEdge` paints `data.route` only. Ctrl/Cmd+A Arrange, Shift+A Compact. Agent rule: React Flow first (Background, snap, stored path) before a custom grid/router. LFO: east `mod` plug; `resolveLfoHz` (sync note @ BPM else freq) for lamp and glow-dot. Split/Join picker: Mid/Side and L/R; `mode = split`/`join` (encode/decode aliases); MS↔LR cables rejected. Bus chip opens `bus dirt:`; Join Signal is inA/inB/out with bindable mix 0.5; emit is `bus dirt:` + `join1: mix` (no second out: mix). Send: in/out + bottom `ctrl`, `kanal` enum; Multiband Split (xover) low/mid/high; Width width/delay/bass; Octaver 5 south params; Custom `+ input` + `scriptAfterRename`; Sidechain IN only when `host.sidechainOn`. Circuit DoF: selected∪hover path sharp (`data-focus`); soft = slight blur+opacity; `motionAllows("dof")` off/prefers-reduced → no blur; no grain. **Mute/Solo**: M/S buttons on blocks with input+output (not LFO/ENV); right-click menu Mute/Solo/Unsolo; soloed blocks override mute; visual feedback via opacity. **Plus button** (`+`) on each block opens inline picker to add next block. **Edge click** opens picker to insert block between. Inspect overlay closes only via X button, not outside click. |
| WP6 Chrome | **grün** — Knobs/Mix/OS/L-R, Footer 0–100, Presets (Explorer **Save As…** → Name/Author/Category, `.nrk` in `UserPresets/`; list+load include user; Import is `.nrk`/`.zip`), Settings, License. IR: Add → Cabinet IR; chip LOAD/CAB, double-click, Inspect, Stages, Terminal line open Impulse (Load / Play / Clear). File stays host state, not the formula. Inactive knob drag-bind activates from chipSpec range/unit; enum = N detents; display round2 + units. Circuit drag/connect is React Flow (`nodesDraggable`, `onConnect`, `getStraightPath` while dragging). Knob-bind preview uses RF `getSmoothStepPath` (`bindSmoothPath`) around chips. Native context menu blocked everywhere (`shouldBlockNativeContextMenu`); only OsContextMenu. Bare Space is host transport (`isHostTransportKey` + `hostKey` → Cubase play/pause), including while the WebView has focus; text fields still type a space. |
| WP7 Telemetrie | **grün** — NKTM-Frame, Canvas-Scope, WebGL-Gonio, kein JSON-Audio. LU-Balken lesen `telemetryStore` (nicht erstarrte ScopeDeck-Props); Demo-Peaks laufen mit. OS-Wechsel nutzt `fadeInRemain` über die OS-Latenz. |
| WP8 Default-Web | **grün** — `createEditor` is Web only. `NEUROKORE_WEB_EDITOR=0` does not reopen native. |

Produkt-Default ist der Web-Editor. Vite-HMR: `NEUROKORE_WEB_DEV_URL=http://localhost:5173`. Ohne WebView2: leere Web-Shell, kein Native-Fallback. Windows: statischer WebView2-Loader. Web-Chrome: Anthrazit + Neonrot/Gelb/Cyan, 45°-Schnitt, gestrichelter Chip-Rahmen, Sans-Titel. CRT-Scan + Chroma (kein Grain). Settings → About (Version, Formate, Kontakt). STUDIO-Chip schaltet LIVE/STUDIO. Header 52 px, Logo 44 px. Preset-Name ist der Titel (öffnet Explorer); kein Extra-Presets-Button; Pfeile bleiben. Circuit: Chip-Körper ist der Drag (kein Title-only-Handle); Details ist eine 26-px-Platte. Auto-ELK nur Erstes Laden / Preset, nicht nach Drag. Circuit: Name/Detail per Klick (kein Akkordeon). Circuit: sechs Knobs unten in einer Reihe (Buchstabe über dem Namen, nicht darübergelegt); L/BOTH/R in der Mix/OS-Zeile. LFO-Chip: Lampe pulsiert in Hz + Kurvenform. Motion: cubic-bezier, CSS-Plasma, kein Telemetrie-Tick auf den Röhren. Knob-Buchsen unten am Chip. Hover hebt die Leitung des Knobs. Knob: geschichtetes SVG (Metall-Fill, Notch-Track, Cyan-Bogen mit dashoffset + Bloom); Karte 45°-Schnitt + Mikro-Grid. Wert ist tippbar (Zahl oder 1/4). Signalleitungen: Drop-Shadow-Bloom auf der Röhre. Mausrad dreht Regler. Leitung: höchstens zwei Ecken auf freier Bahn, Diagonale statt 4-Ecken-Treppe; ELK legt die Kette links→rechts. Röhre sitzt auf der Buchsenkante (kein Gap). Chip wächst mit Sockets/Labels; Text bleibt im Body. Jede Buchse beschriftet. Terminal-Leiste: Edit, Validate, Optimize. Drei Workspaces: Unit (HUD: FFT-Balken, Signal-Path-Minimap, PEAK/RMS/Phase, AST-Checksum, DSP-Zeit, BUF, CORE_TEMP, OS-Mode, Cursor-XY; Logo: Band-RGB-Split, Transient-Slice, Clip-Rain, BPM-Pulse; kein Scope-Wellen-Fill, keine LFO-Miniwellen über den Knobs; logoMotion+faceModel aus bestehender Telemetrie/AST/Host, keine neuen C++-Felder), Circuit, Terminal. Validate/Optimize-Overlays. Overlay-Host: Backdrop-Blur + Clip-Path Auf-/Abbau (Full). Split-Overlays (Presets/Functions/Stages/Help): linke Ordnerleiste bleibt stehen, nur die Liste scrollt. Surface-Bloom auf Logo/Accent. Röhren-Welle mit log-Amplitude-Glow. CRT-Scan + seltener Jitter, Terminal-Scanlines. Vignette nur im Unit/Circuit/Terminal-Fenster, nicht auf HUD/Knobs/Footer. Settings → About. Footer: eine Welle + Stereofeld + Loudness, Quelle IN|OUT|BOTH, Delta, altes Kontextmenü. Fenster hält 1280:860. `src/ui/*` bleibt im Tree. `processLock` unverändert. Vite-Demo: Factory-Load schreibt AST + Knobs + Mix + `lastValidScript` (nicht nur Draft). Mix-Slider schreibt `hostStore.mix`. Footer-OS folgt dem Mix-Index (`1/2/4/8`), nicht einem toten `osFactor: 4`. Header bleibt in 1280: Preset-Titel schrumpft, BYPASS bleibt sichtbar. Auto-Arrange merkt sich die Board-IDs erst nach ELK — sonst frisst Strict Mode den ersten Paint und die Kette bleibt eine Wurst. Preset-Explorer hat ein Close, nicht zwei. Fenster-Fit snappt **abwärts**, nie größer als das Host-Fenster. Circuit-Knobs: Buchstabe sitzt über dem Namen (flow), nicht auf dem Titel.

## Offen

1. Native canvas/FX/chrome is gone (`PcbRouter` too). Left in `src/ui`: WebPluginEditor, StandaloneAudioSettings, MidiLearnManager.
2. Per-Block-DSP-Welle (heute IN/OUT-Telemetrie, Glow log-Amplitude).
3. ENV is a bus tap (audio in + mod out), not an LFO. Circuit draws IN/prev → env.in. Patch `env1` onto a param (`y = x * env1`) to hear it.

## DSP (0.4.8-alpha)

- **`pitch`**: phase-vocoder (FFT 1024 / hop 256), `semitones`/`shift`, `mix`, `formant`, optional `sync`, `ceiling` default −0.3 dB. Latency reported with IR latency.
- **Ceilings**: optional `ceiling` on `gate` / `comp` (default 0 dB soft-cap). Chainwide soft-shape only for `|x| > 1`.

Delay liest linear mit Integer-Wrap (`delayRead`). Hermite-4-Punkt hat jedes Delay-Intervall Index 0 mit `N-1` gemischt — das war das periodische Knacken.

Arrange is ELK layered RIGHT (readable L→R, 128/64 air). Compact wraps the chain into 1–3 rows with 32 px air so tubes stay at least one grid long. AST-JSON zeichnet MS-Gabeln und implizites OUT. Jeder Chip klappt Details auf (Schema-Zeilen, nicht nur Drive). Rechtsklick aufs Board öffnet den Add-Picker (Kategorien links, Blöcke rechts, kein verschachteltes Flyout). Custom ist `y =` plus extra Eingänge. Ctrl+Z / Ctrl+Y ist Formel-Undo. Circuit-Knobs: Bind-Buchse oben, keine A–F-Buchstaben auf dem Titel. Zählzeiten nur bei `param … [1/4, 1/16]`, nie bei numerischen Ranges. Ungetaggtes Chip zwischen MS-Encode/Decode liegt auf **beiden** Schienen (Screenshot 072440); `connectJack` von Encode.`side` schreibt `channel=side` und Mid läuft implizit Encode→Decode. Chip-Details-Platte sitzt außerhalb des Body-Clips. User-xy bleibt bis Arrange / neues Graph.

## Native Circuit (Referenz, nicht mehr der Editor-Default)

Quelle: `screenshots/Screenshot 2026-08-16 231501.png` (Phaser Lab), `231939.png` (Settings).

| Fläche | Ist | Soll |
|---|---|---|
| Circuit tidy | Soft-Contract: `OUT(464,16)` in der Main-Zeile, `filter3` bei x=1136 | OUT rechts der letzten Chip-Spalte, unten — Web: elkjs, nicht C++ tidy |
| Footer | Screenshot kann noch `CPU 173%` zeigen | 0–100 via `cpuDisplayPercent` |



## Modul-Notiz (nur wo unsicher)

| Modul | Wahrheit |
|---|---|
| GraphModel / web circuit | Document + emit stay in C++. Layout/routing is elkjs + A* in `web/`. |
| CpuProtect / Footer | Anzeige soll 0–100 sein. 8× + LFO-Filter kann den Guard trotzdem trippen. |
| Tests | Zu viele Sample-`expect`. Neue Arbeit = Contracts. |
| Rest | Factory: `resources/factory_presets.json` (270 = 254 curated + 4 Club flagships + 12 PDF chains). Vocals 45 bleiben. Nie `generate_factory_presets.mjs` gegen den Shipping-Katalog. CMake/Vite binden erst beim Build. `split` expander ignores `#` / `//` comments (`# d Split:` is not a split block). |

## Build

```bash
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
```

`NeuroKore_All` / VST3 / Standalone **always** run `npm run build` first (`NeuroKoreWeb` is a hard dependency). Missing `npm` fails configure. Fresh `web/dist` is copied next to each artefact. `factory_presets.json` is a configure depend of BinaryData.

Gate 2026-08-17: **0.4.8-alpha**. Factory 258 (254 curated inkl. aller Vocals + Mesa High Gain, plus 909 Newstyle / Crisp Brick / Techno Snap / Side Scream). Artefakte: `build/NeuroKore_artefacts/Release/Standalone/NEUROKORE-0.4.8-alpha.exe`, `build/NeuroKore_artefacts/Release/VST3/NEUROKORE-0.4.8-alpha.vst3`.
