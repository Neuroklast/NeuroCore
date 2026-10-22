# NEUROKORE – MASTER ROADMAP FOR CODING AGENT
## Vollständiger Ausbau, Reparatur und Qualitätssicherung aller DSP-Module

**Repository:** https://github.com/Neuroklast/NeuroCore  
**Verbindlicher Ausgangspunkt:** `main` ab Commit `ea293eb972e5`  
**Ausgangsversion:** 0.7.1-beta  
**Auftrag:** Bestehende Module fertigstellen, fehlende Module implementieren und jedes Modul einzeln vollständig testen.

---

# 0. OBERSTE ARBEITSANWEISUNG

Du bist der verantwortliche Senior-C++-DSP- und JUCE-Entwickler für NEUROKORE.

Deine Aufgabe ist es, die gesamte DSP-Engine anhand dieser Roadmap systematisch zu vervollständigen.

**Arbeite strikt Modul für Modul.**

Ein Modul gilt erst dann als abgeschlossen, wenn seine native DSP-Implementierung, sämtliche vorgesehenen Parameter, die Circuit-Integration, die Bedienoberfläche, Terminal/DSL, die Speicherung und die erforderlichen automatisierten Tests vollständig funktionieren.

Beginne niemals parallel mit der eigentlichen Implementierung des nächsten Moduls, während das vorherige noch offene Fehler oder unvollständige Abnahmekriterien besitzt.

### Verbindlicher Ablauf für JEDES Modul

1. Aktuellen Stand aus `main` prüfen.
2. Bestehende Implementierung vollständig lesen.
3. Anforderungen und tatsächliches Verhalten vergleichen.
4. Reproduzierbare Tests für fehlende oder falsche Funktionalität schreiben.
5. Native DSP-Implementierung entwickeln beziehungsweise verbessern.
6. Parser, Signalgraph und Modulationsanbindung integrieren.
7. Block-Katalog und Parameterdefinitionen vervollständigen.
8. Circuit- und Inspect-Bedienung implementieren.
9. Terminal-Completion und DSL validieren.
10. Preset- und Host-State-Persistenz prüfen.
11. Native DSP-, Stereo-, Parameter- und Regressionstests ausführen.
12. CPU-/Speicher-/Latenzauswirkungen prüfen.
13. Dokumentation aktualisieren.
14. Code selbst reviewen, vereinfachen und Fehler beheben.
15. Eigenen Feature-Branch pushen und Pull Request erstellen.
16. CI kontrollieren und Fehler beheben.
17. Erst bei erfüllten Abnahmekriterien das Modul als fertig markieren.
18. Danach das nächste Modul beginnen.

Führe keine rein kosmetischen Scheinimplementierungen ein.

Ein vorhandener Dateiname, ein Eintrag im Add-Menü oder ein erfolgreicher Kompiliervorgang gelten nicht als Nachweis für einen fertigen Effekt.

**Wichtig:** Ein Modul, das tatsächlich Audio verändert, kann trotzdem technisch falsch oder musikalisch unbrauchbar sein. Prüfe immer, ob der implementierte Algorithmus die versprochene Funktion auch wirklich erfüllt.

---

# 1. VERBINDLICHE ARCHITEKTUR

## 1.1 Bestehende Architektur erhalten

Arbeite mit den vorhandenen Projektkonventionen und dem aktuellen Code.

Relevante Bereiche:

**Native DSP und Verarbeitung**

- `src/dsl/SignalChain.h`
- `src/dsl/SignalChain.cpp`
- `src/dsl/DSLParser.cpp`
- `src/dsl/blocks/`
- `src/dsp/`
- `src/core/DspEngine.cpp`
- `src/core/Config.h`
- `src/bridge/`

**Web und Circuit**

- `web/src/assemble/blockCatalog.ts`
- `web/src/assemble/chipSpec.ts`
- `web/src/assemble/addBlock.ts`
- `web/src/assemble/nodeFace/`
- `web/src/assemble/ChipParameters.tsx`
- `web/src/hack/dslComplete.ts`
- `web/src/hack/dslLanguage.ts`
- `web/src/store/`
- `web/src/presets/`

**Tests und Dokumentation**

- `tests/dsp/`
- `tests/main.cpp`
- `docs/DSL_REFERENCE.md`
- `docs/manual/NEUROKORE.md`
- `docs/DEVELOPMENT_STATUS.md`
- `docs/evidence/dsp-expansion/`

Die angegebenen Pfade beschreiben den derzeitigen Projektaufbau. Prüfe bei jedem Modul, ob Dateien zwischenzeitlich verschoben wurden.

## 1.2 Zentrale Registry

Führe die vorhandenen Definitionen konsequent zusammen.

Der Block-Katalog muss die verbindliche Metadatenquelle sein für:

- Block-ID
- Anzeigename
- Kategorie
- Parameter
- Defaults
- Wertebereiche
- Einheiten
- Enum-Werte
- Audioports
- Modulationsports
- UI-Familie
- Terminal-Vervollständigung
- DSL-Vorlagen

Der aktuelle Zustand enthält einen zentralen `blockCatalog.ts`, aber zusätzlich noch separat gepflegte Add-Menü-Einträge.

Beseitige die unnötige Duplizierung, ohne bisherige Blocknamen, DSL-Aliase oder gespeicherte Presets zu beschädigen.

Prüfe die tatsächlich verwendete Architektur, bevor du eine zweite Registry oder ein komplexes Codegenerierungssystem einführst.

## 1.3 Echtzeitfähigkeit

Im Audiothread sind nicht erlaubt:

- Dynamische Speicherallokationen
- Dateioperationen
- Blockierende Synchronisation
- Parsen von Skripten
- Dynamische Erzeugung von FFT-Plänen
- Konstruktion komplexer Filtergraphen
- Unbegrenzte Schleifen
- Rechenaufwendige UI-Kommunikation
- Unkontrollierte Zerstörung großer DSP-Zustände

Insbesondere `clearRuntimeState()` und Reset-Pfade überprüfen.

Ein Reset darf vorbereitete Audiopuffer grundsätzlich leeren, aber nicht bei jedem Aufruf neu allokieren.

`prepare()` darf notwendige Ressourcen erstellen und vorbereiten. `processBlock()` muss mit diesen Ressourcen arbeiten.

## 1.4 Oversampling und Gain-Staging

Verwende nach Möglichkeit das vorhandene globale Oversampling.

Implementiere kein unabhängiges Oversampling für jeden neuen Verzerrungsblock.

Prüfe bei nichtlinearen Effekten:

- Aliasing
- Harmonische
- Eingangs- und Ausgangspegel
- DC-Offset
- Gain Matching
- Parameter-Smoothing
- Hochfrequenzverhalten
- Verhalten bei unterschiedlichen Sample-Rates

Unterscheide internes Floating-Point-Headroom von bewusstem Clipping.

Vermeide versteckte Lautstärkekorrekturen.

## 1.5 Modul-Kompatibilität

Folgende Bestandteile dürfen nicht unbeabsichtigt verändert werden:

- VST3-Class-ID
- Bestehende Host-Parameter-IDs
- Bisherige DSL-Syntax
- Bestehende Preset-Dateien
- Zustandswiederherstellung älterer DAW-Projekte
- Makrozuordnungen
- Audio-/Modulationsanschlüsse
- Vorhandenes Platinen-Design

Neue Parameter erhalten eindeutige und stabile Bezeichner.

Da der DSL-Parser Parameternamen normalisiert, müssen native Parameterzuordnungen und UI-Spezifikationen dieselbe kanonische Schreibweise verwenden. Behalte alte Schreibweisen bei Bedarf als Aliase bei.

## 1.6 Statusklassen

Jedes Modul erhält im Audit genau einen Status:

- `READY`: Funktionsumfang vollständig implementiert und alle verfügbaren Pflichtprüfungen erfolgreich
- `NEEDS_WORK`: Implementiert, aber technische oder funktionale Lücken
- `MISSING`: Kein entsprechendes Modul vorhanden
- `BLOCKED`: Abhängigkeit oder Testinfrastruktur fehlt
- `VALIDATION_PENDING`: Implementiert, aber erforderliche Plattform- oder Audioabnahme ausstehend

Ein Modul darf nicht nur aufgrund erfolgreicher Unit-Tests als produktionsreif bezeichnet werden.

---

# 2. PHASE A – BASIS STABILISIEREN

Diese Phase vor neuen Funktionen durchführen.

## A01 – Aktuellen Stand sichern

Prüfe:

- Aktuellen `main`-Commit
- Offene PRs und Branches
- Bestehende Tests
- Build-Konfiguration
- Status der Native-CI
- Bestehende Presets
- Web-Build
- Aktuelle Modulregistry

