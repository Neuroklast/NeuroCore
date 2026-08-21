# NeuroKore – Code-Analyse (historisch)

> Snapshot from 2026-04-01. Daily brief: `Agents.md` + `docs/agents/`. Copy: `docs/archive/ANALYSIS_2026-04.md`.

**Analyse-Datum:** 2026-04-01  
**Basis-Commit:** `b5e76bb`  
**Analysiert von:** GitHub Copilot Coding Agent

---

## Zusammenfassung

NeuroKore ist ein innovatives Audio-Plugin mit einem einzigartigen DSL-basierten Ansatz für Echtzeit-Signalverarbeitung. Die Kernidee ist solide und das Plugin ist funktionsfähig. Es fehlen jedoch wichtige professionelle Qualitätsmerkmale, die vor einem Produktions-Release behoben werden müssen.

**Gesamtbewertung:** ~40–50% eines professionellen Gold-Standard-Plugins.

---

## 1. Kritische Bugs

### Bug 1: Licensing-Server ist ein Placeholder
**Status:** behoben 2026-08-13. Offline RSA-`.lic` + `NeuroKoreIssuer`. Kein Activation-Server mehr.

---

### Bug 2: Doppelte Source-Einbindung in CMakeLists.txt
**Datei:** `CMakeLists.txt`  
**Zeile:** ~99–102

```cmake
juce_add_plugin(NeuroKore
    SOURCES ${SOURCE_FILES}
)

target_sources(NeuroKore PRIVATE ${SOURCE_FILES})
```

**Problem:** `SOURCES` in `juce_add_plugin` und nochmals `target_sources` kompilieren dieselben Dateien zweimal. Das erhöht die Build-Zeit und kann ODR-Verstöße (One Definition Rule) verursachen.

**Lösung:** Eine der beiden Source-Einbindungen entfernen.

---

### Bug 3: Blockierender HTTP-Call im Message-Thread
**Datei:** `src/licensing/LicenseManager.cpp`  
**Zeile:** ~34

```cpp
auto response = url.readEntireTextStream();
```

**Problem:** `readEntireTextStream()` blockiert den aufrufenden Thread. Wenn `activateOnline()` oder `deactivateLicense()` aus dem Message-Thread (UI-Thread) aufgerufen wird, friert die gesamte UI ein, bis der HTTP-Request abgeschlossen ist oder ein Timeout eintritt.

**Lösung:** `juce::URL::downloadToFile()` mit Callback oder `std::async` / `juce::Thread`-Subklasse verwenden.

---

### Bug 4: SpinLock + String-Operationen im Audio-Thread
**Datei:** `src/core/PluginProcessor.h`  
**Zeile:** ~138

```cpp
mutable juce::SpinLock variableLock;
```

**Problem:** `juce::String`-Operationen (Kopie, Zuweisung) können Heap-Allokationen auslösen. Wenn `variableLock` einen `juce::String` schützt und aus dem Audio-Thread gehalten wird, kann das zu Heap-Allokationen im Audio-Thread führen → **Knackser und Aussetzer**.

**Lösung:** `std::atomic<int>` für Zahlen, `juce::AbstractFifo` + char-Array-Buffer für Strings im Audio-Thread.

---

### Bug 5: `getScript()` nicht thread-safe
**Datei:** `src/core/PluginProcessor.h`  
**Zeile:** ~89

```cpp
juce::String getScript() const noexcept { return dslScript; }
```

**Problem:** `dslScript` wird ohne Lock gelesen, während `setFormula()` aus einem anderen Thread schreiben kann. Das ist eine klassische Data-Race-Situation (undefiniertes Verhalten in C++).

**Lösung:** Lock beim Lesen und Schreiben von `dslScript` verwenden, oder `std::atomic`-basiertes Shared-Pointer-Pattern.

---

### Bug 6: `shared_ptr<Chain>` ohne `atomic_load` → Race Condition
**Datei:** `src/dsl/SignalChain.h`  
**Zeile:** ~136

```cpp
std::shared_ptr<Chain> chain;
```

**Problem:** `std::shared_ptr` ist nicht thread-safe für concurrent reads/writes ohne Synchronisation. Beim Script-Wechsel (UI-Thread schreibt neuen `chain`-Pointer, Audio-Thread liest ihn) entsteht eine Race Condition.

**Lösung:** `std::atomic<std::shared_ptr<Chain>>` (C++20) oder `std::atomic_load` / `std::atomic_store` (C++11-Kompatibilität).

---

## 2. Architektur-Probleme

### Problem 7: `PluginProcessor` ist eine God-Class
**Datei:** `src/core/PluginProcessor.cpp`  
**Größe:** ~34 KB

**Problem:** Der `PluginProcessor` enthält:
- DSP-Chain-Verwaltung
- Formel-Parsing und -Validierung
- Preset-Logik
- Waveform-Capture für das UI
- Licensing-Checks
- Parameter-Management

Das verletzt massiv das Single-Responsibility-Prinzip und macht den Code schwer zu testen, zu debuggen und zu erweitern.

**Lösung:** Aufteilen in `FormulaEngine`, `DSPChain`, `StateManager`, `ValidationEngine` (siehe `docs/ROADMAP.md` Phase 1).

---

### Problem 8: Keine Trennung von DSP und GUI-State
**Problem:** `evaluateFormula()` ist `public` und wird von beiden Threads aufgerufen:
- Audio-Thread: `processBlock()` für echtes Audio
- Message-Thread: `FormulaDisplayComponent` für Preview-Wellenform

