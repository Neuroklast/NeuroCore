# NeuroKore – Alleinstellungsmerkmale

Was NeuroKore einzigartig und innovativ macht – im Vergleich zu bestehenden kommerziellen und Open-Source-Audio-Plugins.

---

## 1. Runtime Expression Evaluator mit SIMD, Constant-Folding und CSE

**Was:** Ein vollständiger mathematischer Ausdrucks-Parser der **zur Laufzeit** vom User eingegebene Formeln evaluiert – mit professionellen Compiler-Optimierungen.

**Warum einzigartig:**
- Constant Folding: Konstante Teilausdrücke werden zur Parse-Zeit berechnet (z. B. `2 * pi * 440` → `2764.6`)
- Common Subexpression Elimination (CSE): Identische Teilausdrücke werden nur einmal berechnet
- SIMD-Support: `evaluateBlockSimd()` verarbeitet mehrere Samples gleichzeitig via SSE/AVX
- Die meisten Plugins mit "formel-basiertem Waveshaping" (z. B. Külatronic, Glitch Machines) kompilieren Formeln vor dem Laden. NeuroKore evaluiert **live** mit professioneller Performance.

**Technisch:** AST-Baum mit `std::shared_ptr<Node>`, optimiert durch einen zweiten Pass des Compilers.

---

## 2. Deklarative Audio-DSL ("ShaderToy für Audio")

**Was:** Eine eigene Textsprache zur Beschreibung kompletter Signalketten – ähnlich wie ShaderToy für Grafik-Shader, aber für Audio-Processing.

**Warum einzigartig:**
- Kein Plugin auf dem Markt lässt den User **komplette Signalketten als Text beschreiben**
- Kombination aus `stage` (Formel), `filter`, `comp`, `env`, `osc`, `param` in einem kohärenten Syntax
- Formeln können sich auf Envelope-Follower und LFOs beziehen: `y = tanh(x * a + env1 * 2)`
- **Graph** ist die Platine (Chips einrasten, Kabel ziehen). **Script** ist der Hack derselben Kette als Text
- Das öffnet eine ganz neue Kreativitätsdimension: Musiker und Entwickler können Audio-Effekte durch Text definieren

**Vergleich:**
- SOUL/FAUST: Ähnliches Konzept, aber externe Sprachen ohne In-Plugin-Editor
- Max/MSP, PureData: Grafische Programmierung, nicht textbasiert
- NeuroKore: Text-DSL **und** Graph-Platine **direkt im Plugin**, ein Konstrukt, zwei Sichten

---

## 3. Formel-Validierung mit Stabilitätsanalyse

**Was:** Bevor eine neue Formel in die Signalkette übernommen wird, testet `testFormulaStability()` sie ausführlich auf:
- NaN/Inf-Erzeugung
- DC-Offset
- Clipping-Verhalten
- Stabilitäts-Score (Verhältnis stabiler zu instabiler Samples)

**Warum einzigartig:**
- Die meisten Plugins übernehmen Parameter blind
- NeuroKore schützt den User vor selbst verschuldetem Audioschaden (Lautsprecher-Killer-Formeln)
- Mit Fortschrittsanzeige und konkreten Warnmeldungen vor der Aktivierung

---

## 4. Cross-Fade zwischen Formeln (glitchfreier Formelwechsel)

**Was:** Beim Wechsel zu einer neuen Formel blendet NeuroKore sanft zwischen `oldSignalChain` und `signalChain` über (`formulaBlend`).

**Warum einzigartig:**
- Verhindert Knackser und Aussetzer beim Live-Performanz-Einsatz
- Selbst teure kommerzielle Effekte (z. B. FabFilter) haben oft hörbare Glitches bei Preset-Wechseln
- NeuroKore ermöglicht **Echtzeit-Formel-Morphing** auf der Bühne

---

## 5. Lokalisierte DSL-Fehlermeldungen (DE/EN)

**Was:** Fehlermeldungen des DSL-Parsers sind vollständig übersetzt (Deutsch und Englisch).

**Warum einzigartig:**
- Im Audio-Plugin-Markt praktisch einmalig
- Macht das Plugin für deutschsprachige Nicht-Entwickler zugänglich
- Das `Localiser`-System ist erweiterbar für weitere Sprachen

---

## 6. LookupTable + Smoothing Kombination

**Was:** WaveShaper verwendet vorberechnete LookupTables für Performance, kombiniert mit Smoothing für glitchfreie Parameteränderungen.

**Warum einzigartig:**
- Viele günstige Waveshaper rendern entweder per-Sample (langsam) oder haben Parametersprünge (Knackser)
- Die Kombination bietet professionelle Performance **und** Qualität

---

## 7. Integrierter Formel-Präview (FormulaDisplayComponent)

**Was:** Bevor eine Formel aktiviert wird, zeigt NeuroKore eine Vorschau der Transfer-Kurve (Eingabe-/Ausgabe-Mapping).

**Warum wertvoll:**
- Der User sieht sofort ob die Formel sinnvoll ist (z. B. "sieht aus wie Soft-Clipping")
- Reduziert Trial-and-Error bei der DSL-Entwicklung
- Pädagogisch wertvoll: vermittelt den Zusammenhang zwischen Formel und Klang

---

## Zusammenfassung: Positionierung

NeuroKore kombiniert das Kreativitätspotential von **Musikprogrammierung** (FAUST/SOUL) mit der Zugänglichkeit und dem Look eines modernen **kommerziellen Effekt-Plugins**.

```
                    Zugänglichkeit
                          ↑
    Max/MSP ─────────────►│◄─────────────── NeuroKore
    PureData               │              (Ziel)
                           │
    FAUST/SOUL ────────────┼──────────────► Mächtigkeit
                           │
                     NeuroKore
                    (aktueller Stand)
```

Das Alleinstellungsmerkmal ist die **Kombination**:
- Professionelle DSP-Engine (SIMD, Constant-Folding)
- Benutzerfreundliche DSL (sofort erlernbar)
- Sicherheitsnetz (Stabilitätsvalidierung, Cross-Fade)
- Plugin-Format (VST3, kein Max/MSP nötig)