Prüfe insbesondere, dass die Änderungen aus PR #324 bereits in `main` enthalten sind.

Erstelle einen reproduzierbaren Ausgangsstand und dokumentiere bereits vorhandene Testfehler.

**Abnahme:** Reproduzierbare Baseline einschließlich Git-SHA und Testprotokoll.

## A02 – DSP-Testpipeline vervollständigen

Die Pipeline muss nach Änderungen an nativen DSP-Modulen automatisch geeignete Tests ausführen.

Erforderlich:

- Native C++-Build
- DSP-Unit-Tests
- Parser-Tests
- Audio-Regressionsprüfungen
- Parameter-Contract-Tests
- Web- und Katalogtests
- Typprüfung
- Dokumentations-Synchronisierung

Richte die vorhandenen GitHub-Workflows weiter aus, statt redundante CI-Systeme aufzubauen.

Die bereits eingeführten DSP-Expansion-Contracts sind Grundlage.

**Abnahme:** Ein absichtlich defekter DSP-Vertrag führt zuverlässig zu einem fehlgeschlagenen CI-Check.

## A03 – Modulationsarchitektur vorbereiten

Prüfe, wie `osc` und `env` ihre Modulations-Lanes erzeugen und in Zielparameter schreiben.

Aktuell werden diese Quellen besonders behandelt.

Die neuen Module wie StepSeq, ADSR, Random, ModMath und ModMix dürfen nicht lediglich als normale Audioblocks enden.

Implementiere beziehungsweise vervollständige einen gemeinsamen Modulationsvertrag:

- Audio- und Kontrollsignale getrennt typisieren
- Unipolar und bipolar
- Mehrere Quellen
- Mehrere Ziele
- Depth und Polarity
- Host-Sync
- Samplegenaue beziehungsweise definierte Control-Rate-Verarbeitung
- Cycle Detection
- Verlässliches Reset
- Keine Allokationen im Audiothread

Die neuen Modulatoren müssen tatsächlich auf Parameter wie Cutoff, Drive, Threshold, Mix und Feedback wirken können, sofern diese moduliert werden dürfen.

**Abnahme:** Ein neues Modulationssignal kann nachweisbar einen beliebigen dafür freigegebenen Zielparameter kontrollieren, ohne einen Audiokanal als Ersatz für eine Modulationsverbindung zu missbrauchen.

---

# 3. PHASE B – DISTORTION UND KLANGFORMUNG

Arbeite die Module in der angegebenen Reihenfolge ab.

## B01 – CLIPPER (`clip`)

**Aktueller Zustand:** Weitgehend implementiert. Die bisherigen P0-Korrekturen sind gemergt.

### Bestehende Funktionen

- Hard Clip
- Soft Clip
- Asymmetric Clip
- Input Drive
- Threshold
- Knee
- Bias
- Output Trim
- Mix
- Auto Gain
- DC-Filterung

### Noch auszuführen

1. Aktuelle Implementierung nach PR #324 erneut überprüfen.
2. Korrekte Threshold-Begrenzung testen.
3. Kontinuierlichkeit der Soft-Clipping-Transferfunktion überprüfen.
4. Asymmetrischen Modus auf DC-Offset testen.
5. Auto Gain hinsichtlich Pumping und Gain Matching prüfen.
6. Parameterwechsel während der Wiedergabe testen.
7. Oversampling- und Aliasing-Verhalten messen.
8. Mono und Stereo vergleichen.
9. Korrekte Speicherung aller Parameter prüfen.
10. Kontrollieren, dass der Inspector alle Parameter zuverlässig steuert.

### Spezifische Tests

- 44,1 / 48 / 96 / 192 kHz
- Stille
- Hochpegel-Signale
- Sinussweep
- Threshold-Sweep
- Knee-Sweep
- Asymmetrie
- DC-Offset
- Auto Gain
- Mix 0 und Mix 1

### Fertig, wenn

Der Clipper die angegebene Transferfunktion korrekt umsetzt, bei Parameteränderungen stabil bleibt und alle erforderlichen Contracts besteht.

**Keine unnötige Neuentwicklung des bereits vorhandenen Clippers.**

---

## B02 – SATURATOR (`saturate`)

**Aktueller Zustand:** Grundimplementierung vorhanden. Wichtige Reparaturen wurden mit PR #324 gemergt.

### Gewünschte Modi

- Tape
- Tube
- Transistor
- Transformer
- Diode

### Parameter

- Drive
- Bias
- Dynamics
- Tone
- Character
- Output Trim
- Mix
- Auto Gain

### Aufgaben

1. Neue SaturationModels-Implementierung überprüfen.
2. Sicherstellen, dass alle fünf Modi unterschiedliche Klangcharakteristiken besitzen.
3. Tone-Regler auf tatsächliche Filterwirkung überprüfen.
4. Dynamics-Regler auf wirksame dynamische Bearbeitung prüfen.
5. Character-Regler akustisch und numerisch validieren.
6. Bias und DC-Unterdrückung prüfen.
7. Interne Stereo-Verknüpfung bei dynamischer Bearbeitung prüfen.
8. Kontinuierliche Transferkurven sicherstellen.
9. Output Trim und Auto Gain korrekt synchronisieren.
10. Aliasing bei Drive-Maximum messen.
11. Bei Bedarf bestehende Algorithmen optimieren, ohne erneut kaputte Kurven einzuführen.

### Spezifische Tests

Für jeden Modus:

- Harmonisches Spektrum
- THD-Verlauf mit Drive
- Effektivität von Tone, Dynamics und Character
- Null- und Hochpegel-Verhalten
- DC-Settling
- Stereo-Kanaltrennung
- Kontinuierliche Transferfunktion
- Wet/Dry-Transparenz

### Fertig, wenn

Jeder Modus eigenständig klingt, sämtliche Regler hörbar und messbar funktionieren und keine unstetigen Artefakte auftreten.

---

## B03 – FREQUENCY SHIFTER (`freqshift`)

**Aktueller Zustand:** FIR-Hilbert-basierte Reparatur aus PR #324 vorhanden.

### Parameter

- Frequency Shift in Hz
- Fine Shift
- Shift Direction
- Feedback
- Stereo Spread
- Mix

### Aufgaben

1. Bestehende FIR-Hilbert-Implementierung überprüfen.
2. Korrekte Single-Sideband-Verschiebung testen.
3. Image-Rejection messen.
4. Gemeldete und tatsächliche Latenz vergleichen.
5. Dry/Wet-Latenzausgleich überprüfen.
6. Positive und negative Frequenzverschiebung testen.
7. Fine Shift ergänzen.
8. Feedback optional ergänzen.
9. Stereo Spread ergänzen.
10. Bei Nullverschiebung Phasen- und Klangverhalten prüfen.

### Spezifische Tests

- Sinusfrequenz 1 kHz + Shift 100 Hz
- Sinusfrequenz 6 kHz + Shift 400 Hz
- Positive und negative Verschiebung
- Image-Rejection
- 0 Hz
- Mix 0.5
- Stereo
- Alle unterstützten Sample-Rates
- Latenz und Tail

### Fertig, wenn

Echte einseitige Frequenzverschiebung statt bloßer Amplitudenmodulation erfolgt und die Latenz vollständig korrekt behandelt wird.

---

## B04 – DECIMATOR (`decimate`)

**Aktueller Zustand:** Bit- und Sample-Rate-Reduktion vorhanden, mehrere Parameter und Details verbesserungsbedürftig.

### Parameter

- Rate Reduction
- Bit Depth
- Jitter
- Hold
- Reconstruction
- Output Trim
- Mix

### Aufgaben

1. Sample-Hold-Timing überprüfen.
2. Sicherstellen, dass linke und rechte Kanäle unabhängig verarbeitet werden.
3. Jitter funktional implementieren und testen.
4. Hold-Parameter wirksam machen.
5. Bitquantisierung für positive und negative Eingangssignale symmetrisch prüfen.
6. Definiertes Verhalten bei extrem niedriger Sample-Rate.
7. Reconstruction-Filter verbessern.
8. Aliasing bewusst kontrollierbar machen.
9. Output Gain und Mix korrigieren.
10. Parameterbereiche zwischen DSP und UI abgleichen.

### Spezifische Tests

- Stereo L/R getrennt
- Sinus bei niedriger Bitauflösung
- Rate 1 gegen Rate 0.1
- Jitter 0 gegen Jitter 1
- Hold 0 gegen Hold 1
- Reconstruction 0 gegen 1
- Hochpegel-Signal
- Parameter-Smoothing

### Fertig, wenn

Bitquantisierung, Sample-Rate-Reduktion und Jitter unabhängig voneinander kontrollierbar sind.

---

## B05 – CUSTOM WAVESHAPER (`shaper`)

