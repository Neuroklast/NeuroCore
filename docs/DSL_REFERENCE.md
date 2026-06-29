# NeuroCore DSL – Sprachreferenz

Die NeuroCore-DSL beschreibt Audio-Signalketten als **zeilenbasiertes Skript**.
Blöcke werden von oben nach unten verarbeitet.

---

## Allgemeine Syntax

```
blockname: schlüssel = wert; schlüssel = wert
```

- Jede Zeile definiert einen Block: `name:` gefolgt von Argumenten
- Argumente werden mit `;` getrennt, Schlüssel und Wert mit `=`
- `param`-Zeilen müssen **vor** allen anderen Blöcken stehen
- Kommentare: `# ...` oder `// ...`
- Zahlen: Ganzzahlen und Gleitkommazahlen (`0.5`, `-1.0`, `440`)
- Formeln in `stage`: mathematische Ausdrücke (`tanh(x * a)`)

**Beispiel (Minimal):**

```
stage1: y = tanh(x * a)
```

---

## `param` – Parameter-Alias

Weist einem Knob (`a`–`d`) einen Anzeigenamen und optionalen Wertebereich zu.

```
param a = Drive [0.0, 2.0]
param b = Rate [0.1, 10.0]
```

| Teil | Beschreibung |
|---|---|
| `a`–`d` | Welcher Knob konfiguriert wird |
| `Drive` | Anzeigename im UI |
| `[min, max]` | Optionaler Wertebereich (Standard: `[0, 1]`) |

Nach dem Parsen steht der Skaliertwert weiterhin unter `a`–`d`; in Formeln kann der Alias-Name (z. B. `drive`) verwendet werden.

---

## `stage` – Mathematische Transformation

Kernblock. Transformiert das Signal über eine Formel.

```
stage1: y = tanh(x * a * 2.0)
stage2: channel = left; y = x * 0.5
```

| Argument | Werte | Beschreibung |
|---|---|---|
| `y` | Formel | **Pflicht.** Ausgabeformel |
| `channel` | `left` \| `right` \| `both` | Kanal-Routing (Standard: `both`) |
| `ms_encode` | `true` | L/R → Mid/Side vor der Formel |
| `ms_decode` | `true` | Mid/Side → L/R vor der Formel |

### Variablen in `y = ...`

| Variable | Beschreibung |
|---|---|
| `x` | Aktueller Eingabe-Sample |
| `x_prev` | Vorheriger Eingabe-Sample (Leak 0.9999) |
| `y_prev` | Vorheriger Ausgabe-Sample – Feedback (Leak 0.9999) |
| `ch` | Kanal: 0 = links, 1 = rechts |
| `a`–`d` | Knob-Werte (ggf. über `param` skaliert) |
| `env1` … | Ausgabe eines `env`-Blocks (Blockname) |
| `osc1` … | Ausgabe eines `osc`-Blocks (Blockname) |
| `t` | Zeit in Sekunden |
| `sr` | Sample-Rate in Hz |
| `pi` | π |
| `midi_note` | MIDI-Note (0–127) |
| `midi_freq` | Frequenz der MIDI-Note in Hz |
| `midi_vel` | Velocity (0.0–1.0) |
| `midi_gate` | 1.0 wenn Note aktiv |
| `midi_bend` | Pitch Bend (-1.0 … +1.0) |
| `midi_mod` | CC1 Mod Wheel (0.0–1.0) |

---

## `filter` – Zustandsvariablen-Filter