Das führt zu potentiellen Race Conditions auf den internen Evaluator-State.

**Lösung:** Separaten Evaluator-Instanzen für GUI-Preview und Audio-Processing.

---

### Problem 9: `autoGainCompensate()` per-Sample SubBlock
**Datei:** `src/dsp/DSPUtils.h`  
**Zeile:** ~159–165

```cpp
for (size_t i = 0; i < mixed.getNumSamples(); ++i)
{
    gain.setGainLinear(smoothed.getNextValue());
    auto slice = mixed.getSubBlock(i, 1);
    gain.process(juce::dsp::ProcessContextReplacing<float>(slice));
}
```

**Problem:** `getSubBlock(i, 1)` + `ProcessContextReplacing` pro Sample ist **extrem teuer**. Jeder `getSubBlock`-Aufruf erzeugt ein temporäres Objekt, und der Gain-Prozess hat pro-Block-Overhead.

**Lösung:** Direkte Sample-Multiplikation in einer einfachen Schleife:
```cpp
for (size_t i = 0; i < numSamples; ++i)
    buffer[i] *= smoothed.getNextValue();
```

---

## 3. Fehlende Tests

### Problem 10: Keine DSLParser-Tests
**Status:** Kritisch – der DSLParser ist der komplexeste Teil des Systems und hat **keine Tests**.

**Empfehlung:** Unit-Tests für:
- Gültige Blöcke aller Typen
- Fehlerhafte Syntax (Fehlermeldungen prüfen)
- Edge Cases (leerer Input, maximale Länge, Sonderzeichen)

---

### Problem 11: Keine Audio-Integrationstests
**Status:** Hoch – kein Test prüft ob `processBlock()` korrekte Ausgaben erzeugt.

**Empfehlung:** Tests mit bekanntem Input (Sinus-Welle, Impulse, DC-Offset) und erwartetem Output.

---

### Problem 12: Keine Fuzz-Tests für den Expression-Parser
**Status:** Mittel – bei User-Eingaben (DSL-Formeln) besteht das Risiko von Crashes bei unerwarteten Inputs.

**Empfehlung:** AFL++ oder libFuzzer auf `ExpressionEvaluator::parseFormula()` und `DSLParser::parse()` anwenden.

---

### Problem 13: `SignalChainTest` nicht im CMake Test-Target
**Datei:** `CMakeLists.txt`  
**Problem:** `tests/SignalChainTest.h` existiert, ist aber **nicht** im `NeuroKoreTests`-Target verlinkt.

**Lösung:** Die Datei in das Test-Target aufnehmen.

---

## 4. Fehlende Features bis zum Gold-Standard

### DSP/Audio-Qualität

| Feature | Priorität | Status |
|---|---|---|
| Oversampling UI-Control | ✅ | 1× / 2× / 4× / 8× in Settings + Statuszeile |
| DC-Blocker | ✅ | 1-Pol 5 Hz, erstes Glied der Sanitation-Kette nach DSL |
| Latenz-Reporting | 🟡 Mittel | `getLatencyCompensationInSamples()` fehlt |
| Tail-Time | 🟡 Mittel | `getTailLengthSeconds()` gibt 0 zurück |
| Sample-Rate Adaptation | 🟡 Mittel | Filter-Koeffizienten bei Rate-Änderung? |

### UI/UX

| Feature | Priorität | Status |
|---|---|---|
| Resizable UI | ✅ | Seitenverhältnis fest; Scale 100 / 125 / 150 |
| Undo/Redo | ✅ | Ctrl+Z / Ctrl+Y auf Formel + Graph |
| Tooltips | 🟡 Mittel | Keine kontextuellen Hilfe-Tooltips |
| HiDPI/Retina | 🟡 Mittel | PNG-basiert, kein Vektor-Rendering |
| Accessibility | 🟡 Mittel | Kein `AccessibilityHandler` |

### Plattform/Distribution

| Feature | Priorität | Status |
|---|---|---|
| AU-Format (macOS) | 🟢 Erledigt | CMake `aumf`; CI-Job `AU (macOS)` liefert `.component` |
| AAX-Format | 🟡 Mittel | Kein Pro-Tools-Support |
| CLAP-Format | 🟢 Nice | Wachsendes Ökosystem |
| Installer | 🔴 Hoch | Kein NSIS/WiX/pkgbuild |
| Code-Signing | 🔴 Hoch | Gatekeeper/SmartScreen blockieren |
| CI/CD | 🟢 Da | Windows Tests+pluginval; macOS-Job für AU |

---

## 5. Positiv-Highlights

Diese Aspekte sind bemerkenswert gut gelöst:

1. **ExpressionEvaluator** – SIMD + Constant-Folding + CSE ist professionell implementiert
2. **DSL-Konzept** – "ShaderToy für Audio" ist einzigartig und innovativ
3. **Cross-Fade zwischen Formeln** – `formulaBlend` verhindert Glitches beim Formel-Wechsel
4. **Formel-Validierung** – `testFormulaStability()` mit NaN/Inf-Erkennung ist vorbildlich
5. **Lokalisierung** – DE/EN mit lokalisierten DSL-Fehlermeldungen ist ungewöhnlich gut
6. **LookupTable + Smoothing** – Kombination für Performance und Qualität ist clever
7. **WeightedLayout** – Flexibles Layout-System zeigt durchdachtes UI-Design