**Aktueller Zustand:** Einfacher Fold-/Clip-Shaper. Der ursprünglich geplante grafische Waveshaper fehlt.

### Ziel

Ein vollständig frei programmierbarer Waveshaper mit grafischem Kurveneditor.

### Parameter

- Input Drive
- Output Trim
- Mix
- Curve A
- Curve B
- Morph
- Symmetry
- Bias
- Optional DC Block

### Aufgaben

1. Vorhandene Fold- und Clip-Modi erhalten.
2. Neues Kurven-Datenmodell erstellen.
3. Frei platzierbare Kontrollpunkte unterstützen.
4. Punkte sortieren und validieren.
5. Geeignete kontinuierliche Kurveninterpolation implementieren.
6. Vorberechnete Lookup-Tabelle erstellen.
7. Echtzeitsicheren Austausch der Tabellen ermöglichen.
8. Kurven A und B unabhängig verwalten.
9. Morphing zwischen Kurven unterstützen.
10. Asymmetrische Kurven ermöglichen.
11. DC-Offset absichern.
12. Grafischen Kurveneditor im Inspect-Panel implementieren.
13. Zoom, Drag, Point-Insert und Delete hinzufügen.
14. Werte und Kurven persistent speichern.
15. Verhindern, dass Live-Editorbewegungen die Audioverarbeitung blockieren.
16. Passendes NodeFace mit tatsächlicher Transferkurve implementieren.

### Spezifische Tests

- Lineare Identitätskurve
- Hard-Clip-Kurve
- Soft-Clip-Kurve
- Asymmetrische Kurve
- Kurvenmorphing
- Ungültige Kontrollpunkte
- Schnelle Live-Bearbeitung
- Preset-Speicherung
- CPU-Messungen
- Aliasing

### Fertig, wenn

Eine vom Benutzer gezeichnete Kurve tatsächlich in der nativen Engine verarbeitet und nach dem Laden exakt wiederhergestellt wird.

---

## B06 – AMP / PREAMP (`amp`)

**Aktueller Zustand:** Nur Gain, Output Trim und Mix. Kein echter Amp-Simulator.

### Zielarchitektur

Input Conditioning → Preamp → Tone Stack → Poweramp → Master Output

### Modi

- Clean
- Crunch
- High Gain
- Bass

### Parameter

- Input Gain
- Preamp Gain
- Bass
- Mid
- Treble
- Presence
- Resonance
- Bias
- Sag
- Master
- Output Trim
- Mix

### Aufgaben

1. Bestehenden Gain-Block als kompatible Legacy-Konfiguration erhalten.
2. Neue Amp-Modi ergänzen.
3. Preamp-Verzerrungsstufe entwickeln.
4. Frequenzabhängige Vorverzerrung ergänzen.
5. Echten Tone Stack implementieren.
6. Poweramp-Sättigung entwickeln.
7. Bias und Sag sinnvoll modellieren.
8. Presence und Resonance realisieren.
9. Bass-Modus mit kontrolliertem Tieftonverhalten implementieren.
10. Hohe Drive-Werte hinsichtlich Aliasing prüfen.
11. Pegel zwischen den Stufen sinnvoll skalieren.
12. Cabinet IR als separaten Block unterstützen.
13. Amp-spezifisches Inspect-Panel gestalten.
14. Geeignete Presets für Gitarre, Bass und aggressive Synths erstellen.

### Spezifische Tests

- Clean bleibt bei niedrigem Gain ausreichend unverzerrt.
- Crunch besitzt kontrollierte harmonische Verzerrung.
- High Gain funktioniert ohne numerische Instabilität.
- Bass-Modus erhält ausreichenden Tieftonanteil.
- Tone Stack verändert das Spektrum erwartungsgemäß.
- Sag reagiert dynamisch auf Eingangspegel.
- Gain-Staging bleibt kontrollierbar.

### Fertig, wenn

Die vier Amp-Modi tatsächlich unterschiedliche Verstärkerarchitekturen besitzen und nicht bloß vier verschiedene Gain- oder tanh-Einstellungen sind.

---

## B07 – RESONATOR (`resonator`)

**Aktueller Zustand:** Resonanter Bandpassfilter.

### Ziel

Ein echter tonaler Resonator einschließlich Comb- und Feedback-Verhalten.

### Parameter

- Frequency
- Resonance
- Decay
- Feedback
- Damping
- Spread
- Tune
- Gain
- Mix
- Mode

### Aufgaben

1. Bestehenden Bandpass-Modus erhalten.
2. Comb-Resonator ergänzen.
3. Tune und Frequency sinnvoll verbinden.
4. Feedback-Netzwerk implementieren.
5. Damping hinzufügen.
6. Definiertes Decay-Verhalten entwickeln.
7. Stereo Spread ergänzen.
8. Feedback stabil begrenzen.
9. Tail korrekt melden.
10. Parameter-Smoothing bei Frequenzänderung.
11. Natürliche resonante Ausläufe ermöglichen.

### Tests

- Impulsantwort
- Resonanzfrequenz
- Feedback-Stabilität
- Decay
- Stereo Spread
- Tail
- Schnelle Frequenzmodulation

### Fertig, wenn

Aus einem kurzen Eingangsimpuls reproduzierbare tonale Resonanzen mit kontrollierbarer Abklingzeit entstehen.

---

## B08 – NONLINEAR FILTER (`nlfilter`)

**Aktueller Zustand:** Einfacher gesättigter Tiefpass.

### Zielmodi

- Ladder
- Diode
- Driven State Variable

### Parameter

- Cutoff
- Resonance
- Drive
- Character
- Mode
- Mix

### Aufgaben

1. Aktuellen Filter hinsichtlich Stabilität überprüfen.
2. Sample-Rate-abhängige Koeffizienten absichern.
3. Nyquist-Grenzen korrekt behandeln.
4. Ladder-Modell ergänzen.
5. Diode-Modell ergänzen.
6. Nonlinear SVF ergänzen.
7. Resonanzregelung implementieren.
8. Resonanz-Drive-Interaktion berücksichtigen.
9. Schnelle Cutoff-Modulation stabilisieren.
10. Extreme Drive-Werte testen.
11. Aliasing messen.
12. Berechnungen optimieren.

### Tests

- Cutoff-Sweep
- Maximale Resonanz
- Hohe Sample-Rates
- Modulation während Wiedergabe
- Frequenzgang
- THD
- Stabilität bei hohen Eingangspegeln

### Fertig, wenn

Die Filtermodi deutlich unterschiedliche Charaktere besitzen und über den gesamten vorgesehenen Parameterbereich stabil arbeiten.

---

## B09 – WAVEFOLDER (`wavefold`)

**Aktueller Zustand:** Einfacher Triangle-Fold.

### Parameter

- Drive
- Fold Amount
- Bias
- Symmetry
- Tone
- Mix

### Aufgaben

1. Bestehende Fold-Funktion erhalten.
2. Bias ergänzen.
3. Symmetrie-Regelung hinzufügen.
4. Tonale Nachbearbeitung ermöglichen.
5. Anti-Aliasing überprüfen.
6. Mehrstufiges Folding testen.
7. Gain Matching verbessern.
8. Parameter-Smoothing ergänzen.
9. Transferkurve im NodeFace darstellen.

### Tests

- Symmetrische Faltung
- Asymmetrische Faltung
- Fold-Amount-Sweep
- Bias-Sweep
- Obertonspektrum
- DC
- Aliasing

### Fertig, wenn

Drive, Folding, Symmetrie und Bias voneinander unabhängig den Klang formen.

---

## B10 – RING MODULATOR (`ringmod`)

**Aktueller Zustand:** Sinus-Carrier und Mix.

### Parameter

- Carrier Frequency
- Carrier Shape
- Depth
- Stereo Phase
- External Carrier
- Mix
- Sync

### Aufgaben

1. Bestehende Sinus-Ringmodulation prüfen.
2. Unterschiedliche Carrier-Wellenformen ermöglichen.
3. Bandbegrenzte Oszillatoren verwenden.
4. Externen Carrier über geeignete Routing-Anschlüsse ergänzen.
5. Stereo-Phase implementieren.
6. Modulationstiefe getrennt vom Wet/Dry-Mix anbieten.
7. BPM-Synchronisation unterstützen.
8. DC und Aliasing kontrollieren.

### Tests

- Erwartete Summen- und Differenzfrequenzen
- Carrier-Frequenz-Sweep
- Stereo-Phase
- Externer Carrier
- Mix-Transparenz
- Sample-Rate-Verhalten

### Fertig, wenn

Der Ringmodulator sowohl intern erzeugte als auch extern zugeführte Trägersignale korrekt verarbeitet.

---

