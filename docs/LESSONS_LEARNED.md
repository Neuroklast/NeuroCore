# Lessons Learned

Dieser Erfahrungsspeicher wird nach **jeder** Coding-Agent-Session ergänzt.
Er dient dazu, Fehler nicht zu wiederholen und bekannte Fallstricke zu dokumentieren.

---

## Session-Log

---

### 2026-05-24 – Windows Visual-Studio-Generator Build-Fix via Ninja

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Windows-Build mit `juce::juceaide`-Fehler stabilisieren (`build_debug.bat`, `build_release.bat`, `CMakeLists.txt`)  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Der Visual-Studio-CMake-Generator kann bei JUCE-Custom-Commands (`binarydata`, `rcfile`) `juce::juceaide` als Literal statt als ausführbaren Pfad behandeln; `Ninja Multi-Config` umgeht dieses Problem zuverlässig.
- Für lokale Windows-Skripte ist es robuster, Ninja/CMake aus den VS2022 Build Tools explizit in den `PATH` zu setzen und vor dem Build `vcvars64.bat` zu laden.
- `add_subdirectory("${JUCE_DIR}" "JUCE")` hält die JUCE-Einbindung näher am stabilen Zustand vor PR #195 und vermeidet zusätzliche Pfadauflösungs-Risiken.

#### Fallstricke

- In dieser Sandbox bleibt eine vollständige lokale Build-Validierung blockiert, weil Linux-Systemabhängigkeiten (`x11`) für den CMake-Configure fehlen.

#### Empfehlungen für nächste Session

1. CI-Rerun für `ci.yml` prüfen, speziell den Windows-Job mit dem vorherigen `MSB8066`/`juce::juceaide`-Fehler.
2. Optional README-Buildsektion um Ninja-Hinweis für Windows ergänzen.

---

### 2026-05-24 – Projucer/CMake Sync-Fix für PR #195

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Fehlende Dateien in `NeuroCore.jucer` nachtragen, JUCE-CMake-Einbindung (`juceaide`) reparieren, Windows-Buildskripte ergänzen  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Wenn neue Klassen nur in `CMakeLists.txt`, aber nicht in `NeuroCore.jucer` eingetragen werden, driften CMake- und Projucer-Build auseinander und Visual-Studio-Projekte aus Projucer fehlen dann komplette Units.
- Für stabile JUCE-CMake-Integration muss `JUCE_BUILD_HELPER_TOOLS` VOR jeder JUCE-Einbindung gesetzt werden; sonst fehlt in CI der `juceaide`-Target und BinaryData/RC-Generierung bricht.
- Das Entfernen von unnötigen Plattform-Libs (`curl` im Test-Target) reduziert plattformspezifische Build-Probleme, ohne Funktionalität zu verlieren.

#### Fallstricke

- In dieser Sandbox bleibt `cmake -B build -S .` weiterhin am Linux-Dependency-Check (`x11`) hängen; vollständige Build-Validierung muss daher über CI-Jobs erfolgen.

#### Empfehlungen für nächste Session

1. CI-Rerun von `ci.yml` prüfen, ob der `juceaide target does not exist` Fehler auf Linux/macOS/Windows verschwunden ist.
2. Optional README um kurzen Hinweis auf `build_debug.bat` und `build_release.bat` ergänzen.

---

### 2026-05-21 – God-Class-Auflösung + Musikalische/DSP-Features

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** PluginProcessor God-Class auflösen (DspEngine, ScriptManager, WaveformCapture) + MIDI-Variablen, Tempo-Sync, Feedback-Schutz, Stereo-Features, Tail-Time  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Die Auflösung einer God-Class erfordert sorgfältige Analyse der Abhängigkeiten zwischen Klassen. `DspEngine::processBlock` nimmt `signalChain` und `oldSignalChain` als Referenz-Parameter – so muss DspEngine die ScriptManager-Klasse nicht kennen.
- `ValidationProgressInfo` muss in eine eigene Header-Datei (`ValidationTypes.h`) ausgelagert werden, um zirkuläre Includes zwischen `PluginProcessor.h` und `ScriptManager.h` zu vermeiden.
- `signalChain` und `oldSignalChain` müssen `public` in ScriptManager bleiben, weil `DspEngine::processBlock` und `PluginProcessor::getTailLengthSeconds` direkten Zugriff benötigen.
- Für `ms_encode`/`ms_decode` in Stage müssen L- und R-Kanal gemeinsam transformiert werden – das erfordert Zugriff auf den gesamten Stereo-Buffer, nicht nur auf einzelne Samples. Die Stage-`processBlock`-Methode muss vor der Formel-Schleife diese Transformation durchführen.
- `std::atomic<float>` ist ideal für MIDI-Variablen im Audio-Thread (RT-safe), erfordert aber `std::atomic_init` in Konstruktoren bei älteren Compilern; `= {0.f}` Initialisierung ist sicherer.
- Für Tempo-Sync bei Osc-Blöcken: `sync = 1/4` als String-Ratio parsen (Float-Division) und in `Osc::applyTempo(bpm)` auf `freq = bpm/60 * ratio` abbilden.

