# NeuroCore

NeuroCore ist ein experimentelles Audio-Plug-in, das Audioeingangsdaten mithilfe einer frei definierten mathematischen Formel transformiert. Die Formel wird zur Laufzeit ausgewertet und kann Modulationen über vier Parameter **a** bis **d** verwenden. Zusätzlich steht ein Sinus-Modulationssignal (*mod*) bereit.

## Voraussetzungen

- **JUCE 8.0.6** oder neuer. Der Pfad zu den JUCE-Modulen muss im Projucer unter *Global Paths* eingetragen sein, beispielsweise `D:\JUCE\modules` oder `~/JUCE/modules`. Stellen Sie sicher, dass die VST3‑SDK unter `juce_audio_processors/format_types/VST3_SDK` vorhanden ist.
- **IDE**: Visual Studio 2022 (andere IDEs mit JUCE-Projucer/CMake sind möglich).

## Abhängigkeiten einrichten

1. JUCE inklusive Untermodulen klonen:
   ```bash
   git clone --recurse-submodules https://github.com/juce-framework/JUCE.git <JUCE>
   ```
2. Den Pfad `<JUCE>/modules` im Projucer unter *Global Paths → JUCE Modules* eintragen oder über die Umgebungsvariable `JUCE_PATH` bereitstellen.
3. Sicherstellen, dass die VST3-SDK im Ordner `<JUCE>/modules/juce_audio_processors/format_types/VST3_SDK` liegt.

## Build-Schritte

1. Projektdatei `NeuroCore.jucer` im Projucer öffnen und den JUCE-Pfad anpassen ("Global Path"). Anschließend über *File → Export Project* die gewünschten Exporter generieren.
2. Für die **Standalone-Version** das Projekt `NeuroCore_StandalonePlugin` in der IDE öffnen und übersetzen. Die erzeugte Anwendung verhält sich wie ein eigenständiger Effekt.
3. Für die **VST3-Version** das Projekt `NeuroCore_VST3` kompilieren. Die entstandene *.vst3*-Datei kann in kompatiblen Hosts geladen werden.

## Plug-in-Überblick

Nach dem Start zeigt die Oberfläche vier Regler (Parameter *a–d*) sowie ein Eingabefeld für die Formel. Jede Formel darf die Variablen **x** (aktuelles Samplesignal), **mod** (Sinus-LFO) und die Parameter **a–d** verwenden. Beispiel:

```
clamp(tanh(x * a + mod * b), -c, d)
```

Diese Formel wendet eine Tangens-Hyperbolicus-Transformation an und begrenzt das Ergebnis mithilfe der Parameter *c* und *d*.

## Presets laden und speichern

Um die Parameter- und Formeldaten zu sichern, kann die JUCE-eigene `AudioProcessorValueTreeState` verwendet werden. Beispiele finden sich in den Methoden `getStateInformation()` und `setStateInformation()` des Plug-ins. Dort lässt sich ein `ValueTree` oder XML-Dokument erzeugen und in `destData` speichern bzw. beim Laden daraus rekonstruieren.

## Lokalisierung

Sprachressourcen liegen im Ordner `Resources` in einfachen Textdateien. Beim Start lädt das Plug-in automatisch die Datei `de.txt` oder `en.txt` abhängig von der Systemsprache. Weitere Sprachen können durch zusätzliche Dateien im gleichen Format ergänzt werden.