## B11 – FORMANT FILTER (`formant`)

**Aktueller Zustand:** Fehlt.

### Ziel

Ein resonanter Mehrbandfilter zur Vokal- und Formantgestaltung.

### Parameter

- Vowel
- Vowel Morph
- Formant Shift
- Bandwidth
- Resonance
- Drive
- Mix

### Aufgaben

1. Neuen nativen Block implementieren.
2. Mehrere Formantfilter kombinieren.
3. Vokalmodelle A/E/I/O/U anbieten.
4. Kontinuierliches Morphing zwischen Vokalen.
5. Formant Shift ohne ungültige Frequenzen.
6. Resonanz und Bandbreite getrennt regeln.
7. Modulation aller relevanten Parameter ermöglichen.
8. Stereo-Verhalten prüfen.
9. Vokalvisualisierung im Inspect-Panel integrieren.
10. Presets für Vocals, Growls und Bass erstellen.

### Tests

- Resonanzfrequenzen
- Vowel-Wechsel
- Morph-Glättung
- Resonanzmaximum
- Hohe Drive-Werte
- Stereo
- Numerische Stabilität

### Fertig, wenn

Zwischen den Vokalformen ohne hörbare Sprünge moduliert werden kann und die vorgesehenen Resonanzbereiche korrekt entstehen.

---

# 4. PHASE C – MODULATIONSBLÖCKE

Erst die gemeinsame Modulationsarchitektur aus A03 fertigstellen, dann diese Module nacheinander integrieren.

## C01 – STEP SEQUENCER (`stepseq`)

**Aktueller Zustand:** Vier-Schritt-Sequencer.

### Ziel

Vollständiger 1–64-Step-Modulationssequencer.

### Funktionen

- 1 bis 64 Schritte
- BPM-Sync
- Frei einstellbare Rate
- Step-Werte
- Gate Length
- Glide
- Swing
- Probability
- Step Mute
- Forward
- Reverse
- Ping-Pong
- Random
- Seed
- Reset Mode

### Aufgaben

1. Festen Vier-Step-Code durch dynamisch konfigurierbare, begrenzte Step-Anzahl ersetzen.
2. Speicher für maximal 64 Steps vorbereiten.
3. Step-Werte persistent speichern.
4. Audio- und Modulationsverarbeitung korrekt trennen.
5. Host-BPM und Transportposition unterstützen.
6. Kein Drift bei wiederholtem Stop/Start.
7. Swing implementieren.
8. Step Probability mit reproduzierbarem Seed ergänzen.
9. Glide implementieren.
10. Richtung und Step-Reset unterstützen.
11. Mehrere Zielparameter gleichzeitig modulieren.
12. Interaktiven Step-Editor im Inspect-Panel entwickeln.
13. Live-Position im Circuit darstellen.

### Tests

- 1/4/16/32/64 Steps
- BPM-Wechsel
- Transport-Seeking
- Swing
- Random
- Probability
- Glide
- Sample-Rate-Unabhängigkeit
- Persistenz
- Gleichzeitige Modulation mehrerer Ziele

### Fertig, wenn

Ein 64-Step-Sequencer mehrere echte DSP-Parameter synchron zur DAW modulieren kann und nach dem Laden denselben Zustand besitzt.

---

## C02 – ADSR (`adsr`)

**Aktueller Zustand:** Grundlegende vierphasige Hüllkurve.

### Ziel

Vollständig triggerbare, präzise Hüllkurve.

### Parameter

- Attack
- Decay
- Sustain
- Release
- Velocity
- Retrigger
- Gate Source

### Aufgaben

1. Bestehende State Machine überprüfen.
2. Die aktuelle Attack- und Decay-Berechnung untersuchen: Eine asymptotische Annäherung darf nicht zu einem unerreichbaren Zustandswechsel führen.
3. Definierte Phasenwechsel implementieren.
4. MIDI-Gate unterstützen.
5. Audio-Trigger unterstützen.
6. Retrigger-Regeln definieren.
7. Velocity berücksichtigen.
8. Gate-Länge korrekt behandeln.
9. Kontinuierliche Modulations-Lane erzeugen.
10. Hüllkurve im Inspector anzeigen.

### Tests

- Kurzer Trigger
- Langer Trigger
- Retrigger während Attack
- Retrigger während Release
- Sustain 0
- Sustain 1
- Attack-Minimum und -Maximum
- Gate-Wechsel innerhalb eines Audiobuffers

### Fertig, wenn

Alle vier Phasen zeitlich korrekt erreicht werden und Trigger bei jeder unterstützten Buffergröße funktionieren.

---

## C03 – SAMPLE & HOLD (`samplehold`)

**Aktueller Zustand:** Einfaches periodisches Sampling.

### Parameter

- Rate
- Sync
- Hold
- Slew
- Quantize
- Mix

### Aufgaben

1. Bestehendes Sample-Hold überprüfen.
2. Modulationsinput statt unklarer Audiokanal-Semantik unterstützen.
3. BPM-Sync ergänzen.
4. Slew hinzufügen.
5. Quantisierung ergänzen.
6. Definiertes Verhalten bei Rate 0.
7. Stereo-/Mono-Verhalten festlegen.
8. Host-Sync überprüfen.

### Tests

- Halteintervalle
- Sync
- Quantisierung
- Slew
- Blockgrößen-Unabhängigkeit
- Reset

### Fertig, wenn

Werte in reproduzierbaren Abständen übernommen und bis zum nächsten Sampling stabil gehalten werden.

---

## C04 – RANDOM GENERATOR (`random`)

**Aktueller Zustand:** Pseudozufallswerte mit festem Seed.

### Parameter

- Seed
- Rate
- Range Min/Max
- Shape
- Smooth
- Bipolar
- Sync

### Aufgaben

1. Benutzerkonfigurierbaren Seed implementieren.
2. Deterministischen Reset ermöglichen.
3. Geglättete Zufallsverläufe ergänzen.
4. Step- und Smooth-Random anbieten.
5. Bipolare/unipolare Skalierung unterstützen.
6. Host-Sync implementieren.
7. Verhalten bei Min größer Max definieren.
8. Mehrere unabhängige Random-Instanzen ermöglichen.

### Tests

- Gleicher Seed ergibt gleiche Folge.
- Unterschiedlicher Seed verändert Folge.
- Begrenzte Ausgabewerte
- Sync
- Smoothing
- Reset
- Mehrfachinstanzen

### Fertig, wenn

Random-Modulation reproduzierbar, speicherbar und unabhängig zwischen mehreren Instanzen ist.

---

## C05 – ENVELOPE TRIGGER (`envtrigger`)

**Aktueller Zustand:** Threshold und Hold.

### Parameter

- Threshold
- Sensitivity
- Hysteresis
- Holdoff
- Hold Time
- Level
- Detector Source

### Aufgaben

1. Audio-Transientenerkennung verbessern.
2. Hysterese implementieren.
3. Doppeltrigger verhindern.
4. Holdoff implementieren.
5. Trigger-Puls zeitlich stabil gestalten.
6. Peak/RMS-Quelle unterstützen, wenn sinnvoll.
7. Modulationsausgang integrieren.
8. Trigger-Visualisierung hinzufügen.

### Tests

- Ein Impuls erzeugt genau einen Trigger.
- Doppelte Transienten
- Threshold-Grenzfälle
- Hysterese
- Holdoff
- Stille
- Wiederholte Hits

### Fertig, wenn

Ein einzelner Transient genau den erwarteten Trigger erzeugt, ohne unkontrollierte Mehrfachauslösung.

---

## C06 – MODULATION MATH (`modmath`)

**Aktueller Zustand:** Audio-L/R-Kanäle werden als mathematische Eingänge verwendet.

### Ziel

Mathematische Verarbeitung echter Modulationssignale.

### Funktionen

- Add
- Subtract
- Multiply
- Divide
- Min
- Max
- Absolute
- Invert
- Clamp
- Remap
- Quantize
- Slew

### Aufgaben

1. Zwei unabhängige Modulations-Eingänge implementieren.
2. Audio-L/R als implizite Quellen entfernen, sofern nicht ausdrücklich als Audio-Operation gewählt.
3. Alle Operationen implementieren.
4. Division durch 0 absichern.
5. Clamp und Remap unterstützen.
6. Unipolare und bipolare Signale korrekt behandeln.
7. Mehrere Ausgänge ermöglichen, sofern die Grapharchitektur dies unterstützt.
8. Modulations-Output im Circuit korrekt routen.
9. Wertebereiche im Inspector darstellen.

### Tests

- Für jede mathematische Operation
- Grenzwerte
- Division durch Null
- Bipolar/Unipolar
- Gleichzeitige Quellen
- Reset
- Zyklische Verbindungen ablehnen

