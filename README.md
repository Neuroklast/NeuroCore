# NeuroCore

NeuroCore ist ein experimentelles Audio-Plug-in, das Audioeingangsdaten mithilfe einer frei definierten mathematischen Formel transformiert. Die Formel wird zur Laufzeit ausgewertet und kann Modulationen über vier Parameter **a** bis **d** verwenden. Zusätzlich steht ein Sinus-Modulationssignal (*mod*) bereit.

## Voraussetzungen

- **JUCE 7** oder neuer. Das Projekt wurde mit den Standardmodulen ohne Anpassungen erstellt.
- **IDE**: Visual Studio 2022 (andere IDEs mit JUCE-Projucer/CMake sind möglich). Stellen Sie sicher, dass der JUCE-Pfad korrekt eingetragen ist (siehe *NeuroCore.jucer*). JUCE muss die VST3-SDK enthalten.

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

