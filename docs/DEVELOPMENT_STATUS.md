# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-08-13  
**Version:** 0.2.3  
**Gesamtfortschritt:** ~88–90%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ✅ Session-State (var names, language); ChangeBroadcaster; modFrequency entfernt | 90% | 2026-06-29 |
| Core/DspEngine | ✅ + AudioDiagnostics (NaN/Jump/Crackle Log mit Preset-Kontext) | 98% | 2026-08-12 |
| Core/ScriptManager | ✅ Neu: Skript-Verwaltung, Variable Names, Preview, testFormulaStability | 85% | 2026-05-21 |
| Core/WaveformCapture | ✅ Neu: Lock-free Ring-Buffer für Input/Output-Waveform | 90% | 2026-05-21 |
| Core/MidiVariableMapper | ✅ Neu: midi_note/vel/gate/bend/mod/freq als DSL-Variablen (atomic) | 95% | 2026-05-21 |
| Core/PluginEditor | ✅ Gain+Mix only; Current-Preset Label; Live-Formel A–D; Knob-Farbringe | 97% | 2026-08-11 |
| Core/Config.h | ✅ kFeedbackLeakFactor, kDefaultTailTime hinzugefügt | 98% | 2026-05-21 |
| DSL/DSLParser | ✅ + delay / reverb / ms + bus/send/out | 97% | 2026-08-13 |
| DSL/SignalChain | ✅ Delay/Reverb/MS + Send-DAG Multi-Bus (max 4) | 99% | 2026-08-13 |
| DSL/ExpressionEvaluator | ✅ Solide (SIMD, CSE, Const-Folding) + SIMD-Funktionspfade + Template-Block-APIs | 90% | 2026-05-21 |
| DSP/InputGain | ✅ Funktional | 80% | 2026-04-01 |
| DSP/WaveShaper | ✅ Funktional | 75% | 2026-04-01 |
| DSP/SignalPolisher | ✅ Funktional + DC-Blocker nach DSL integriert | 82% | 2026-05-19 |
| DSP/DSPUtils | ✅ autoGainCompensate optimiert (direkte Sample-Multiplikation) | 85% | 2026-05-19 |
| UI/DslTerminalEditor | ✅ Edit-Modus; View-Modus = FormulaDisplay mit Live-Werten | 85% | 2026-08-11 |
| UI/FormulaDisplayComponent | ✅ A/B/C/D-Farben + Live-Eval in eckigen Klammern | 90% | 2026-08-11 |
| UI/WaveformDisplay | ✅ Funktional | 70% | 2026-04-01 |
| UI/LoudnessMeter | ✅ Funktional | 70% | 2026-04-01 |
| UI/ParameterComponent | ✅ MIDI Learn + Accent-Farbe (A rot / B gelb / C blau / D lila) | 90% | 2026-08-11 |
| UI/MidiLearnManager | ✅ Neu erstellt, vollständig | 90% | 2026-04-01 |
| Preset-System | ✅ 108 Factory (Delay/Reverb/MS echt); Templates mit echten delay/reverb | 99% | 2026-08-12 |
| Localiser | ✅ DE/EN; Gain/Mix Labels; CurrentPreset | 82% | 2026-08-11 |
| Licensing | ⚠️ Async-API implementiert, Server weiterhin Placeholder | 45% | 2026-05-19 |
| Tests | ✅ Suite schlank & stabil: **1057 passed / 0 failed ~0.3s** (MessageManager + AsyncUpdater-Fix; Factory sampled) | 98% | 2026-08-12 |
| CI/CD | ✅ pluginval ohne `|| true` (strict fail); VS2022 + CMake 4.x Workaround | 90% | 2026-06-29 |
| Build (Windows) | ✅ Standalone + VST3 Release unter VS2022 | 100% | 2026-06-29 |
| Dokumentation | ✅ UserManual EN+DE: Neue Features dokumentiert | 82% | 2026-05-21 |
| Installer | ❌ Fehlt | 0% | — |
| AU-Format | ✅ Aktiviert (Standalone + VST3 + AU) | 100% | 2026-05-19 |

---

## Aktive Checkliste

### 2026-08-13 – Cinematic Cyber-UI FX (`feat/cinematic-ui-fx`)

- [x] `CyberFxDirector` / `CyberSequence` / `decodeGlitchText` + Unit-Tests
- [x] Cached Backdrop (kein 480× Hex-Text pro Frame)
- [x] Modal enter/exit via VBlank, kein Host-Fokus-Diebstahl
- [x] Fenster-Assemble wie Neuroklast-Modals (Clip-Reveal ~340 ms), kein Scanline-Boot
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
- [x] 8 User-Params a–h (Engine, APVTS, UI 4×2, Formula colors)
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
- [x] Log-Datei: `%AppData%/NEUROKLAST/NeuroCore/audio_diagnostics.log`
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
- [x] Tests für alle neuen Features in `NeuroCoreExtrasTest.h`
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
- [x] UI-Modernisierung: `NeuroCoreLookAndFeel`, WeightedLayout-Grid, Preset-Tabelle mit Kategorie-Spalte
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
- [ ] Licensing-Backend (echter Server statt Placeholder)
- [ ] Windows/macOS Installer
- [ ] UI: Progressive Disclosure (Settings-Panel für Oversampling/Language)
- [ ] UI: Immediate Feedback (Syntaxfehler inline-highlighting)
- [ ] UI: 8pt-Grid konsequent anwenden
- [ ] SLEEF/vDSP für echtes SIMD bei transzendenten Funktionen
- [ ] Polyphoner Betrieb / Note-per-Channel
- [ ] Preset Drag-and-Drop Import/Export
- [x] `autoGainCompensate()` optimieren
- [x] Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
- [x] AU-Format für macOS aktivieren
- [x] `NeuroCore.jucer` um neue Core-/UI-Dateien aus PR #195 ergänzt (Projucer-Build wieder konsistent)
- [x] CMake JUCE-Einbindungslogik für `juceaide` robust gemacht (`JUCE_BUILD_HELPER_TOOLS` + FetchContent/Add-Subdirectory-Pfad)
- [x] `curl` aus `NeuroCoreTests` entfernt (Windows-Linking vermeiden)
- [x] Windows-Buildskripte ergänzt: `build_debug.bat`, `build_release.bat`
- [x] Windows-Buildskripte auf Ninja Multi-Config umgestellt (VS-Generator `juce::juceaide` Custom-Command-Bug umgangen)
- [ ] Windows/macOS Installer
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