### Fertig, wenn

Zwei unabhängige Modulationsquellen tatsächlich mathematisch kombiniert und an weitere Zielparameter weitergegeben werden.

---

## C07 – MODULATION MIXER (`modmix`)

**Aktueller Zustand:** Zwei audiokanalbasierte Eingänge mit Gain-Gewichtung.

### Parameter

- Input A Level
- Input B Level
- Master Mix
- Offset
- Polarity
- Clamp
- Normalize

### Aufgaben

1. Echte Modulations-Eingänge A und B integrieren.
2. Gewichte unabhängig steuern.
3. Bipolar- und Unipolarverhalten berücksichtigen.
4. Offset ergänzen.
5. Clamp ergänzen.
6. Optional Normalisierung anbieten.
7. Definiertes Verhalten bei fehlender Quelle.
8. Visualisierung der Signalanteile ermöglichen.

### Tests

- Nur A
- Nur B
- A+B
- Gegenphasige Quellen
- Clamp
- Normalisierung
- Preset-Restore

### Fertig, wenn

Mehrere Quellen ohne Umweg über Audio-Kanäle kontrolliert kombiniert werden können.

---

## C08 – MSEG (`mseg`)

**Aktueller Zustand:** Fehlt.

### Ziel

Vollständiger Multi-Segment-Envelope-Generator.

### Funktionen

- Frei platzierbare Stützpunkte
- Lineare und gekrümmte Segmente
- Loop Start/End
- Sustain
- One-Shot
- Freerun
- Retrigger
- BPM-Sync
- Trigger Source
- Modulation Output

### Aufgaben

1. Begrenztes Punktmodell entwickeln.
2. Segmentzeiten definieren.
3. Kurveninterpolation implementieren.
4. Loop-Bereiche unterstützen.
5. Sustain und Release integrieren.
6. MIDI- und Audio-Trigger ermöglichen.
7. Host-BPM berücksichtigen.
8. Grafischen MSEG-Editor entwickeln.
9. Stützpunkte verschieben, erstellen und löschen können.
10. Zustand vollständig serialisieren.
11. Echtzeitverarbeitung ohne Allokationen implementieren.
12. Verschiedene Segmentkurven testen.

### Tests

- Zwei-Punkt-Hüllkurve
- Mehrere Segmente
- Loop
- Sustain
- Retrigger
- BPM-Sync
- Extreme Segmentzeiten
- Preset-Restore
- Unterschiedliche Buffergrößen

### Fertig, wenn

Eine beliebige gültige MSEG-Kurve mit definierter Zeitbasis reproduzierbar ausgegeben und als Modulationsquelle verwendet werden kann.

---

# 5. PHASE D – ZEITBASIERTE EFFEKTE

## D01 – TAPE STOP (`tapestop`)

**Aktueller Zustand:** Variable Abspielgeschwindigkeit vorhanden. Teilreparaturen aus PR #324 integriert.

### Parameter

- Stop Time
- Restart Time
- Curve
- Trigger
- Depth
- Mix
- Sync

### Aufgaben

1. Rate-Interpolation überprüfen.
2. Sicherstellen, dass die Readerate nicht versehentlich jedes Sample zurückgesetzt wird.
3. Definierte Stop- und Restart-Zustände implementieren.
4. Triggerquelle unterstützen.
5. Tempo-Synchronisation ergänzen.
6. Variablen Read-Head korrekt interpolieren.
7. Stereo-Unabhängigkeit sicherstellen.
8. Pufferüberlauf und Wraparound behandeln.
9. Knackfreie Übergänge ermöglichen.
10. Puffer nicht in Echtzeit-Reset-Funktionen neu allokieren.

### Tests

- 100 ms Stop
- 1 s Stop
- 5 s Stop
- Wiederanlauf
- Trigger
- Stereo
- Buffer-Wrap
- Stille
- Schnelle Parameteränderungen

### Fertig, wenn

Der Effekt wirklich über die eingestellte Zeit abbremst und anschließend definiert wieder anfahren kann.

---

## D02 – REVERSE DELAY (`reversedelay`)

**Aktueller Zustand:** Gepufferte Rückwärtslese-Funktion. Stereo wurde im Reparaturbranch verbessert.

### Parameter

- Time
- Feedback
- Reverse Window
- Crossfade
- Sync
- Mix

### Aufgaben

1. Echte rückwärts abgespielte Zeitfenster implementieren.
2. Schreib- und Leseposition unabhängig verwalten.
3. Fenster an Segmentgrenzen überblenden.
4. Stereo-Puffer strikt getrennt halten.
5. Feedback sauber einspeisen.
6. Delayzeit in Millisekunden korrekt behandeln.
7. BPM-Sync ergänzen.
8. Wraparound ohne Diskontinuität sicherstellen.
9. Korrekte Tail-Berechnung.
10. Dry/Wet und Latenz prüfen.

### Tests

- Testsignal mit eindeutigem zeitlichem Verlauf
- Rückwärts ausgegebene Sample-Reihenfolge
- 250 ms Fenster
- Crossfade-Grenze
- Feedback
- Stereo-Isolation
- Tail
- Host-Sync

### Fertig, wenn

Die tatsächliche Audiosequenz innerhalb definierter Fenster rückwärts wiedergegeben wird und keine bloße zeitversetzte Delay-Ausgabe entsteht.

---

## D03 – GRANULAR PROCESSOR (`granular`)

**Aktueller Zustand:** Fehlt.

### Parameter

- Grain Size
- Density
- Position
- Scatter
- Pitch
- Reverse Probability
- Stereo Spread
- Feedback
- Freeze
- Mix

### Aufgaben

1. Vorallokierten Audio-Ringbuffer entwickeln.
2. Begrenztes Grain-Pool-System implementieren.
3. Unterschiedliche Grain-Fenster anbieten.
4. Grain-Position und Scatter unterstützen.
5. Pitch pro Grain implementieren.
6. Reverse Probability unterstützen.
7. Stereo Spread entwickeln.
8. Freeze-Modus implementieren.
9. Dichte und aktive Grain-Anzahl begrenzen.
10. Stabiles Feedback ermöglichen.
11. CPU- und Speicherbudgets dokumentieren.
12. Grain-Verteilung im Inspect-Panel visualisieren.

### Tests

- Einzelnes Grain
- Mehrere Grains
- Maximale Dichte
- Freeze
- Pitch
- Reverse
- Stereo
- Puffergrenzen
- CPU-Stresstest
- Keine Allokationen beim Processing

### Fertig, wenn

Der Granular Processor auch bei hohen Grain-Dichten kontrolliert arbeitet, reproduzierbar klingt und keine Echtzeitfehler verursacht.

---

## D04 – SPECTRAL FREEZE (`spectralfreeze`)

**Aktueller Zustand:** Fehlt.

### Parameter

- Freeze
- FFT Size
- Smear
- Shift
- Decay
- Mix

### Aufgaben

1. Geeignete STFT-Architektur auswählen.
2. Fensterung implementieren.
3. Overlap-Add-Rekonstruktion ergänzen.
4. Freeze von Spektralmagnituden ermöglichen.
5. Phasenbehandlung sauber lösen.
6. Smear entwickeln.
7. Spektrales Shifting implementieren.
8. Decay hinzufügen.
9. FFT-Pläne außerhalb des Audiothreads vorbereiten.
10. Latenz korrekt melden.
11. Freeze-Status im Circuit anzeigen.
12. Speicherverbrauch begrenzen.

### Tests

- Identitätsdurchlauf ohne Freeze
- Freeze nach Impuls
- Freeze bei komplexem Signal
- Spektrale Stabilität
- Phasenkontinuität
- Latenz
- FFT-Größenwechsel
- CPU
- Numerische Stabilität

### Fertig, wenn

Ein eingefrorenes Spektrum ohne unkontrollierte Artefakte gehalten und gezielt weiterverarbeitet werden kann.

---

## D05 – CONVOLUTION REVERB (`convreverb`)

**Aktueller Zustand:** Kein eigenständiger allgemeiner Convolution-Reverb-Block. Cabinet IR existiert.

### Parameter

- IR Select
- Predelay
- Wet
- Dry
- Gain
- Tone
- Stereo Mode

### Aufgaben

1. Bestehende IR-Engine analysieren.
2. Prüfen, ob sich deren Architektur sinnvoll erweitern lässt.
3. Längere Raumimpulsantworten unterstützen.
4. Partitioned Convolution bei Bedarf implementieren.
5. Stereo-IRs verarbeiten.
6. Predelay implementieren.
7. IR-Laden asynchron und außerhalb des Audiothreads halten.
8. Speichergrenzen definieren.
9. IR-Austausch ohne gefährliche Zustandswechsel ermöglichen.
10. Latenz und Tail melden.
11. Preset-Restore und Dateireferenzen absichern.
12. Cabinet IR weiterhin unabhängig nutzbar halten.