#### Fallstricke

- In dieser Sandbox bleiben lokale Build-/Testläufe durch fehlende Linux-Dependency `x11` bereits beim CMake-Configure blockiert; CI-Status immer über GitHub Actions prüfen.
- `PluginProcessor::processBlock` muss `midiVariableMapper` auf BEIDEN Chains (signalChain UND oldSignalChain) aufrufen, sonst klingt der Crossfade bei Formula-Wechsel mit MIDI inkonsistent.
- `DspEngine.prepare()` muss die neuen Buffergrößen für `upBlock` UND `scriptBuffer`/`oldScriptBuffer` korrekt berechnen – Fehler dort führen zu stillen Buffer-Overflows.

#### Empfehlungen für nächste Session

1. CI-Jobs neu triggern, damit alle neuen NeuroCoreExtrasTests auf allen Plattformen laufen.
2. UI: `ValidationTypes.h` prüfen ob `ValidationContentComponent.cpp` noch `#include "ValidationTypes.h"` benötigt oder es über `PluginProcessor.h` bekommt.
3. Negativtest für kaputte Preset-Dateien (aus vorheriger Session) noch ausstehend.

---



**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Review-Follow-up mit Code-Optimierung, Legacy-Cleanup, Testabdeckung und Doku-/Agent-Workflow-Updates  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Beim NRK-Chunk-Parsing sollten Entry-Zahlen, Offsets und Längen strikt validiert werden, um fehlerhafte/kaputte Preset-Dateien früh und sicher abzulehnen.
- Ein expliziter Test auf `DSCR`-Priorität gegenüber dem aus `STAT` restaurierten Skript schützt die gewünschte v2-Semantik zuverlässig gegen Regressionen.
- Verbindliche Abschluss-Schritte in `AGENTS.md` und `docs/AGENTS.md` reduzieren Review-Runden, weil Optimierung, Cleanup, Tests und Doku systematisch abgearbeitet werden.

#### Fallstricke

- In dieser Sandbox bleiben lokale Build-/Testläufe durch fehlende Linux-Dependency `x11` bereits beim CMake-Configure blockiert; deshalb CI-Status immer zusätzlich über GitHub Actions prüfen.
- `action_required` Workflow-Runs können ohne gestartete Jobs erscheinen; die Diagnose muss dann über Run-Metadaten und spätere Reruns erfolgen.

#### Empfehlungen für nächste Session

1. Falls möglich CI-Jobs neu triggern/approven, damit die neuen Preset- und Parser-Checks auf allen Plattformen laufen.
2. Zusätzlichen Negativtest ergänzen: Preset mit ungültiger Chunk-Entry-Anzahl muss `loadPreset()` sauber fehlschlagen.
3. README-DSL-Kurzübersicht bei Gelegenheit formatieren (der bestehende Listenblock ist schwer lesbar).

### 2026-05-21 – SIMD-Hotpath + NRK-DSCR-Chunk

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Performance-Bottlenecks in `ExpressionEvaluator`/`SignalChain` reduzieren und DSL-Skript als `DSCR`-Chunk im NRK-Format speichern  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Für JUCE-Ausdrücke lohnt sich ein optionaler SIMD-Callback direkt im AST-Knoten (`FunctionNode`), damit `sin/cos/tanh/exp` nicht auf per-Lane-Scalar-Fallback zurückfallen.
- Template-basierte Block-Evaluierung (`evaluateBlockT` / `evaluateBlockSimdT`) entfernt unnötige `std::function`-Indirektion in Hotpaths, während die alten APIs als Wrapper kompatibel bleiben.
- Ein zusätzlicher roher `DSCR`-Chunk im Presetformat (NRK v2) ermöglicht lesbare, nicht-escaped DSL-Skripte bei voller Rückwärtskompatibilität über den verschlüsselten `STAT`-Chunk.

