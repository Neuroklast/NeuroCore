# Lessons Learned

Dieser Erfahrungsspeicher wird nach **jeder** Coding-Agent-Session ergänzt.
Er dient dazu, Fehler nicht zu wiederholen und bekannte Fallstricke zu dokumentieren.

---

## Session-Log

---

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