### Tests

- Delta-IR
- Kurze IR
- Lange IR
- Stereo-IR
- Predelay
- Latenz
- Tail
- Dateiwechsel
- Fehlerhafte Datei
- Speicherlimit

### Fertig, wenn

Allgemeine Raumimpulsantworten korrekt verarbeitet werden können, ohne Cabinet-IR-Funktionalität und Echtzeitverhalten zu beschädigen.

---

# 6. PHASE E – ERWEITERTE DYNAMIK

## E01 – DYNAMIC EQ (`dynamiceq`)

**Aktueller Zustand:** Fehlt.

### Parameter je Band

- Frequency
- Q
- Static Gain
- Dynamic Range
- Threshold
- Ratio
- Attack
- Release
- Detector Source

### Aufgaben

1. Vorhandenen EQ und bestehende Dynamikdetektoren prüfen.
2. Dynamische EQ-Bänder implementieren.
3. Peak/RMS-Detektor unterstützen.
4. Frequenzabhängige Gain-Reduktion implementieren.
5. Sidechain-Anbindung ermöglichen.
6. Attack/Release korrekt glätten.
7. Mehrere Bänder unterstützen.
8. CPU-effiziente Koeffizientenverarbeitung sicherstellen.
9. Statische und dynamische Kurve im Inspector darstellen.
10. Gain-Reduction-Telemetrie anzeigen.

### Tests

- Statischer EQ
- Dynamischer EQ
- Kein Signal
- Signal oberhalb Threshold
- Sidechain
- Mehrere Bänder
- Sweep
- Gain Reduction
- Stereo
- CPU

### Fertig, wenn

Ein Frequenzband abhängig von einem definierten Detektorsignal dynamisch verstärkt oder abgesenkt werden kann.

---

## E02 – MULTIBAND DYNAMICS (`multicomp`)

**Aktueller Zustand:** Fehlt als frei konfigurierbarer Block; OTT existiert bereits.

### Funktionen

- 2 bis 4 Frequenzbänder
- Kompression
- Expansion
- Threshold pro Band
- Ratio pro Band
- Attack/Release pro Band
- Makeup pro Band
- Solo/Mute pro Band
- Global Mix

### Aufgaben

1. Vorhandene Crossover-Implementierung wiederverwenden.
2. Mehrere unabhängige Dynamikprozessoren kombinieren.
3. Kompressions- und Expansionsmodi anbieten.
4. Bänder phasenkorrekt rekonstruieren.
5. Getrennte Gain-Reduction-Werte messen.
6. Band-Solo und Band-Mute realisieren.
7. Parameter pro Band im Inspector darstellen.
8. Crossover-Frequenzen frei einstellen.
9. Stereo-Link unterstützen.
10. Globalen Dry/Wet-Mix sauber integrieren.

### Tests

- Bypass beziehungsweise Unity-Rekonstruktion
- Zwei Bänder
- Vier Bänder
- Crossover-Grenzen
- Kompression je Band
- Expansion je Band
- Stereo
- PDC
- CPU

### Fertig, wenn

Jedes Band unabhängig dynamisch verarbeitet und danach ohne unbeabsichtigte Frequenzgang- oder Phasenfehler rekonstruiert werden kann.

---

# 7. PHASE F – BESTANDSMODULE ÜBERPRÜFEN

Diese Phase darf bestehende Klangalgorithmen nicht ohne nachgewiesenen Fehler verändern.

Die folgenden Module sind bereits vorhanden. Prüfe sie jeweils einzeln auf vollständige Bedienbarkeit, Parametertreue, Stabilität und Tests.

## F01 – Compressor (`comp`)

Prüfen:

- Peak und RMS
- Threshold
- Ratio
- Knee
- Attack/Release
- Makeup
- Detector HPF
- Lookahead
- Parallel Mix
- Sidechain
- Gain-Reduction-Anzeige
- Stereo-Link
- Latenz

**Abnahme:** Gemessene Kompressionskennlinie und Dynamikzeiten entsprechen den eingestellten Parametern.

## F02 – Noise Gate (`noisegate`)

Prüfen:

- Threshold
- Attack
- Hold
- Release
- Hysteresis
- Range
- Ceiling
- Stereo-Link
- Sidechain, soweit unterstützt

**Abnahme:** Kein Chattering am Threshold und korrektes Öffnen/Schließen.

## F03 – Limiter (`limit`)

Prüfen:

- Ceiling
- Lookahead
- Release
- Stereo-Link
- Oversampling
- Peak und True Peak
- Latenz
- Bypass
- Host-PDC

**Abnahme:** Definierter Ceiling und korrekte Latenz über alle unterstützten Sample-Rates.

## F04 – OTT (`ott`)

Prüfen:

- Crossover-Frequenzen
- Upward Compression
- Downward Compression
- Depth
- Time
- Band Gain
- Stereo
- Phasenkorrekte Summierung
- Dry/Wet
- Gain-Reduction

**Abnahme:** Alle Bänder wirken wie vorgesehen und summieren sich kontrolliert.

## F05 – Parametric EQ (`eq`)

Prüfen:

- Peak
- Shelf
- Highpass/Lowpass
- Notch
- Frequency
- Q
- Gain
- Kanalzuordnung
- Automatisierung
- Frequenzgang-Darstellung

**Abnahme:** Gemessener Frequenzgang stimmt mit den Einstellungen und der UI-Darstellung überein.

## F06 – Filter (`filter`)

Prüfen:

- Filtermodi
- Cutoff
- Resonance
- Kanalzuordnung
- Nyquist-Grenzen
- Dynamische Modulation
- Koeffizientenglättung

**Abnahme:** Stabile Verarbeitung bei schnellen Cutoff-Änderungen.

## F07 – De-esser (`deesser`)

Prüfen:

- Detektor-Frequenz
- Threshold
- Amount
- Attack
- Release
- Listen-Modus
- Split-Band-Verarbeitung
- Stereo

**Abnahme:** Zielgerichtete Zischlaut-Absenkung ohne unerwünschte Breitbandkompression.

## F08 – Transient Shaper (`transient`)

Prüfen:

- Attack
- Sustain
- Fast/Slow Detector
- Parallel Mix
- Stereo
- Verhalten bei unterschiedlichen Eingangspegeln

**Abnahme:** Attack und Sustain beeinflussen die vorgesehenen Signalanteile getrennt.

## F09 – Delay (`delay`)

Prüfen:

- Zeit in Millisekunden
- BPM-Sync
- Feedback
- Mix
- Pingpong
- Wow
- Flutter
- Tail
- Modulation
- Stereo

**Abnahme:** Exakte Delays und stabile Feedback-Ketten.

## F10 – Reverb (`reverb`)

Prüfen:

- Size
- Decay
- Damping
- Mix
- Stereo
- Tail
- Energieverlauf
- Presets mit extremen Parameterwerten

**Abnahme:** Definierte Abklingzeiten und keine unkontrollierte Energiekonzentration.

## F11 – Chorus (`chorus`)

Prüfen:

- Rate
- Depth
- Delay
- Voices
- Width
- Mix
- Stereo-Modulation
- Clickfreiheit

**Abnahme:** Stabile Stimmenmodulation und korrektes Stereoverhalten.

## F12 – Flanger (`flanger`)

Prüfen:

- Rate
- Depth
- Delay
- Feedback
- Invert
- Mix
- Through-Zero-Verhalten
- Stabilität

**Abnahme:** Korrekte modulierte Delayzeit und kontrolliertes Feedback.

## F13 – Phaser (`phaser`)

Prüfen:

- Stufen
- Rate
- Depth
- Center
- Feedback
- Mix
- Allpass-Verhalten

**Abnahme:** Plausible Phasenkerben und kein instabiles Resonanzverhalten.

## F14 – Pitch Shifter (`pitch`)

Prüfen:

- Semitones
- Formant
- Mix
- Stereo
- FFT
- Latenz
- Phasenverhalten
- Signalqualität bei großen Intervallen

**Abnahme:** Erwartete Tonhöhenverschiebung und korrekte Latenzmeldung.

## F15 – Octaver (`octaver`)

Prüfen:

- Sub
- Up
- Mix
- Tone
- Threshold
- Tracking
- Tiefe Eingangssignale
- Stereo

**Abnahme:** Stabile Oktavenerzeugung über den vorgesehenen Eingangstonbereich.

## F16 – Vocoder (`vocoder`)

Prüfen:

- Bandanzahl
- Carrier
- Modulator
- Host-Sidechain
- Attack
- Release
- Band-Q
- Mix
- Sprachverständlichkeit
- Stereo

