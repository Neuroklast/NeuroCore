# Coding Agent Workflow

Dieses Dokument beschreibt den **verbindlichen Ablauf** für jeden Coding-Agent-Einsatz im NeuroCore-Projekt.
Alle Regeln müssen eingehalten werden, um Qualität und Nachvollziehbarkeit zu gewährleisten.

---

## Pflicht-Ablauf für JEDE Session

### Phase 1: PLANEN (vor jedem Code-Change)

1. `docs/DEVELOPMENT_STATUS.md` lesen → aktuellen Stand und offene Aufgaben verstehen
2. `docs/LESSONS_LEARNED.md` lesen → bekannte Fallstricke und vergangene Probleme beachten
3. `docs/ANALYSIS.md` lesen → bekannte Bugs und Architektur-Probleme im Blick behalten
4. Konkreten Plan formulieren:
   - Was genau wird geändert?
   - Welche Dateien sind betroffen?
   - Welche Tests werden geschrieben oder angepasst?
   - Welche Risiken bestehen?
5. Plan in der PR-Beschreibung dokumentieren (Checkliste verwenden)

**⛔ KEIN Code schreiben, bevor Phase 1 abgeschlossen ist.**

---

### Phase 2: UMSETZEN

6. Code-Änderungen gemäß dem in Phase 1 formulierten Plan durchführen
7. **Nur eine logische Änderung pro Session** – nicht alles auf einmal!
   - Falsch: "Refactore PluginProcessor + fixe Thread-Safety + schreibe Tests"
   - Richtig: "Fixe Thread-Safety für getScript()" (eine Sache)
8. Änderungen in kleinen, nachvollziehbaren Schritten committen

---

### Phase 3: TESTEN

9. Bestehende Tests ausführen:
   ```bash
   cmake --build build --target NeuroKoreTests
   ctest --test-dir build
   ```
10. Neue Tests für geänderten Code schreiben
11. Manuelle Prüfung:
    - Kompiliert der Code ohne Warnungen?
    - Läuft das Plugin als Standalone korrekt?
    - Keine neuen Warnungen in der IDE/CI?

---

### Phase 4: ANPASSEN (wenn etwas nicht klappt)

12. **⛔ STOP** – nicht blind weitermachen wenn Tests fehlschlagen!
13. Analysieren: Was genau schlägt fehl? Warum?
14. Plan überarbeiten
15. Erkenntnisse in `docs/LESSONS_LEARNED.md` dokumentieren (BEVOR neu implementiert wird)
16. Neuen Ansatz formulieren
17. Erst dann neu implementieren (zurück zu Phase 2)

---

### Phase 5: DOKUMENTIEREN

18. `docs/DEVELOPMENT_STATUS.md` aktualisieren:
    - Modul-Status anpassen
    - Checkliste aktualisieren (abgeschlossene Punkte markieren)
    - Neue offene Punkte eintragen
19. `docs/LESSONS_LEARNED.md` ergänzen (mindestens ein Eintrag pro Session)
20. Betroffene `docs/*.md` Dateien aktualisieren (z. B. `ANALYSIS.md` wenn ein Bug behoben wurde)

---

## Verbindliche Regeln

### Allgemein
- **NIEMALS** blind durchziehen wenn Tests fehlschlagen
- **IMMER** erst planen, dann umsetzen
- **Kleine, fokussierte Änderungen** sind besser als große Monolith-PRs
- **JEDE Session** aktualisiert `docs/DEVELOPMENT_STATUS.md`
- **JEDE Session** fügt mindestens einen Eintrag zu `docs/LESSONS_LEARNED.md` hinzu

### Code-Qualität
- Keine neuen Compiler-Warnungen einführen
- Keine neuen TODO/FIXME ohne Eintrag in `DEVELOPMENT_STATUS.md`
- Thread-Safety bei JEDER neuen Klasse bedenken (Audio-Thread vs. Message-Thread)
- Keine Heap-Allokationen im Audio-Thread (kein `new`, keine `std::string`-Operationen, kein `juce::String` unter Lock)

### Dokumentation
- Neue öffentliche Methoden kurz kommentieren
- Neue DSL-Features in `docs/DSL_REFERENCE.md` ergänzen
- Neue Architektur-Entscheidungen in `docs/ARCHITECTURE.md` eintragen

### Tests
- Für jeden behobenen Bug: Regression-Test schreiben
- Für jede neue Funktion: mindestens ein Unit-Test
- Fuzz-Tests für alle Parser-Eingaben (DSLParser, ExpressionEvaluator)

---

## Typische Fallstricke (Kurzversion)

> Vollständige Liste in `docs/LESSONS_LEARNED.md`

1. **JUCE-Build** braucht spezifische JUCE-Version (≥ 8.0.6) und VST3-SDK
2. **Audio-Thread** darf NICHT blockieren (kein I/O, keine Locks mit Heap-Allokation)
3. **CMakeLists.txt** hat doppelte Source-Einbindung (noch nicht behoben!)
4. **SignalChain** hat Race Conditions bei Script-Wechsel (noch nicht behoben!)
5. **PluginProcessor** ist eine God-Class – neue Features NICHT einfach dort hinzufügen

---

## Schnellreferenz: Build & Test

```bash
# Build vorbereiten (einmalig)
cmake -B build -S .

# Build ausführen
cmake --build build --config Release

# Nur Tests bauen und ausführen
cmake --build build --target NeuroKoreTests
ctest --test-dir build --output-on-failure

# Mit Sanitizern (empfohlen für Development)
cmake -B build-asan -S . -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --target NeuroKoreTests
```

---

## Verwandte Dokumente

| Dokument | Zweck |
|---|---|
| `docs/DEVELOPMENT_STATUS.md` | Aktueller Entwicklungsstand und Checklisten |
| `docs/LESSONS_LEARNED.md` | Erfahrungsspeicher aus allen Sessions |
| `docs/ANALYSIS.md` | Vollständige Code-Analyse mit allen bekannten Bugs |
| `docs/ROADMAP.md` | Phasen-basierte Entwicklungs-Roadmap |
| `docs/ARCHITECTURE.md` | Architektur-Übersicht aller Module |
| `docs/DSL_REFERENCE.md` | DSL-Sprachreferenz |
