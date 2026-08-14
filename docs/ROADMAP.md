# NeuroCore – Entwicklungs-Roadmap

**Version:** 0.1.0  
**Stand:** 2026-04-01  
**Ziel:** Professionelles, veröffentlichungsreifes Audio-Plugin

---

## Phase 1 – Stabilität (2–4 Wochen)

Diese Phase behebt alle kritischen Bugs und legt das Fundament für professionelle Qualität.

### Thread-Safety & Architektur
- [ ] `PluginProcessor` refactoren: God-Class in `FormulaEngine`, `DSPChain`, `StateManager`, `ValidationEngine` aufteilen
- [ ] Thread-Safety für `SignalChain` fixen: `std::atomic<std::shared_ptr<Chain>>` (C++20) verwenden
- [ ] `getScript()` thread-safe machen (Lock beim Lesen)
- [ ] `SpinLock` + `juce::String` ersetzen: `juce::AbstractFifo` + char-Array für UI→Audio-Kommunikation
- [ ] Separate `ExpressionEvaluator`-Instanzen für GUI-Preview und Audio-Processing

### Build & Infrastruktur
- [ ] CMake: doppelte Source-Einbindung fixen (`SOURCES` in `juce_add_plugin` ODER `target_sources`, nicht beides)
- [ ] `SignalChainTest.h` in das CMake Test-Target `NeuroCoreTests` aufnehmen
- [ ] CI/CD: GitHub Actions Workflow für Matrix-Build (Windows/macOS/Linux)

### DSP-Korrektheit
- [ ] DC-Blocker in Signalkette einbauen (nach OutputGain oder als optionaler Stage)
- [ ] Latenz-Reporting: `getLatencyCompensationInSamples()` korrekt implementieren
- [ ] `autoGainCompensate()` optimieren: per-Sample SubBlock durch direkte Multiplikation ersetzen
- [ ] Oversampling UI-Control implementieren (Dropdown im Editor)

### Licensing
- [x] Offline RSA-`.lic` + `NeuroCoreIssuer` (E-Mail → Datei)
- [x] Demo: Mix = 0 nach 20 Minuten
- [ ] Optional später: Online-Widerruf / Maschinenbindung

---

## Phase 2 – Professionalität (4–6 Wochen)

Diese Phase bringt NeuroCore auf den Stand eines professionell vertriebenen Plugins.

### Format-Support
- [x] AU-Format für macOS aktivieren (JUCE `FORMATS AU`, `aumf` MusicEffect)
- [ ] CLAP-Format vorbereiten (Community-Wrapper evaluieren)
- [ ] AAX-Format für Pro Tools (Avid-Zertifizierung erforderlich)

### UI/UX-Verbesserungen
- [ ] Resizable UI: `setResizable(true, true)` + `ComponentBoundsConstrainer`
- [ ] Undo/Redo: `juce::UndoManager` für Formeländerungen
- [ ] MIDI Learn für alle 4 Knobs (a–d)
- [ ] Tooltips für alle UI-Elemente (DSL-Befehle, Knobs, Buttons)
- [ ] Echtzeit-Parameterwert-Anzeige über den Knobs (a=1.23)

### Distribution
- [ ] Windows-Installer (NSIS oder WiX)
- [ ] macOS-Installer (pkgbuild / DMG)
- [ ] Code-Signing (Gatekeeper + SmartScreen)
- [ ] pluginval-Integration in CI/CD (automatische VST3-Validierung)

### Tests
- [ ] DSLParser-Unit-Tests (alle Block-Typen, Fehlerbehandlung, Edge Cases)
- [ ] Audio-Integrationstests (processBlock mit bekanntem Input/Output)
- [ ] Fuzz-Tests für `ExpressionEvaluator::parseFormula()` (AFL++ oder libFuzzer)
- [ ] Sanitizer-Builds in CI (ASan, TSan, UBSan)

---

## Phase 3 – Innovation (6–8 Wochen)

Diese Phase hebt NeuroCore über den Stand typischer kommerzieller Plugins.

### Erweiterte DSL
- [ ] `delay`-Block: Delay-Line mit Feedback
- [ ] `reverb`-Block: einfacher Algorithmic-Reverb
- [ ] `sidechain`-Block: Sidechain-Input-Routing
- [ ] Mehr integrierte Funktionen: `comb`, `allpass`, `rms_window`

### Editor-Integration
- [ ] WebView-Integration für DSL-Editor: Monaco-Editor (Syntax-Highlighting) via `juce::WebBrowserComponent`
- [ ] visageui als npm-Package im WebView für Preset-Browser und Panels
- [ ] Hot-Reloading der DSL während Entwicklung

### Performance
- [ ] JIT-Compilation für Formeln (LLVM oder custom Bytecode-VM)
- [ ] Block-Processing im `ExpressionEvaluator` vollständig ausbauen (SIMD für alle Operationen)

### Community & Sharing
- [ ] Preset-Sharing: Export/Import via URL (Base64-kodiertes Preset)
- [ ] Online-Preset-Library (GitHub-Repository als Backend)
- [ ] In-App DSL-Hilfesystem mit Beispielen

### Visualisierung
- [ ] Spectral-Analyzer (FFT-basiertes Frequenzspektrum)
- [ ] Phase-Scope (Lissajous-Figur für Stereo-Analyse)
- [ ] Transfer-Curve-Anzeige (Eingabe-/Ausgabe-Mapping für Waveshaping)

---

## Phase 4 – Polish (2–4 Wochen)

### Grafik & Accessibility
- [ ] HiDPI/Retina: Vektor-basierte Grafiken für Knobs und Icons (SVG oder OpenGL)
- [ ] Accessibility: `juce::AccessibilityHandler` für alle interaktiven Elemente
- [ ] Dark/Light-Mode-Unterstützung

### Inhalte
- [ ] 50+ Factory-Presets (Distortion, Modulation, Filter, Creative)
- [ ] Video-Tutorials (Getting Started, DSL-Einführung, Fortgeschrittene Techniken)
- [ ] In-App-Hilfe: Kontextuelle Erklärungen zu DSL-Blöcken

### Qualitätssicherung
- [ ] Beta-Testing mit 10–20 echten Usern
- [ ] Crash-Reporting (Sentry oder Breakpad)
- [ ] Performance-Profiling (Xcode Instruments / VTune)
- [ ] JUCE pluginval: 100% aller Tests müssen bestehen

---

## Übersicht Timeline

```
2026-04                          2026-05        2026-06        2026-07
|-------- Phase 1 ---------|-------- Phase 2 ---------|
                                              |--- Phase 3 ------|--- Phase 4 ---|
```

---

## Abhängigkeiten & Risiken

| Risiko | Wahrscheinlichkeit | Impact | Mitigation |
|---|---|---|---|
| JUCE API-Änderungen | Mittel | Hoch | Version pinnen, Changelogs beobachten |
| AAX-Zertifizierung scheitert | Mittel | Mittel | AU als macOS-Hauptformat priorisieren |
| JIT-Komplexität unterschätzt | Hoch | Mittel | Bytecode-VM als Fallback |
| Lizenzserver-Kosten | Mittel | Mittel | Open-Source-Alternative (Keygen.sh) |