**Abnahme:** Getrennte Carrier- und Modulatorsignale werden tatsächlich korrekt kombiniert.

## F17 – Bitcrush (`bitcrush`)

Prüfen:

- Bit Depth
- Recovery Tone
- Mix
- Quantisierungsverhalten
- Stereo
- Gain-Staging

**Abnahme:** Korrekte Bittiefenreduktion ohne unerwartete Kanalfehler.

## F18 – Cabinet IR (`ir`)

Prüfen:

- IR-Laden
- Stereo
- Gain
- Mix
- Speichergrenzen
- Latenz
- Preset-Restore
- Ungültige IRs
- Ressourcenfreigabe

**Abnahme:** Zuverlässige Cabinet-Convolution und keine Audio-Thread-Allokationen.

## F19 – LFO (`osc`)

Prüfen:

- Wellenformen
- Rate
- Depth
- Sync
- Zielbindung
- Phase
- Stereo
- Modulations-Lane
- Transport-Reset

**Abnahme:** Samplegenaue beziehungsweise dokumentiert zeitdiskrete Modulation ohne Drift.

## F20 – Envelope Follower (`env`)

Prüfen:

- Peak/RMS
- Attack
- Release
- Hold
- Min/Max
- Invert
- Source
- Unipolar/Bipolar
- Modulations-Lane

**Abnahme:** Modulationswert folgt dem Eingang mit korrektem zeitlichem Verhalten.

## F21 – Utility (`utility`)

Prüfen:

- Gain
- Pan
- Polarity
- Stereo
- Parameter-Smoothing
- Linearität
- Unity Gain

**Abnahme:** Transparente Verarbeitung bei neutralen Einstellungen.

## F22 – Width (`widen`)

Prüfen:

- Width
- Delay
- Bass-Mono
- Stereo-Korrelation
- Phasenverhalten
- Mono-Kompatibilität

**Abnahme:** Die Verbreiterung verändert nicht unkontrolliert die Mono-Kompatibilität.

## F23 – Formula Stage (`stage`) / Custom

Prüfen:

- DSL-Formelauswertung
- Parameterbindung
- Eingangsvariablen
- Bereichsbegrenzung
- Performance
- Fehlerbehandlung
- Unbekannte Funktionen
- NaN/Inf
- Realtime-Sicherheit

**Abnahme:** Formeln werden deterministisch, sicher und ohne Audiothread-Parsing verarbeitet.

## F24 – Routing, Busse und I/O

Prüfen:

- Mid/Side Split/Join
- Left/Right Split/Join
- Multiband Split
- Bus
- Join
- Send
- In
- Out
- Sidechain
- Meter

**Abnahme:** Kein unbeabsichtigter Signalverlust, korrekte Kanalzuordnung, saubere Bus-Latenz und kein fehlerhaftes Modulationsrouting.

Bei dieser Phase gilt: **Keine komplette Neuimplementierung, wenn die vorhandene Funktion bereits korrekt ist.** Dokumentiere bestandene Tests und behebe ausschließlich reproduzierbare Probleme.

---

# 8. PHASE G – UI, INSPECT UND CIRCUIT

Nach jeder Modulimplementierung ist die Bedienbarkeit verpflichtend. Diese Phase überprüft zusätzlich alle Module gemeinsam.

## 8.1 Einheitliche Bedienung

Jeder numerische Parameter benötigt einen sinnvollen Regler.

- Logarithmisch für Frequenzen
- Sinnvolle Skalierung für dB
- Eindeutige Zeiten in ms beziehungsweise s
- Dropdown für diskrete Modi
- Numerische Präzisionseingabe
- Standardwert zurücksetzen
- Sichtbare Modulationsbindung
- Korrekte Werte bei Parameteränderungen

## 8.2 Komplexe Module

Implementiere spezielle Editoren für:

- Shaper-Kurven
- Step-Sequencer
- MSEG
- Granular
- Dynamic EQ
- Multiband Dynamics

Benutze bestehende UI-Bausteine, sofern sie geeignet sind.

## 8.3 Platinenlook erhalten

NEUROKORE muss die bestehende Computerchip-Ästhetik behalten.

Erhalte:

- Punktraster
- Chipgehäuse
- Metallkontakte
- Warn- und Funktionslabels
- Statusanzeigen
- Chip-Namen
- Anschlussgeometrie
- PCB-Traces
- Mute/Solo
- Multi-Select
- Pan und Zoom

Neue Module müssen sich visuell in diese Sprache einfügen.

**Keine generischen Karten, Rack-Reihen oder zweckfremden Web-Dashboard-Komponenten.**

## 8.4 Darstellungen

Closed NodeFaces sollen sinnvolle, modulabhängige Visualisierungen zeigen.

Nur reale Parameter oder tatsächliche Telemetrie darstellen.

Keine Fake-Pegel, zufälligen Spektren oder vorgetäuschte Audioaktivität.

## 8.5 Performance

Prüfe Circuit-Platinen mit:

- 6 Chips
- 20 Chips
- 50 Chips
- 100 Chips
- 200 Chips

Bei mehreren Zoomstufen und DPI-Einstellungen.

Das Einfügen neuer Module darf die bestehenden optimierten Leitungs-, Impuls- und Glow-Systeme nicht beeinträchtigen.

---

# 9. PHASE H – FACTORY PRESETS

Erstelle nach Fertigstellung des jeweiligen Moduls zunächst drei sinnvolle Test-Patches.

Nach vollständiger technischer Implementierung können daraus Factory-Presets werden.

Kategorien:

- Mixing
- Mastering
- Drums
- Bass
- Guitar
- Vocals
- Industrial
- Techno
- Drum & Bass
- Experimental

Die Presets müssen unterschiedliche musikalische Aufgaben erfüllen.

Teste jedes Preset mit sinnvoll ausgewählten Eingangssignalen.

Prüfe:

- Kein Clipping außerhalb beabsichtigter Effekte
- Kein unnötiger Gain-Sprung
- Sinnvolle Makrobindungen
- Richtige DSL
- Gültige Blockreihenfolge
- Tatsächlich aktive Effekte
- Funktionierendes Preset-Restore
- Keine ungültigen Ressourcen
- Keine toten Parameter
- Keine ungewollte Stille

Lösche keine bisherigen Factory-Presets.

---

# 10. PHASE I – MODULÜBERGREIFENDE QUALITÄTSSICHERUNG

## 10.1 Native Regression

Teste:

- Jedes Modul einzeln
- Mehrere Module in Serie
- Mehrere Module parallel
- Mid/Side-Ketten
- Multiband-Ketten
- Mehrere Busse
- Mehrere Modulatoren
- Kombinationen mit Sidechain
- Sehr große Patchgraphen

## 10.2 Echtzeitprüfung

Überprüfe:

- Audio-Thread-Allokationen
- Buffergrößen
- Reinitialisierung
- Thread-Synchronisierung
- Presetwechsel
- Sample-Rate-Wechsel
- DSP-State-Austausch
- Speicherfreigabe
- CPU-Ausreißer

## 10.3 Klangvalidierung

Erstelle reproduzierbare Referenzrenders für:

- Sinus
- Impuls
- Rauschen
- Sweep
- Stereo-Testsignale
- Transienten
- Bass-Signale
- Komplexe Musikbeispiele

Messgrößen:

- Peak
- RMS
- DC-Offset
- Frequenzgang
- THD
- Alias-Komponenten
- Stereo-Korrelation
- Latenz
- Tail
- CPU-Kosten

Nicht jedes Modul benötigt jede Messgröße, aber jede Messung muss zum behaupteten DSP-Verhalten passen.

## 10.4 Hostvalidierung

Wenn möglich:

- Windows VST3
- Windows Standalone
- macOS AU/VST3
- Linux
- pluginval
- DAW-State-Restore
- Preset-Restore
- Host-Bypass
- Plugin-Neuinstanziierung
- Offline-Rendering

Nicht ausgeführte Tests als `NOT RUN` ausweisen.

---

# 11. VERBINDLICHE UMSETZUNGSREIHENFOLGE

Die folgende Reihenfolge ist verbindlich, sofern sich nicht durch eine nachgewiesene technische Abhängigkeit eine andere Reihenfolge ergibt.

