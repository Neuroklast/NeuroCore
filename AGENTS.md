# AGENTS

> **⚠️ WICHTIG FÜR JEDEN CODING-AGENT:**
> 1. Lies zuerst **`docs/AGENT_WORKFLOW.md`** bevor du Code änderst
> 2. Lies **`docs/DEVELOPMENT_STATUS.md`** um den aktuellen Entwicklungsstand zu verstehen
> 3. Lies **`docs/LESSONS_LEARNED.md`** um bekannte Fallstricke zu beachten
> 4. Aktualisiere **`docs/DEVELOPMENT_STATUS.md`** und **`docs/LESSONS_LEARNED.md`** am Ende jeder Session

---

## Dokumentation

| Dokument | Beschreibung |
|---|---|
| [`docs/AGENT_WORKFLOW.md`](docs/AGENT_WORKFLOW.md) | Verbindlicher Workflow für Coding-Agent-Sessions |
| [`docs/DEVELOPMENT_STATUS.md`](docs/DEVELOPMENT_STATUS.md) | Aktueller Entwicklungsstand und Checklisten |
| [`docs/LESSONS_LEARNED.md`](docs/LESSONS_LEARNED.md) | Erfahrungsspeicher aus allen Sessions |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Architektur-Übersicht aller Module |
| [`docs/DSL_REFERENCE.md`](docs/DSL_REFERENCE.md) | DSL-Sprachreferenz |
| [`docs/ANALYSIS.md`](docs/ANALYSIS.md) | Code-Analyse: Bugs, Probleme, Lücken |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Phasen-basierte Entwicklungs-Roadmap |
| [`docs/UNIQUE_SELLING_POINTS.md`](docs/UNIQUE_SELLING_POINTS.md) | Alleinstellungsmerkmale |
| [`docs/VISAGE_UI_INTEGRATION.md`](docs/VISAGE_UI_INTEGRATION.md) | VisageUI Integration Guide |

---

## Build & Setup

Dieses Projekt basiert auf JUCE und benötigt wenige externe Abhängigkeiten.
Die folgenden Schritte installieren alle benötigten Komponenten:

1. **JUCE klonen (mindestens Version 8.0.6)**
   ```bash
   git clone --recurse-submodules https://github.com/juce-framework/JUCE.git ~/JUCE
   ```
   Alternativ kann eine bestehende Installation verwendet werden. Der Pfad muss
   der Umgebungsvariable `JUCE_DIR` entsprechen oder beim Aufruf von CMake mit
   `-DJUCE_DIR=/pfad/zur/JUCE` übergeben werden. Ist `JUCE_DIR` nicht gesetzt,
   lädt CMake JUCE automatisch.

2. **VST3 SDK bereitstellen**
   Kopiere die VST3-SDK in folgendes Verzeichnis:
   `~/JUCE/modules/juce_audio_processors/format_types/VST3_SDK`.
   Die SDK ist nötig, um das Plug-in als VST3 zu erzeugen.

3. **Abhängigkeiten via CMake aufbauen**
   ```bash
   cmake -B build -S .
   cmake --build build --config Release
   ```
   Diese Befehle laden JUCE (falls notwendig) und erstellen alle Artefakte im
   Ordner `build/NeuroCore_artefacts`.

4. **Tests ausführen**
   ```bash
   cmake --build build --target NeuroCoreTests
   ctest --test-dir build
   ```
   Sofern JUCE korrekt eingebunden ist, laufen alle Unit-Tests durch.