```
filter1: type = lowpass; cutoff = 800; resonance = 0.7
filter2: type = bandpass; center = 1000; width = 500
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `type` | `lowpass` \| `highpass` \| `bandpass` | Filtertyp (Kurzformen: `lpf`, `hpf`, `bpf`) |
| `cutoff` | Hz / Formel | Grenzfrequenz |
| `resonance` | 0.1–10 | Gütefaktor |
| `center` | Hz | Mittenfrequenz (Bandpass) |
| `width` | Hz | Bandbreite (Bandpass) |
| `lowcut` / `highcut` | Hz | Alternative Bandpass-Definition |

Bandpass erfordert entweder `center` + `width` oder `lowcut` + `highcut`.

---

## `comp` – Kompressor

```
comp1: threshold = -12; ratio = 4.0; attack = 0.005; release = 0.1
```

| Argument | Beschreibung |
|---|---|
| `threshold` | Schwelle (dBFS oder Formel) |
| `ratio` | Kompressionsverhältnis (≥ 1.0) |
| `attack` | Anstiegszeit in Sekunden (oder Formel) |
| `release` | Abklingzeit in Sekunden (oder Formel) |

---

## `env` – Envelope Follower

```
env1: type = rms; attack = 0.01; release = 0.2
env2: type = peak; trigger = midi_gate
```

| Argument | Werte | Beschreibung |
|---|---|---|
| `type` | `rms` \| `peak` | Messmethode |
| `attack` | Sekunden / Formel | Anstiegszeit |
| `release` | Sekunden / Formel | Abklingzeit |
| `trigger` | `midi_gate` | Attack bei MIDI-Note-On neu starten |

Der Blockname (`env1`) ist die Variable in nachfolgenden `stage`-Formeln. Ausgabe: 0.0–1.0.

---

## `osc` – LFO-Oszillator

```
osc1: shape = sine; freq = 0.5; depth = 1.0
osc2: shape = triangle; sync = 1/4; depth = d
```

| Argument | Werte | Beschreibung |
|---|---|---|
| `shape` | `sine` \| `saw` \| `triangle` \| `square` \| `noise` | Wellenform |
| `freq` | Hz / Formel | Frequenz (ignoriert wenn `sync` gesetzt) |
| `depth` | 0.0–1.0 | Amplitude |
| `sync` | Ratio-String | Tempo-Sync (`1/4`, `1/8`, `1`, `2`, …) |

Der Blockname (`osc1`) ist die Variable in Formeln. Ausgabe: ca. -1.0 … +1.0.

---

## Mathematik-Funktionen

| Funktion | Syntax | Beschreibung |
|---|---|---|
| `sin`, `cos`, `tan` | `sin(x)` | Trigonometrie |
| `tanh` | `tanh(x)` | Soft-Clip |
| `abs`, `sign` | `abs(x)` | Absolutwert / Vorzeichen |
| `sqrt`, `exp`, `log`, `log2` | `sqrt(x)` | Standard-Math |
| `pow` | `pow(x, n)` | Potenz |
| `min`, `max`, `clamp` | `clamp(x, lo, hi)` | Begrenzung |
| `lerp` | `lerp(a, b, t)` | Interpolation |
| `step`, `smoothstep` | `step(edge, x)` | Stufen / weiche Stufen |
| `fmod` | `fmod(x, y)` | Modulo |
| `map` | `map(x, inLo, inHi, outLo, outHi)` | Wertebereich-Mapping |
| `noise` | `noise(x)` | Deterministisches Rauschen |
| `fold`, `wrap` | `fold(x, lo, hi)` | Waveshaping |
| `softclip`, `hardclip` | `hardclip(x, limit)` | Clipping |
| `bitcrush`, `quantize` | `bitcrush(x, bits)` | Lo-Fi |

---

## Beispiel-Skripte

### Soft Overdrive

```
param a = Drive [0.1, 4.0]
stage1: y = tanh(x * a)
```

### Tremolo mit LFO

```
param b = Rate [0.1, 10.0]
osc1: shape = sine; freq = b; depth = 1.0
stage1: y = x * (0.5 + 0.5 * osc1)
```

### Envelope-Filter + Kompressor

```
env1: type = rms; attack = 0.02; release = 0.3
filter1: type = lowpass; cutoff = 200 + env1 * 4000; resonance = 0.8
comp1: threshold = -18; ratio = 3.0; attack = 0.01; release = 0.15
stage1: y = x
```

### Bitcrusher mit Feedback

```
param c = Bits [2.0, 16.0]
stage1: y = bitcrush(x + y_prev * 0.1, c)
```

### Tempo-synchroner LFO

```
osc1: shape = sine; sync = 1/4; depth = d
stage1: y = x + osc1 * 0.1
```

---

## Hinweise

- Blocknamen müssen eindeutig sein
- `stage` ohne `y = ...` ist ein Parse-Fehler
- `y_prev` / `x_prev` starten bei 0.0
- Formeln werden vor Übernahme über die Validierung geprüft (`testFormulaStability`)
- Factory-Presets in `resources/factory_presets.json` verwenden Zeilen-Syntax (seit 2026-06-29)