| Reihenfolge | Arbeitspaket | Status zu Beginn |
|---|---|---|
| A01 | Baseline und Git-Prüfung | Erneut prüfen |
| A02 | Native DSP-Testpipeline | Vorhanden, ergänzen |
| A03 | Gemeinsame Modulationsarchitektur | Unvollständig |
| B01 | Clipper | Weitgehend fertig, validieren |
| B02 | Saturator | Repariert, validieren |
| B03 | Frequency Shifter | Repariert, validieren |
| B04 | Decimator | Verbessern |
| B05 | Custom Waveshaper | Deutlich erweitern |
| B06 | Amp / Preamp | Neu aufbauen unter Erhalt der Legacy-Funktion |
| B07 | Resonator | Erweitern |
| B08 | Nonlinear Filter | Erweitern |
| B09 | Wavefolder | Erweitern |
| B10 | Ring Modulator | Erweitern |
| B11 | Formant Filter | Neu |
| C01 | Step Sequencer | Erweitern |
| C02 | ADSR | Korrigieren und erweitern |
| C03 | Sample & Hold | Erweitern |
| C04 | Random Generator | Erweitern |
| C05 | Envelope Trigger | Erweitern |
| C06 | Modulation Math | Architektur korrigieren |
| C07 | Modulation Mixer | Architektur korrigieren |
| C08 | MSEG | Neu |
| D01 | Tape Stop | Reparaturen prüfen und erweitern |
| D02 | Reverse Delay | Reparaturen prüfen und vervollständigen |
| D03 | Granular Processor | Neu |
| D04 | Spectral Freeze | Neu |
| D05 | Convolution Reverb | Neu beziehungsweise IR-Engine erweitern |
| E01 | Dynamic EQ | Neu |
| E02 | Multiband Dynamics | Neu |
| F01–F24 | Bestandsmodule | Einzeln validieren und gezielt reparieren |
| G | Vollständiger UI-Audit | Offen |
| H | Factory-Preset-Ausbau | Offen |
| I | Gesamte DSP-, Host- und Performance-Abnahme | Offen |

**Abweichung zur reinen Tabellenreihenfolge:** Die Architekturarbeiten in A03 dürfen notwendige vorbereitende Änderungen durchführen. Die Module selbst werden trotzdem jeweils separat abgeschlossen.

Wenn eine Abhängigkeit verhindert, dass ein Modul abgeschlossen werden kann, markiere es als `BLOCKED`, dokumentiere die konkrete technische Ursache und behebe diese Abhängigkeit, bevor du das Modul fortsetzt.

---

# 12. PULL-REQUEST-REGELN

Für jedes Modul einen eigenen PR.

Beispiel:

`feat/dsp-decimator-complete`

`feat/dsp-shaper-editor`

`feat/dsp-amp-architecture`

`feat/mod-stepseq-64`

`feat/dsp-formant-filter`

Jeder PR enthält:

1. Ziel und Ausgangslage
2. Geänderte native DSP-Funktionen
3. Geänderte Parameter
4. UI-/DSL-Änderungen
5. Kompatibilitätsbetrachtung
6. Reproduzierbare Tests
7. Reale Messergebnisse
8. Bekannte Einschränkungen
9. Vorher-/Nachher-Vergleich
10. Erfüllte Abnahmekriterien

### Merge-Regeln

- Kein Merge bei fehlgeschlagener relevanter CI.
- Kein Merge bei bekannten Audio-Datenkorruptionen.
- Kein Merge bei bekannten Audio-Thread-Allokationen.
- Kein Merge bei nicht funktionierenden neuen Parametern.
- Kein Merge, wenn das Modul nur einen Platzhalter enthält.
- Kein Merge bei neuen Preset-Inkompatibilitäten.
- Kein eigenmächtiges Ändern der Produktversion.

Einen vollständig validierten PR zur Prüfung bereitstellen. Merges nur nach den geltenden Freigaberegeln des Repositories durchführen.

Nicht durch fehlende Testumgebung verifizierbare Funktionen müssen transparent als ausstehend gekennzeichnet werden.

---

# 13. VERBINDLICHE MODUL-ABNAHME

Für jedes Modul eine eigene Checklist-Datei erstellen.

Pfad beispielsweise:

`docs/evidence/dsp-expansion/modules/clip.md`

Verwende folgendes Schema:

```text
MODULE: clip
STATUS: READY / NEEDS_WORK / BLOCKED / VALIDATION_PENDING

NATIVE DSP
[ ] Algorithmus vollständig
[ ] Alle Parameter wirksam
[ ] Mono
[ ] Stereo
[ ] Sample-Rates
[ ] Realtime-sicher
[ ] Keine NaN/Inf
[ ] Korrekte Latenz
[ ] Korrekte Tail-Semantik

INTEGRATION
[ ] Parser
[ ] DSP Factory
[ ] Signalgraph
[ ] Block-Katalog
[ ] Circuit Add-Menü
[ ] Inspect
[ ] NodeFace
[ ] Terminal
[ ] Makros / Modulation
[ ] Persistenz

TESTS
[ ] Native Unit Tests
[ ] DSP Behavior Tests
[ ] Parameter Contracts
[ ] Regression
[ ] CPU Benchmark
[ ] Preset Restore
[ ] Host Validation

EVIDENCE
- Commit:
- Pull Request:
- Native Test Result:
- Web Test Result:
- DSP Measurements:
- Remaining Issues:

DECISION
READY / NOT READY
```

Alle Statusangaben müssen durch tatsächliche Nachweise gestützt sein.

---

# 14. FORTSCHRITTSBERICHT NACH JEDEM MODUL

Nach Abschluss eines Moduls diesen Bericht ausgeben:

```text
NEUROKORE DSP EXPANSION

MODULE:
BRANCH:
COMMIT:
PULL REQUEST:

IMPLEMENTIERT
- ...

GEÄNDERTE DATEIEN
- ...

DSP-FUNKTIONEN
- ...

PARAMETER
- ...

CIRCUIT / INSPECT / TERMINAL
- ...

TESTS
Native:
Web:
Regression:
CPU:
pluginval:
DAW:

KOMPATIBILITÄT
Presets:
Host State:
Parameter IDs:
Latenz:

OFFENE PROBLEME
- ...

STATUS
READY / NEEDS_WORK / BLOCKED / VALIDATION_PENDING

NÄCHSTES MODUL
- ...
```

Erfinde keine Messergebnisse.

Beschreibe Fehler präzise und mit reproduzierbaren Informationen.

---

# 15. ABSOLUTE NO-GO-REGELN

- Keine Scheinparameter ohne DSP-Wirkung.
- Keine DSP-Blocks, deren `processBlock()` nur ungeprüft das Eingangssignal zurückgibt, obwohl sie einen Effekt versprechen.
- Keine UI-Elemente ohne echte native Funktion.
- Keine erfundenen Messwerte.
- Keine zufälligen visuellen Audioanzeigen.
- Keine unnötigen Systemneuschreibungen.
- Keine aufwendigen DSP-Allokationen im Audiothread.
- Keine unkontrollierten Speicheranforderungen.
- Keine absichtlich abgeschwächten Tests, um grüne CI zu erzwingen.
- Keine Entfernung bestehender Presets ohne ausdrückliche Freigabe.
- Keine willkürliche Änderung des bestehenden Circuit-Designs.
- Keine Änderungen an Lizenzierung, Website oder sonstigen fachfremden Bereichen.
- Keine gleichzeitige unkontrollierte Bearbeitung mehrerer Module.
- Keine Behauptung, ein Modul sei fertig, solange ein zugesicherter Parameter nicht funktioniert.

---

# 16. ABSCHLIESSENDER ARBEITSAUFTRAG

**BEGINNE JETZT MIT A01.**

Stelle zunächst den aktuellen Stand fest, einschließlich der bereits gemergten Reparaturen aus PR #324.

Arbeite danach strikt in der festgelegten Reihenfolge weiter.

Beende jedes Modul vollständig, bevor das nächste Modul bearbeitet wird.

Nutze echte DSP-Tests und reproduzierbare Messungen. Repariere zuerst den bestehenden Code, statt ihn ohne Not zu ersetzen.

Bewahre die aktuelle Klangqualität, die bestehende Projektkompatibilität und das charakteristische NEUROKORE-Circuit-Design.

Führe die Roadmap so weit eigenständig aus, wie Git-Zugriff, verfügbare Laufzeit und Testumgebung es erlauben. Wenn die Arbeit nicht vollständig in einer Ausführung abgeschlossen werden kann, hinterlasse einen eindeutig dokumentierten, reproduzierbaren Zwischenstand mit dem nächsten konkreten Arbeitsschritt.

**Das Entwicklungsziel ist ein vollständig integrierter, modularer Audioeffekt mit professionellen DSP-Algorithmen, nachvollziehbarer Qualitätssicherung und ohne halbfertige Module.**

Das Modul gilt nicht deshalb als fertig, weil es im Menü steht.

**Es ist erst fertig, wenn es korrekt klingt, korrekt arbeitet, korrekt bedienbar ist, korrekt gespeichert wird und seine zugesicherten Funktionen nachweislich erfüllt.**