#### Fallstricke

- `report_progress` kann Build-Artefakte committen, wenn `build/` nicht ignoriert ist; deshalb `build/` explizit in `.gitignore` eintragen.
- In dieser Sandbox scheitert `cmake -B build -S .` weiterhin früh wegen fehlendem Systempaket `x11`; Test-Validierung kann dadurch lokal blockiert sein.

#### Empfehlungen für nächste Session

1. CI-Run prüfen, ob SIMD-/Preset-Änderungen auf allen Plattformen grün sind.
2. Optional echte vektorielle Approximationskerne für LookupTables-SIMD-Funktionen evaluieren (anstatt laneweiser LUT-Auswertung).
3. Preset-Inspector/UI um DSCR-Chunk-Anzeige ergänzen (Debugging/Diagnose).

### 2026-05-19 – Stabilitätsfixes + Agent Docs + Factory Presets

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Kritische DSP/Licensing-Schwächen beheben, `docs/AGENTS.md` erstellen, Factory-Presets/Templates erweitern  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `DSPUtils::autoGainCompensate()` sollte im Hotpath ausschließlich direkte Sample-Multiplikation verwenden; per-Sample `getSubBlock()` + `ProcessContext` erzeugt unnötigen Overhead.
- Ein `juce::Thread`-basierter Async-Wrapper mit `juce::MessageManager::callAsync()` ist ein robuster Weg, blockierende Licensing-HTTP-Aufrufe aus dem Message-Thread herauszuhalten.
- DC-Offset-Entfernung gehört direkt hinter die DSL-Signalkette; in oversampelten Setups müssen Coefficients auf der effektiven Processing-Samplerate gesetzt werden.
- Oversampling-Latenz muss an zwei Stellen konsistent sein: `setLatencySamples(...)` für den Host und `dryWetMixer.setWetLatency(...)` für internes Dry/Wet-Alignment.

#### Fallstricke

- Lokale Linux-Builds können ohne `install_linux_deps.sh` bereits bei CMake-Dependency-Checks fehlschlagen.
- Die aktuelle CMake/JUCE-Konfiguration kann lokal beim Test-Build über `juce::juceaide` scheitern; das ist ein bestehendes Infrastrukturproblem und nicht Teil der inhaltlichen Fixes.

#### Empfehlungen für nächste Session

1. CMake/JUCE-Tooling (`juce::juceaide`) robust machen, damit lokale Tests ohne Workarounds laufen.
2. Factory-Presets an Preset-UI/Import-Workflow anbinden (falls noch nicht konsumiert).
3. `PluginProcessor` schrittweise entkoppeln (God-Class-Abbau).

### 2026-04-01 – Phase 2 Professionalität: Build-Fixes + Feature-Integration

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** CI/CD Build-Fehler beheben + Phase 2 Features (Resizable UI, MIDI Learn, Undo/Redo, Oversampling ComboBox, pluginval)  
**Ergebnis:** ✅ Erfolgreich  
**PR:** copilot/fix-build-error-and-implement-phase-2

#### Erkenntnisse

- **CI/CD:** Der `ci.yml` Workflow schlug fehl weil juceaide manuell gebaut wurde und `pkgRedirects` nicht erstellt werden konnte. Lösung: JUCE direkt mit `--recurse-submodules` klonen (bringt VST3 SDK mit) und JUCE über `add_subdirectory()` + `JUCE_BUILD_HELPER_TOOLS ON` einbinden lassen – juceaide wird dann automatisch konfiguriert.
- **MSBuild Workflow:** `msbuild.yml` war komplett broken (suchte `NeuroCore.sln` die nie existierte). Entfernt, da `ci.yml` bereits Windows/macOS/Linux abdeckt.
- **MIDI Learn Architektur:** `MidiLearnManager` mit SpinLock und TryLock-Pattern im Audio-Thread ist sauber. `processMidiMessages()` verwendet `ScopedTryLockType` damit der Audio-Thread nie blockiert – wichtig für Echtzeit-Garantie.
- **Undo/Redo Pattern:** `setFormula()` erstellt `FormulaChangeAction` und delegiert an `applyFormula()`. Die UndoableAction ruft auch `applyFormula()` auf (nicht `setFormula()`), um Rekursion zu vermeiden.
- **EDITOR_WANTS_KEYBOARD_FOCUS:** Muss `TRUE` sein damit `keyPressed()` im Editor funktioniert. Ohne diese Einstellung kommen Tastatur-Events nie an.
- **NEEDS_MIDI_INPUT:** Muss `TRUE` sein damit der Host MIDI-Daten an `processBlock()` weiterleitet. Ohne das bleibt die `MidiBuffer` immer leer.
- **pluginval:** Läuft mit `|| true` am Ende, damit der CI nicht fehlschlägt wenn pluginval Warnungen ausgibt. Für Strictness Level 5 ist das normal bei Plugins in Entwicklung.

