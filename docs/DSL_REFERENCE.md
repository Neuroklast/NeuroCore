# NeuroCore DSL – Sprachreferenz

Die NeuroCore-DSL ist eine deklarative Skriptsprache zur Beschreibung von Audio-Signalketten.
Jedes Skript besteht aus einer Folge von **Blöcken**, die von oben nach unten verarbeitet werden.

---

## Allgemeine Syntax

```
blocktyp {
    schlüssel wert
    schlüssel wert
}
```

- Blöcke werden in geschweiften Klammern definiert
- Schlüssel und Werte sind durch Leerzeichen getrennt
- Kommentare: `# Kommentartext`
- Zahlen: Ganzzahlen und Gleitkommazahlen (z. B. `0.5`, `-1.0`, `440`)
- Formeln: mathematische Ausdrücke (z. B. `tanh(x * a)`)

---

## Block-Typen

### `param` – Parameter-Alias

Weist einem der vier Knobs (a–d) einen benutzerdefinierten Namen und Wertebereich zu.

```
param {
    alias   a
    name    "Drive"
    min     0.0
    max     2.0
}
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `alias` | `a` \| `b` \| `c` \| `d` | Welcher Knob wird konfiguriert |
| `name` | String | Anzeigename im UI |
| `min` | Zahl | Minimaler Parameterwert |
| `max` | Zahl | Maximaler Parameterwert |

---

### `stage` – Mathematische Transformation

Der Kernblock. Transformiert das Audiosignal über eine mathematische Formel.

```
stage {
    y   tanh(x * a * 2.0)
}
```

#### Verfügbare Variablen in der Formel (`y = ...`)

| Variable | Beschreibung |
|---|---|
| `x` | Aktueller Eingabe-Sample |
| `x_prev` | Vorheriger Eingabe-Sample (Leak-Faktor 0.9999 – verhindert DC-Akkumulation) |
| `y_prev` | Ausgabe des vorherigen Samples – Feedback (Leak-Faktor 0.9999) |
| `ch` | Aktueller Kanal: 0 = links, 1 = rechts |
| `a` | Knob A (nach `param`-Block skaliert) |
| `b` | Knob B |
| `c` | Knob C |
| `d` | Knob D |
| `env1` … `envN` | Ausgabe eines benannten `env`-Blocks |
| `osc1` … `oscN` | Ausgabe eines benannten `osc`-Blocks |
| `t` | Aktuelle Zeit in Sekunden (z. B. für `sin(2 * pi * 440 * t)`) |
| `sr` | Sample-Rate in Hz |
| `pi` | π (3.14159…) |
| `midi_note` | Aktuelle MIDI-Note (0–127, 0 wenn keine Note) |
| `midi_freq` | Frequenz der MIDI-Note in Hz |
| `midi_vel` | MIDI-Velocity (0.0–1.0) |
| `midi_gate` | 1.0 wenn Note aktiv, 0.0 sonst |
| `midi_bend` | Pitch Bend (-1.0 bis +1.0) |
| `midi_mod` | CC1 Modulationsrad (0.0–1.0) |

**Kanal-Routing** (optional pro Stage):

| Argument | Werte | Beschreibung |
|---|---|---|
| `channel` | `left` \| `right` \| `both` | Stage wird nur auf diesen Kanal angewendet (Standard: `both`) |
| `ms_encode` | `true` | Wandelt L/R-Buffer in Mid/Side um (vor der Formel) |
| `ms_decode` | `true` | Wandelt Mid/Side-Buffer zurück in L/R (vor der Formel) |

---

### `filter` – Zustandsvariablen-Filter

Wendet einen digitalen Filter an (`juce::StateVariableTPTFilter`).

```
filter {
    type     lowpass
    cutoff   800
    resonance 0.7
}
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `type` | `lowpass` \| `highpass` \| `bandpass` | Filtertyp |
| `cutoff` | Hz | Grenzfrequenz (auch als Formel möglich: `400 + a * 2000`) |
| `resonance` | 0.1–10 | Gütefaktor (Q) |
| `center` | Hz | Mittenfrequenz (für Bandpass) |
| `width` | Hz | Bandbreite (für Bandpass) |
| `lowcut` | Hz | Untere Grenzfrequenz (bei kombiniertem Typ) |
| `highcut` | Hz | Obere Grenzfrequenz (bei kombiniertem Typ) |

---

### `comp` – Kompressor

Dynamik-Kompressor mit einstellbaren Schwellen- und Zeitkonstanten.

```
comp {
    threshold  -12
    ratio       4.0
    attack      5
    release     100
}
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `threshold` | dBFS | Kompressions-Schwelle |
| `ratio` | ≥ 1.0 | Kompressions-Verhältnis (1:1 = kein Effekt, ∞:1 = Limiter) |
| `attack` | ms | Einschwingzeit |
| `release` | ms | Ausschwingzeit |

---

### `env` – Envelope Follower

Verfolgt die Hüllkurve des Signals und stellt den Wert als Variable zur Verfügung.

```
env {
    name     env1
    mode     rms
    attack   10
    release  200
}
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `name` | Bezeichner | Name der Output-Variable (z. B. `env1`, `envDrive`) |
| `mode` | `rms` \| `peak` | Messmethode |
| `attack` | ms | Ansprechzeit |
| `release` | ms | Abklingzeit |
| `trigger` | `midi_gate` | Startet Attack-Phase bei jedem MIDI-Note-On neu |

