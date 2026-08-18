# Entwicklungsstand NEUROKORE

**Stand:** 2026-08-18  
**Version:** 0.4.8-alpha  
**Branch:** `ui-overhaul`

Alte Tages-Checklisten: `docs/archive/DEVELOPMENT_STATUS_HISTORY.md`.

## Jetzt

Web-UI-Umbau (Strangler): DSL + `SignalChain` bleiben der Compiler. JSON-AST ist das UI-Dokument.

| WP | Stand |
|---|---|
| WP0/WP1 AstJson + GraphOps + UI_READY-Latch + Telemetrie-Frame | **grün** — `tests/AstJsonTest.h` |
| WP2 WebView-Shell | **grün** — `tests/WebShellTest.h`. Native bleibt Default. |
| WP3 Zustand + compile/ast | **grün** — `tests/WebCompileTest.h`, `web` Vitest. |
| WP4 Hack (Monaco) | **grün** — lint ohne apply; Edit/Save; 250 ms Marker. View = muted frame + `a[0.34]` inlays + header `# name` / `# How it sounds`; Edit = accent + caret. Autocomplete compile-safe (after `type =` only enums). Save/Optimize-Apply runs `validateOnSave`. Validate overlay lists Parse/Braces/Enums/Ranges/MS/LR/Bus/Join/Jacks. Optimize empty → `Script already optimal`, no Apply. |
| WP5 Assemble | **grün** — React Flow: easy-connect, proximity-to-jack (not chip origin), pulse-edges, DnD grab/crosshair/pointer chrome, Delete/Backspace removes selected cable (`scriptAfterDisconnect` → `bus __park:` / graphOp disconnect) with formula undo, context menu, zoom-tiers, validation, L→R. ChipSpec catalog (`chipSpec` / `typeCode` / `minBodyPx`); expand no longer grows the chip. Inspect: enum `<select>` from `chipSpec.enums`, numeric clamp to `ranges`, blurb + bound knobs (unit, 2 dp). Tubes: `segmentAngle` ∈ {0,45,90}; cable kinds audio red+flow / param yellow / LFO blue; arrange `ARR_CHIP_GAP` + foreign-tube clearance. Split/Join picker: Mid/Side and L/R; `mode = split`/`join` (encode/decode aliases); MS↔LR cables rejected. Bus chip opens `bus dirt:`; Join Signal is inA/inB/out with bindable mix 0.5; emit is `bus dirt:` + `join1: mix` (no second out: mix). Send: in/out + bottom `ctrl`, `kanal` enum; Multiband Split (xover) low/mid/high; Width width/delay/bass; Octaver 5 south params; Custom `+ input` + `scriptAfterRename`; Sidechain IN only when `host.sidechainOn`. Circuit DoF: selected∪hover path sharp (`data-focus`); soft = slight blur+opacity; `motionAllows("dof")` off/prefers-reduced → no blur; no grain. |
| WP6 Chrome | **grün** — Knobs/Mix/OS/L-R, Footer 0–100, Presets, Settings, License/IR-Dialog. Inactive knob drag-bind activates from chipSpec range/unit; enum = N detents; display round2 + units. |
| WP7 Telemetrie | **grün** — NKTM-Frame, Canvas-Scope, WebGL-Gonio, kein JSON-Audio. |
| WP8 Default-Web | **grün** — Plugin öffnet WebView, wenn der Backend-Probe läuft. Sonst Native. |

