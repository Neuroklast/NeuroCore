# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-04-01  
**Version:** 0.2.0  
**Gesamtfortschritt:** ~65–70%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ⚠️ God-Class, aber Undo/Redo + MIDI Learn integriert | 70% | 2026-04-01 |
| Core/PluginEditor | ✅ Resizable (600×400 – 1600×1000), Oversampling ComboBox, Undo/Redo Shortcuts | 75% | 2026-04-01 |
| Core/Config.h | ✅ Vollständig | 95% | 2026-04-01 |
| DSL/DSLParser | ✅ Funktional, Tests vorhanden | 80% | 2026-04-01 |
| DSL/SignalChain | ✅ atomic_load/store korrekt | 75% | 2026-04-01 |
| DSL/ExpressionEvaluator | ✅ Solide (SIMD, CSE, Const-Folding) | 85% | 2026-04-01 |
| DSP/InputGain | ✅ Funktional | 80% | 2026-04-01 |
| DSP/WaveShaper | ✅ Funktional | 75% | 2026-04-01 |
| DSP/SignalPolisher | ✅ Funktional | 75% | 2026-04-01 |
| DSP/DSPUtils | ⚠️ autoGainCompensate ineffizient | 65% | 2026-04-01 |
| UI/DslTerminalEditor | ✅ Funktional | 70% | 2026-04-01 |
| UI/WaveformDisplay | ✅ Funktional | 70% | 2026-04-01 |
| UI/LoudnessMeter | ✅ Funktional | 70% | 2026-04-01 |
| UI/ParameterComponent | ✅ MIDI Learn Rechtsklick-Menü | 85% | 2026-04-01 |
| UI/MidiLearnManager | ✅ Neu erstellt, vollständig | 90% | 2026-04-01 |
| Preset-System | ✅ Funktional (Blowfish) | 70% | 2026-04-01 |
| Localiser | ✅ DE/EN vorhanden | 80% | 2026-04-01 |
| Licensing | ❌ Placeholder-URL, für Dev deaktiviert | 20% | 2026-04-01 |
| Tests | ✅ DSLParser-Tests + alle Header verlinkt | 60% | 2026-04-01 |
| CI/CD | ✅ GitHub Actions ci.yml (Linux/macOS/Windows) + pluginval | 80% | 2026-04-01 |
| Dokumentation | ✅ Vollständig (docs/) | 70% | 2026-04-01 |
| Installer | ❌ Fehlt | 0% | — |
| AU-Format | ❌ Fehlt (nur VST3+Standalone) | 0% | — |

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

- [ ] `SpinLock` + `juce::String` im Audio-Thread durch Lock-Free-Pattern ersetzen
- [ ] `PluginProcessor` aufteilen (God-Class auflösen)
- [ ] DC-Blocker in Signalkette einbauen
- [ ] `autoGainCompensate()` optimieren
- [ ] Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
- [ ] AU-Format für macOS aktivieren
- [ ] Windows/macOS Installer

### Kürzlich abgeschlossen

- [x] Phase 2 Features implementiert (Resizable UI, MIDI Learn, Undo/Redo, Oversampling ComboBox)
- [x] CI/CD Pipeline mit pluginval
- [x] `kEnableLicensing = false` für Dev-Builds gesetzt (Config.h)
- [x] CMake doppelte Sources gefixt
- [x] `getScript()` thread-safe gemacht (SpinLock-Guard)
- [x] `getChain()` atomic getter zu `SignalChain.h` hinzugefügt
- [x] `tests/DSLParserTest.h` erstellt (13 Tests)
- [x] `tests/SignalChainTest.h` und `tests/LookupTableSmootherTest.h` in CMake Test-Target aufgenommen
- [x] Vollständige Codebase-Analyse durchgeführt
- [x] Dokumentationsstruktur erstellt (`docs/` Verzeichnis)

---

## Bekannte kritische Probleme

> Diese Tabelle enthält nur aktive, noch nicht behobene kritische Issues.

| # | Problem | Datei | Priorität |
|---|---|---|---|
| 1 | Blockierender HTTP-Call im UI-Thread | `LicenseManager.cpp` | 🔴 Kritisch |
| 2 | SpinLock + String-Heap-Allokation im Audio-Thread | `PluginProcessor.h` | 🔴 Kritisch |
| 3 | God-Class PluginProcessor | `PluginProcessor.cpp` | 🟡 Hoch |
| 4 | `autoGainCompensate` per-Sample | `DSPUtils.h` | 🟡 Hoch |

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
