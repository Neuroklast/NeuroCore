# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-05-21  
**Version:** 0.2.0  
**Gesamtfortschritt:** ~65–70%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ⚠️ God-Class, aber Undo/Redo + MIDI Learn integriert; Audio-Callback ohne chPtr-Heap-Allokation | 72% | 2026-05-21 |
| Core/PluginEditor | ✅ Resizable (600×400 – 1600×1000), Oversampling ComboBox, Undo/Redo Shortcuts | 75% | 2026-04-01 |
| Core/Config.h | ✅ Vollständig | 95% | 2026-04-01 |
| DSL/DSLParser | ✅ Funktional, Tests vorhanden | 80% | 2026-04-01 |
| DSL/SignalChain | ✅ atomic_load/store korrekt, Stage-SIMD mit externem Evaluator-Snapshot | 80% | 2026-05-21 |
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
| Preset-System | ✅ Funktional (Blowfish) + DSCR-Chunk im NRK-Format (v2) | 82% | 2026-05-21 |
| Localiser | ✅ DE/EN vorhanden | 80% | 2026-04-01 |
| Licensing | ⚠️ Async-API implementiert, Server weiterhin Placeholder | 45% | 2026-05-19 |
| Tests | ✅ DSLParser-Tests + alle Header verlinkt | 60% | 2026-04-01 |
| CI/CD | ✅ GitHub Actions ci.yml (Linux/macOS/Windows) + pluginval | 80% | 2026-04-01 |
| Dokumentation | ✅ Vollständig (docs/) | 70% | 2026-04-01 |
| Installer | ❌ Fehlt | 0% | — |
| AU-Format | ✅ Aktiviert (Standalone + VST3 + AU) | 100% | 2026-05-19 |

---

## Aktive Checkliste

### Phase 2 – Professionalität ✅

- [x] Version auf 0.2.0 erhöht
- [x] Resizable UI: `setResizable(true, true)` mit Limits 600×400 – 1600×1000
- [x] Oversampling ComboBox im UI (1x, 2x, 4x, 8x) an APVTS gebunden
- [x] MIDI Learn: `MidiLearnManager` erstellt, in `processBlock()` integriert
- [x] MIDI Learn: Rechtsklick-Menü auf ParameterComponent (Learn/Unlearn)
- [x] MIDI Learn: State-Persistence in `getStateInformation`/`setStateInformation`
- [x] Undo/Redo: `juce::UndoManager` + `FormulaChangeAction` implementiert
- [x] Undo/Redo: Keyboard Shortcuts (Ctrl+Z, Ctrl+Shift+Z, Ctrl+Y)
- [x] CI/CD: `ci.yml` mit Matrix-Build + pluginval auf allen Plattformen
- [x] CI/CD: `msbuild.yml` entfernt (redundant)
- [x] CI/CD: `scripts/install_linux_deps.sh` mit allen Dependencies
- [x] `NEEDS_MIDI_INPUT TRUE` + `EDITOR_WANTS_KEYBOARD_FOCUS TRUE` in CMakeLists.txt

### Nächste Schritte (Priorität)

- [x] Audit: `variableLock`/`variableNames` nicht im Audio-Thread verwendet
- [ ] `PluginProcessor` aufteilen (God-Class auflösen)
- [x] DC-Blocker in Signalkette einbauen
- [x] `autoGainCompensate()` optimieren
- [x] Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
- [x] AU-Format für macOS aktivieren
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
- [x] `docs/AGENTS.md` erstellt (maschinelle Agent-Guidelines)
- [x] `resources/factory_presets.json` mit Factory-Presets erstellt
- [x] `resources/templates.json` auf 15+ Kurzformeln erweitert
- [x] Vollständige Codebase-Analyse durchgeführt
- [x] Dokumentationsstruktur erstellt (`docs/` Verzeichnis)

---

## Bekannte kritische Probleme

> Diese Tabelle enthält nur aktive, noch nicht behobene kritische Issues.

| # | Problem | Datei | Priorität |
|---|---|---|---|
| 1 | God-Class PluginProcessor | `PluginProcessor.cpp` | 🟡 Hoch |
| 2 | Licensing-Backend noch Placeholder (Produktionsserver fehlt) | `Config.h` / `LicenseManager.cpp` | 🟡 Hoch |

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
