# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-08-11  
**Version:** 0.2.1  
**Gesamtfortschritt:** ~78–80%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ✅ Session-State (var names, language); ChangeBroadcaster; modFrequency entfernt | 90% | 2026-06-29 |
| Core/DspEngine | ✅ LPF osSpec; kein RT Chain-Copy; Signalkette dokumentiert | 90% | 2026-06-29 |
| Core/ScriptManager | ✅ Neu: Skript-Verwaltung, Variable Names, Preview, testFormulaStability | 85% | 2026-05-21 |
| Core/WaveformCapture | ✅ Neu: Lock-free Ring-Buffer für Input/Output-Waveform | 90% | 2026-05-21 |
| Core/MidiVariableMapper | ✅ Neu: midi_note/vel/gate/bend/mod/freq als DSL-Variablen (atomic) | 95% | 2026-05-21 |
| Core/PluginEditor | ✅ Layout/Knobs/Mix-Slider UX-Fix, größerer Preset-Browser, Double-Click Load | 94% | 2026-08-11 |
| Core/Config.h | ✅ kFeedbackLeakFactor, kDefaultTailTime hinzugefügt | 98% | 2026-05-21 |
| DSL/DSLParser | ✅ param-Ranges `[min,max]` robust; sync/channel/ms/trigger | 92% | 2026-08-11 |
| DSL/SignalChain | ✅ Param-Map in Stages; Osc-Freq-Formeln; Filter `+`/`*` Modulation | 96% | 2026-08-11 |
| DSL/ExpressionEvaluator | ✅ Solide (SIMD, CSE, Const-Folding) + SIMD-Funktionspfade + Template-Block-APIs | 90% | 2026-05-21 |
| DSP/InputGain | ✅ Funktional | 80% | 2026-04-01 |
| DSP/WaveShaper | ✅ Funktional | 75% | 2026-04-01 |
| DSP/SignalPolisher | ✅ Funktional + DC-Blocker nach DSL integriert | 82% | 2026-05-19 |
| DSP/DSPUtils | ✅ autoGainCompensate optimiert (direkte Sample-Multiplikation) | 85% | 2026-05-19 |
| UI/DslTerminalEditor | ✅ Funktional | 70% | 2026-04-01 |
| UI/WaveformDisplay | ✅ Funktional | 70% | 2026-04-01 |
| UI/LoudnessMeter | ✅ Funktional | 70% | 2026-04-01 |
| UI/ParameterComponent | ✅ MIDI Learn Rechtsklick-Menü | 85% | 2026-04-01 |
| UI/MidiLearnManager | ✅ Neu erstellt, vollständig | 90% | 2026-04-01 |
| Preset-System | ✅ NRK v2 + 75 ladbare Factory-Presets (`FactoryPresetLibrary`, alle unit-getestet) | 95% | 2026-08-11 |
| Localiser | ✅ DE/EN vorhanden | 80% | 2026-04-01 |
| Licensing | ⚠️ Async-API implementiert, Server weiterhin Placeholder | 45% | 2026-05-19 |
| Tests | ✅ 2108 Tests grün inkl. Apply aller 75 Factory-Presets | 90% | 2026-08-11 |
| CI/CD | ✅ pluginval ohne `|| true` (strict fail); VS2022 + CMake 4.x Workaround | 90% | 2026-06-29 |
| Build (Windows) | ✅ Standalone + VST3 Release unter VS2022 | 100% | 2026-06-29 |
| Dokumentation | ✅ UserManual EN+DE: Neue Features dokumentiert | 82% | 2026-05-21 |
| Installer | ❌ Fehlt | 0% | — |
| AU-Format | ✅ Aktiviert (Standalone + VST3 + AU) | 100% | 2026-05-19 |

---

## Aktive Checkliste

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
- [x] 75 Factory-Presets in 16 Kategorien — alle `loadScript` + `applyPreset` grün
- [x] DSL: param-Ranges, Stage-Map, Osc-Freq-Formeln, Filter `+`/`*` Modulation
- [x] UI: größere Knobs, längere Mix/Gain-Slider, größerer Preset-Browser, Double-Click Load
- [x] Unit-Test deckt alle Factory-Presets ab

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