Der Ausgabewert (0.0–1.0) steht in nachfolgenden `stage`-Blöcken als `env1` usw. zur Verfügung.

---

### `osc` – LFO-Oszillator

Erzeugt ein periodisches Signal als Modulationsquelle.

```
osc {
    name    osc1
    shape   sin
    freq    0.5
    depth   1.0
}
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `name` | Bezeichner | Name der Output-Variable (z. B. `osc1`, `oscVib`) |
| `shape` | `sin` \| `saw` \| `tri` \| `square` | Wellenform |
| `freq` | Hz | Frequenz des LFOs (wird ignoriert wenn `sync` gesetzt) |
| `depth` | 0.0–1.0 | Amplitude des LFOs |
| `sync` | Ratio | Synchronisiert die Frequenz an das DAW-Tempo (z. B. `1/4` = Viertelnote, `1/8` = Achtelnote, `1` = Takt, `2` = 2 Takte) |

**Beispiel Tempo-Sync:**
```
osc1: shape = sin; sync = 1/4; depth = d
```

Der Ausgabewert (-1.0–+1.0) steht in nachfolgenden `stage`-Blöcken zur Verfügung.

---

## Verfügbare Mathematik-Funktionen

| Funktion | Syntax | Beschreibung |
|---|---|---|
| `sin` | `sin(x)` | Sinus |
| `cos` | `cos(x)` | Kosinus |
| `tan` | `tan(x)` | Tangens |
| `tanh` | `tanh(x)` | Hyperbolischer Tangens (Soft-Clip) |
| `abs` | `abs(x)` | Absolutwert |
| `sign` | `sign(x)` | Vorzeichen (-1, 0, +1) |
| `sqrt` | `sqrt(x)` | Quadratwurzel |
| `exp` | `exp(x)` | Exponentialfunktion (e^x) |
| `log` | `log(x)` | Natürlicher Logarithmus |
| `log2` | `log2(x)` | Logarithmus zur Basis 2 |
| `pow` | `pow(x, n)` | Potenz (x^n) |
| `min` | `min(x, y)` | Minimum zweier Werte |
| `max` | `max(x, y)` | Maximum zweier Werte |
| `clamp` | `clamp(x, lo, hi)` | Begrenzt x auf [lo, hi] |
| `lerp` | `lerp(a, b, t)` | Lineare Interpolation |
| `step` | `step(edge, x)` | Stufenfunktion (0 oder 1) |
| `smoothstep` | `smoothstep(lo, hi, x)` | Weiche Stufenfunktion |
| `fmod` | `fmod(x, y)` | Gleitkomma-Modulo |
| `noise` | `noise(x)` | Pseudo-Rauschen (deterministisch) |
| `fold` | `fold(x, lo, hi)` | Faltung/Reflektions-Waveshaping |
| `wrap` | `wrap(x, lo, hi)` | Wrap-Around-Waveshaping |
| `softclip` | `softclip(x)` | Sanftes Clipping |
| `hardclip` | `hardclip(x, limit)` | Hartes Clipping |
| `bitcrush` | `bitcrush(x, bits)` | Bit-Reduktion |
| `quantize` | `quantize(x, steps)` | Stufenquantisierung |

---

## Beispiel-Skripte

### Einfaches Soft-Clipping (Overdrive)

```
param {
    alias  a
    name   "Drive"
    min    0.1
    max    4.0
}

stage {
    y   tanh(x * a)
}
```

### Tremolo mit LFO

```
param {
    alias  b
    name   "Rate"
    min    0.1
    max    10.0
}

osc {
    name   osc1
    shape  sin
    freq   b
    depth  1.0
}

stage {
    y   x * (0.5 + 0.5 * osc1)
}
```

### Envelope-gesteuerter Filter + Kompressor

```
env {
    name     env1
    mode     rms
    attack   20
    release  300
}

filter {
    type     lowpass
    cutoff   200 + env1 * 4000
    resonance 0.8
}

comp {
    threshold  -18
    ratio       3.0
    attack      10
    release     150
}
```

### Bitcrusher mit Feedback

```
param {
    alias  c
    name   "Bits"
    min    2.0
    max    16.0
}

stage {
    y   bitcrush(x + y_prev * 0.1, c)
}
```

---

## Hinweise und Einschränkungen

- Maximale Anzahl Stages: siehe `kMaxStages` in `Config.h`
- Maximale Formellänge: `kMaxFormulaLength` Zeichen
- Rekursive Abhängigkeiten (z. B. zwei Stages die sich gegenseitig referenzieren) sind nicht möglich
- Bei Verwendung von `y_prev` ohne initialen Wert: Erster Sample-Wert ist 0.0
- `noise()` ist deterministisch und sample-positionsabhängig, kein echtes Rauschen
- Formeln werden validiert bevor sie in die Signalkette übernommen werden (`ValidationContentComponent`)