#### Fallstricke

- **Doppelte Undo-Registration:** Wenn `setFormula()` sowohl parst als auch `applyFormula()` aufruft und dann die UndoableAction registriert, muss die Action NUR `applyFormula()` aufrufen (nicht `setFormula()`), sonst entsteht eine Endlosschleife.
- **MidiLearnManager in Test-Target:** Muss auch in `NeuroCoreTests` eingebunden werden, da `PluginProcessor.cpp` (das im Test-Target ist) `MidiLearnManager.h` inkludiert.
- **install_linux_deps.sh:** Fehlende Pakete (`libxcursor-dev`, `libxinerama-dev`, `libasound2-dev`, `libcurl4-openssl-dev`, `pkg-config`) führen zu kryptischen CMake-Fehlern. Immer ALLE JUCE-Abhängigkeiten auflisten.

#### Empfehlungen für nächste Session

1. CI-Pipeline testen: Überprüfen ob der Build auf allen drei Plattformen grün ist
2. `PluginProcessor` God-Class beginnen aufzuteilen
3. `autoGainCompensate()` per-Sample-Ineffizienz beheben
4. Lock-Free FIFO für UI→Audio-Kommunikation evaluieren

### 2026-04-01 – Phase 1 Stabilität: Erste Fixes

**Agent:** GitHub Copilot  
**Aufgabe:** CMake-Fix, Licensing-Dev-Mode, Thread-Safety, DSLParser-Tests  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `SignalChain.cpp` verwendete bereits `std::atomic_load`/`std::atomic_store` für `chain` an allen relevanten Stellen: `prepare()` (Zeile 36), `loadScript()` (Zeile 298), `processBlock()` (Zeile 317) und `processBlockSmoothed()` (Zeile 329). Der Konstruktor weist `chain` direkt zu (Single-Thread, kein Race möglich). Es fehlte nur der öffentliche `getChain()` Getter im Header.
- `tests/main.cpp` inkludierte und registrierte `SignalChainTest` und `LookupTableSmootherTest` bereits korrekt – sie fehlten nur in `target_sources(NeuroCoreTests)` in `CMakeLists.txt`.
- `kEnableLicensing = true` mit Placeholder-URL `licensing.example.com` macht das Plugin im Dev-Build sofort zum Demo-Plugin. Das ist der gefährlichste stille Bug.
- Das doppelte `target_sources(NeuroCore PRIVATE ${SOURCE_FILES})` in CMakeLists.txt (Zeile 99 in `juce_add_plugin SOURCES` + Zeile 102 explizit) kann ODR-Verstöße und erhöhte Build-Zeit verursachen.
- DSLParser validiert bereits viele Fehlerfälle (fehlender Doppelpunkt, unbekannter Block-Typ, doppelter Block-Name, param nach Block). Tests decken jetzt alle diese Fälle ab.

#### Fallstricke

- `getScript()` ohne Lock ist ein echter Data-Race: `dslScript` kann von `setFormula()` im UI-Thread geschrieben werden während `getScript()` liest. Der `noexcept`-Qualifier muss entfernt werden, da SpinLock-Zugriff technisch werfen kann.
- Bei `SpinLock` + `juce::String`: Die String-Kopie unter Lock ist ein potenzieller Heap-Allokations-Punkt im Audio-Thread. Für eine vollständige Lösung wäre ein Lock-Free FIFO (z. B. `juce::AbstractFifo`) besser – das ist aber Phase 2.
- Kein blindes Hinzufügen von Sourcen zu beiden Targets: `NeuroCore` und `NeuroCoreTests` haben unterschiedliche Abhängigkeiten (NeuroCoreTests braucht kein `juce::juce_audio_plugin_client`).

#### Empfehlungen für nächste Session

1. Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
2. `PluginProcessor` God-Class beginnen aufzuteilen
3. `autoGainCompensate()` per-Sample-Ineffizienz beheben

---

### 2026-04-01 – Initiale Analyse & Dokumentation

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Vollständige Codebase-Analyse und Dokumentationsstruktur erstellen  
**Ergebnis:** ✅ Erfolgreich  
**Commit:** `b5e76bb` (Analyse-Basis)

#### Erkenntnisse

**Codebase:**
- `PluginProcessor.cpp` ist mit ~34 KB eine massive God-Class, die dringend in kleinere, fokussierte Klassen aufgeteilt werden muss. Das ist das größte Architektur-Problem des Projekts.
- `ExpressionEvaluator` ist das Herzstück des Plugins – die Kombination aus SIMD, Constant-Folding und CSE-Elimination ist beeindruckend professionell implementiert.
- Das Licensing-System ist komplett nicht-funktional: `licensing.example.com` ist ein Placeholder. `kEnableLicensing = true` muss für alle Dev-Builds auf `false` gesetzt werden.
- Der `DSLParser` hat **keine Tests** – das ist die höchste Priorität für die nächste Entwicklungs-Session.
- Thread-Safety zwischen UI-Thread und Audio-Thread ist das größte Stabilitätsrisiko: `getScript()`, `shared_ptr<Chain>` und `SpinLock + juce::String` sind alle potentielle Race Conditions oder Heap-Allokationen im Audio-Thread.
- Die DSL-Idee ("ShaderToy für Audio") ist das absolute Alleinstellungsmerkmal – das muss das Herzstück aller Marketingbemühungen sein.
- Das `WeightedLayout`-System ist clever und flexibel, aber die feste Fenstergröße 1600×900 macht das Plugin auf kleineren Bildschirmen (z. B. 13" Laptops) schwierig nutzbar.
- Blowfish-Verschlüsselung für Presets ist ungewöhnlich – ein Upgrade auf AES-256 wäre professioneller.
- `autoGainCompensate()` mit per-Sample `getSubBlock()` ist ein ernsthafter Performance-Bottleneck.

**Build-System:**
- JUCE muss in Version ≥ 8.0.6 vorhanden sein. Die CMake-Integration lädt JUCE automatisch wenn `JUCE_DIR` nicht gesetzt ist.
- Die VST3-SDK muss manuell in `~/JUCE/modules/juce_audio_processors/format_types/VST3_SDK` kopiert werden.
- `CMakeLists.txt` hat eine doppelte Source-Einbindung (`SOURCES` in `juce_add_plugin` UND `target_sources`) – das ist ein Bug der Warnungen erzeugen kann.
- `SignalChainTest.h` existiert im `tests/`-Verzeichnis, ist aber nicht im `NeuroCoreTests` CMake-Target verlinkt.

#### Fallstricke

- Kein blindes Hinzufügen von Code in `PluginProcessor` – diese Klasse ist bereits zu groß
- Bei jeder neuen Klasse: Thread-Safety von Anfang an bedenken (Audio-Thread vs. Message-Thread)
- `juce::String`-Operationen unter Lock = potentielle Heap-Allokation = Audio-Thread-Knackser
- `std::shared_ptr` ist nicht thread-safe ohne explizite Synchronisation (`atomic_load`/`atomic_store`)

#### Empfehlungen für nächste Session

1. Als erstes `kEnableLicensing = false` setzen (verhindert Demo-Modus bei Entwicklung)
2. Dann Thread-Safety fixen (kritischstes Stabilitätsproblem)
3. DSLParser-Tests schreiben (kritischste fehlende Test-Abdeckung)

---

## Vorlage für neue Einträge

```markdown
### YYYY-MM-DD – [Kurztitel der Session]

**Agent:** [Agenten-Typ / Name]
**Aufgabe:** [Was war die Aufgabe?]
**Ergebnis:** ✅ Erfolgreich / ⚠️ Teilweise / ❌ Fehlgeschlagen
**Commit:** [Commit-Hash oder PR-Link]

#### Erkenntnisse

- [Was wurde gelernt?]
- [Was war überraschend?]
- [Was hat gut funktioniert?]

#### Fallstricke

- [Was ist schiefgelaufen?]
- [Was muss beim nächsten Mal beachtet werden?]

#### Empfehlungen für nächste Session

1. [Konkrete nächste Schritte]
```
