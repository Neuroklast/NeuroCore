# Entwicklungsstand NeuroCore

**Letzte Aktualisierung:** 2026-04-01  
**Version:** 0.1.0  
**Gesamtfortschritt:** ~45–55%

---

## Modul-Status

| Modul | Status | Fortschritt | Letzte Änderung |
|---|---|---|---|
| Core/PluginProcessor | ⚠️ Funktional, God-Class | 65% | 2026-04-01 |
| Core/PluginEditor | ⚠️ Fest 1600×900, nicht resizable | 55% | 2026-04-01 |
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
| UI/ParameterComponent | ✅ Funktional | 75% | 2026-04-01 |
| Preset-System | ✅ Funktional (Blowfish) | 70% | 2026-04-01 |
| Localiser | ✅ DE/EN vorhanden | 80% | 2026-04-01 |
| Licensing | ❌ Placeholder-URL | 20% | 2026-04-01 |
| Tests | ✅ DSLParser-Tests + alle Header verlinkt | 60% | 2026-04-01 |
| CI/CD | ❌ Dockerfile, kein GitHub Actions | 10% | 2026-04-01 |
| Dokumentation | ✅ Initiale Struktur erstellt | 60% | 2026-04-01 |
| Installer | ❌ Fehlt | 0% | — |
| AU-Format | ❌ Fehlt (nur VST3+Standalone) | 0% | — |

---

## Aktive Checkliste

### Nächste Schritte (Priorität)

- [ ] `SpinLock` + `juce::String` im Audio-Thread durch Lock-Free-Pattern ersetzen
- [ ] `PluginProcessor` aufteilen (God-Class auflösen)
- [ ] DC-Blocker in Signalkette einbauen
- [ ] `autoGainCompensate()` optimieren
- [ ] Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen

### Kürzlich abgeschlossen

- [x] `kEnableLicensing = false` für Dev-Builds gesetzt (Config.h)
- [x] CMake doppelte Sources gefixt (`target_sources(NeuroCore PRIVATE ...)` entfernt)
- [x] `getScript()` thread-safe gemacht (SpinLock-Guard)
- [x] `getChain()` atomic getter zu `SignalChain.h` hinzugefügt
- [x] `tests/DSLParserTest.h` erstellt (13 Tests)
- [x] `tests/SignalChainTest.h` und `tests/LookupTableSmootherTest.h` in CMake Test-Target aufgenommen
- [x] Vollständige Codebase-Analyse durchgeführt (Commit b5e76bb)
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
