# AGENTS

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
