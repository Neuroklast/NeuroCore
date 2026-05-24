# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-05-24  
**Version:** 0.2.1  
**Gesamtfortschritt:** ~72–75%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ✅ God-Class aufgelöst; delegiert an DspEngine, ScriptManager, WaveformCapture | 88% | 2026-05-21 |
| Core/DspEngine | ✅ Neu: DSP-Chain, Oversampling, Gain, Dry/Wet, Buffers | 85% | 2026-05-21 |
| Core/ScriptManager | ✅ Neu: Skript-Verwaltung, Variable Names, Preview, testFormulaStability | 85% | 2026-05-21 |
| Core/WaveformCapture | ✅ Neu: Lock-free Ring-Buffer für Input/Output-Waveform | 90% | 2026-05-21 |
| Core/MidiVariableMapper | ✅ Neu: midi_note/vel/gate/bend/mod/freq als DSL-Variablen (atomic) | 95% | 2026-05-21 |
| Core/PluginEditor | ✅ Resizable (600×400 – 1600×1000), Oversampling ComboBox, Undo/Redo | 75% | 2026-04-01 |
| Core/Config.h | ✅ kFeedbackLeakFactor, kDefaultTailTime hinzugefügt | 98% | 2026-05-21 |
| DSL/DSLParser | ✅ sync, channel, ms_encode, ms_decode, trigger Parameter | 88% | 2026-05-21 |
| DSL/SignalChain | ✅ t/sr/pi Variablen, ch Var, Feedback Leak, setTempo(), setMidiVariables(), getMaxTailTime() | 88% | 2026-05-21 |
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
| Preset-System | ✅ Funktional (Blowfish) + DSCR-Chunk im NRK-Format (v2) + robustere Chunk-Validierung | 84% | 2026-05-21 |
| Localiser | ✅ DE/EN vorhanden | 80% | 2026-04-01 |
| Licensing | ⚠️ Async-API implementiert, Server weiterhin Placeholder | 45% | 2026-05-19 |
| Tests | ✅ NeuroCoreExtrasTest: MIDI-Vars, Tempo-Sync, Feedback-Leak, t/sr/pi, Channel-Routing | 72% | 2026-05-21 |
| CI/CD | ✅ GitHub Actions ci.yml (Linux/macOS/Windows) + pluginval + JUCE-Einbindungsfix für juceaide + Ninja-Windows-Buildskripte | 86% | 2026-05-24 |
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

### Nächste Schritte (Priorität)

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