Produkt-Default ist der Web-Editor. Native: `$env:NEUROKORE_WEB_EDITOR="0"`. Vite-HMR: `NEUROKORE_WEB_DEV_URL=http://localhost:5173`. Ohne WebView2: Native-Editor, kein Install-Screen (`170714`). Windows: statischer WebView2-Loader. Web-Chrome: Anthrazit + Neonrot/Gelb/Cyan, 45°-Schnitt, gestrichelter Chip-Rahmen, Sans-Titel. CRT-Scan + Chroma (kein Grain). Settings → About (Version, Formate, Kontakt). STUDIO-Chip schaltet LIVE/STUDIO. Header 52 px, Logo 44 px. Preset-Name ist der Titel (öffnet Explorer); kein Extra-Presets-Button; Pfeile bleiben. Circuit: Chip-Körper ist der Drag (kein Title-only-Handle); Details ist eine 26-px-Platte. Auto-ELK nur Erstes Laden / Preset, nicht nach Drag. Circuit: Name/Detail per Klick (kein Akkordeon). Circuit: sechs Knobs unten in einer Reihe (Buchstabe über dem Namen, nicht darübergelegt); L/BOTH/R in der Mix/OS-Zeile. LFO-Chip: Lampe pulsiert in Hz + Kurvenform. Motion: cubic-bezier, CSS-Plasma, kein Telemetrie-Tick auf den Röhren. Knob-Buchsen unten am Chip. Hover hebt die Leitung des Knobs. Knob: geschichtetes SVG (Metall-Fill, Notch-Track, Cyan-Bogen mit dashoffset + Bloom); Karte 45°-Schnitt + Mikro-Grid. Wert ist tippbar (Zahl oder 1/4). Signalleitungen: Drop-Shadow-Bloom auf der Röhre. Mausrad dreht Regler. Leitung: höchstens zwei Ecken auf freier Bahn, Diagonale statt 4-Ecken-Treppe; ELK legt die Kette links→rechts. Röhre sitzt auf der Buchsenkante (kein Gap). Chip wächst mit Sockets/Labels; Text bleibt im Body. Jede Buchse beschriftet. Terminal-Leiste: Edit, Validate, Optimize. Drei Workspaces: Unit (geschlossen, Logo+aktive Knobs; Logo-Glitch: Balken / Blitz / Pixel-Noise; logoMotion(loudness/bass/treble)→glow/chroma/jitter, Full Motion; reduced-motion/off→0), Circuit, Terminal. Validate/Optimize-Overlays. Overlay-Host: Backdrop-Blur + Clip-Path Auf-/Abbau (Full). Split-Overlays (Presets/Functions/Stages/Help): linke Ordnerleiste bleibt stehen, nur die Liste scrollt. Surface-Bloom auf Logo/Accent. Röhren-Welle mit log-Amplitude-Glow. CRT-Scan + seltener Jitter, Terminal-Scanlines. Vignette nur im Unit/Circuit/Terminal-Fenster, nicht auf HUD/Knobs/Footer. Settings → About. Footer: eine Welle + Stereofeld + Loudness, Quelle IN|OUT|BOTH, Delta, altes Kontextmenü. Fenster hält 1280:860. `src/ui/*` bleibt im Tree. `processLock` unverändert. Vite-Demo: Factory-Load schreibt AST + Knobs + Mix + `lastValidScript` (nicht nur Draft). Mix-Slider schreibt `hostStore.mix`. Footer-OS folgt dem Mix-Index (`1/2/4/8`), nicht einem toten `osFactor: 4`. Header bleibt in 1280: Preset-Titel schrumpft, BYPASS bleibt sichtbar. Auto-Arrange merkt sich die Board-IDs erst nach ELK — sonst frisst Strict Mode den ersten Paint und die Kette bleibt eine Wurst. Preset-Explorer hat ein Close, nicht zwei. Fenster-Fit snappt **abwärts**, nie größer als das Host-Fenster. Circuit-Knobs: Buchstabe sitzt über dem Namen (flow), nicht auf dem Titel.

## Offen

1. Native `src/ui` löschen erst, wenn Circuit-Contracts auf elk/AST umgezogen sind.
2. Per-Block-DSP-Welle (heute IN/OUT-Telemetrie, Glow log-Amplitude).

Delay liest linear mit Integer-Wrap (`delayRead`). Hermite-4-Punkt hat jedes Delay-Intervall Index 0 mit `N-1` gemischt — das war das periodische Knacken.

Arrange stapelt eine lange Kette spaltenweise (wie Fender Clean im Browser), nicht in eine einzige Zeile. AST-JSON zeichnet MS-Gabeln und implizites OUT. Jeder Chip klappt Details auf (Schema-Zeilen, nicht nur Drive). Rechtsklick aufs Board öffnet den Add-Picker (Kategorien links, Blöcke rechts, kein verschachteltes Flyout). Custom ist `y =` plus extra Eingänge. Ctrl+Z / Ctrl+Y ist Formel-Undo. Circuit-Knobs: Bind-Buchse oben, keine A–F-Buchstaben auf dem Titel. Zählzeiten nur bei `param … [1/4, 1/16]`, nie bei numerischen Ranges. Ungetaggtes Chip zwischen MS-Encode/Decode liegt auf **beiden** Schienen (Screenshot 072440); `connectJack` von Encode.`side` schreibt `channel=side` und Mid läuft implizit Encode→Decode. Chip-Details-Platte sitzt außerhalb des Body-Clips. User-xy bleibt bis Arrange / neues Graph.

## Native Circuit (Referenz, nicht mehr der Editor-Default)

Quelle: `screenshots/Screenshot 2026-08-16 231501.png` (Phaser Lab), `231939.png` (Settings).

| Fläche | Ist | Soll |
|---|---|---|
| Circuit tidy | Soft-Contract: `OUT(464,16)` in der Main-Zeile, `filter3` bei x=1136 | OUT rechts der letzten Chip-Spalte, unten — Web: elkjs, nicht C++ tidy |
| Footer | Screenshot kann noch `CPU 173%` zeigen | 0–100 via `cpuDisplayPercent` |



## Modul-Notiz (nur wo unsicher)

| Modul | Wahrheit |
|---|---|
| GraphModel / GraphCanvas / PcbRouter | Orthogonal-Router existiert. Auto-Arrange **erfüllt den Screenshot-Vertrag nicht**. |
| CpuProtect / Footer | Anzeige soll 0–100 sein. 8× + LFO-Filter kann den Guard trotzdem trippen. |
| Tests | Zu viele Sample-`expect`. Neue Arbeit = Contracts. |
| Rest | Factory: `resources/factory_presets.json` (254, curated) im Repo; CMake/Vite binden erst beim Build. License, OS-Bänke unverändert. |

## Build

```bash
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
```

Gate 2026-08-17: **0.4.8-alpha**. Factory curated 254 (alle Vocals + Mesa High Gain; keine unechten Markennamen). Artefakte: `build/NeuroKore_artefacts/Release/Standalone/NEUROKORE-0.4.8-alpha.exe`, `build/NeuroKore_artefacts/Release/VST3/NEUROKORE.vst3`. Web-UI kommt zur Laufzeit aus `web/dist`.
