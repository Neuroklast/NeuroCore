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
| `channel` | `left`/`l`/`mid`/`m` \| `right`/`r`/`side`/`s` \| `both` | Kanal-Routing (Standard: `both`). **mid/side** = L/R **nach** `ms encode` |
| `ms_encode` | `true` | L/R → Mid/Side vor der Formel |
| `ms_decode` | `true` | Mid/Side → L/R nach der Formel |

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
| `channel` | `left`/`mid` \| `right`/`side` \| `both` | Nur diesen Kanal filtern (nützlich nach `ms encode`) |

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

## `delay` – echte Delay-Line

```
delay1: time = 350; feedback = 0.4; mix = 0.35; damp = 4500
delay2: sync = 1/8; feedback = 0.45; mix = 0.4; damp = 5000; pingpong = true
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `time` / `time_ms` | ms / Formel / Knob | Delay-Zeit in **Millisekunden** (1…2000) |
| `sync` | `1/16`…`1/1` / `bar` | Tempo-Sync (Host-BPM); ersetzt `time` |
| `feedback` / `fb` | 0…0.95 | Feedback-Gain |
| `mix` / `wet` | 0…1 | Wet-Anteil (0 = dry, 1 = nur Delay) |
| `damp` / `damping` / `tone` | Hz | One-Pole-LPF im Feedback (Dunkler bei niedriger Hz) |
| `pingpong` | `true` | Stereo-Kreuz-Feedback L↔R |
| `channel` | `left`/`mid` \| `right`/`side` \| `both` | Kanal-Routing |

**Hinweis:** Das ist eine echte Ringpuffer-Delay-Line (lineare Interpolation), **kein** 1-Sample-`y_prev`-Comb.

---

## `reverb` – algorithmischer Hall (Freeverb-Stil)

```
reverb1: size = 0.55; decay = 0.5; damp = 0.4; mix = 0.3; width = 1.0
```

| Argument | Typ | Beschreibung |
|---|---|---|
| `size` / `room` | 0…1 | Raumgröße (skaliert Comb/Allpass-Längen) |
| `decay` / `feedback` | 0…0.95 | Nachhall / Comb-Feedback |
| `damp` / `damping` | 0…1 | Höhen-Dämpfung in den Combs |
| `mix` / `wet` | 0…1 | Wet-Anteil |
| `width` / `stereo` | 0…1 | Stereobreite des Wet-Signals (0 = mono) |

8 parallele Combs + 4 Allpasses pro Kanal (Schroeder/Freeverb). Alias: `verb1: …`.

---

## `ms` – Mid/Side Encode/Decode

```
ms1: mode = encode
stage1: channel = mid; y = x
stage2: channel = side; y = x * a
ms2: mode = decode
```

| Argument | Werte | Beschreibung |
|---|---|---|
| `mode` / `type` | `encode` (Default) \| `decode` | L/R↔M/S |

Nach `encode` liegt **Mid auf L**, **Side auf R**. Stages/Filter mit `channel = mid|side` (oder `left|right`) greifen die passende Komponente. Alternativ weiterhin pro Stage: `ms_encode = true` / `ms_decode = true`.

Filter unterstützen ebenfalls `channel = mid|side` (z. B. Side-HPF).

---

## Multi-Bus (`bus` / `send` / `out`)

Send-DAG: Input splitten, getrennte Pfade, Mixdown. Keine Feedback-Loops.

```
stage1: y = softclip(x, 1.2)

bus dirt:
  send: in = 1
  stage2: y = tube(x, a)

bus verb:
  send: main = 0.35
  delay1: time = 350; mix = 1

out: main = 1; dirt = c; verb = d
```

| Konstrukt | Bedeutung |
|---|---|
| `in` | Read-only Kopie des Ketten-Eingangs |
| `main` | Impliziter Bus. Alle Blöcke vor dem ersten `bus`/`out`. Startet als Kopie von `in` |
| `bus name:` | Öffnet einen benannten Bus (max. 4). Folgende Blöcke gehören dazu |
| `send: src = gain` | `busInput += gain * src` (`src` = `in`, `main` oder ein **früherer** Bus) |
| `out: name = gain; …` | Gewichtete Summe. Ohne `out:` ist der Output `main` |

Reservierte Namen: `in`, `main`, `out`, `bus`, `send`.  
`send` nur **in** einem benannten Bus. Einrückung ist optional (wird getrimmt).  
Gains: Zahl, Knob `a`–`f`, oder Komplement `1-c` / `1 - c` (0…2). So bleibt Blend ein Crossfade, keine Summe. Alte Skripte ohne `bus`/`out` bleiben seriell auf `main`.

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
| `sin`, `cos`, `tan`, `atan` | `sin(x)` | Trigonometrie |
| `tanh`, `asinh` | `tanh(x)` | Soft-Sättigung |
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
| `softclip` | `softclip(x)` / `softclip(x, drive)` | **atan**-Softsat \((2/\pi)\mathrm{atan}((\pi/2)\,d\,x)\) + ADAA (Drive ~0.25–6) — weniger HF/Knistern als altes \(x/\sqrt{1+x^2}\) |
| `hardclip` | `hardclip(x, limit)` | Soft-Knee-Ceiling **~18 % Knee** (smootherstep), **ohne** ADAA; echtes ±limit |
| `tube` | `tube(x, drive)` | Asym. 12AX7-Näherung + ADAA (Drive ~0.5–12) |
| `diode` | `diode(x, drive)` | Dioden-Softclip (asinh) + ADAA |
| `bitcrush`, `quantize` | `bitcrush(x, bits)` | Lo-Fi Quantisierung — **immer LPF danach** |
| `fold`, `wrap` | `fold(x, lo, hi)` | Wavefold/Wrap — **immer LPF + ≥4× OS** |
| `lerp`, `map`, `step`, `smoothstep`, `noise` | siehe Beispiele | Utility / Modulation |

**Engine-Safeguards:** Filter-Cutoff ∈ [20, Nyquist), Resonanz ∈ [0.1, 4.5], Stage Soft-Ceiling algebraisch, Default-Oversampling **4× FIR**, AA-LPF ≈ 0.88× Host-Nyquist auf OS-Buffer, Polisher Default Limiter, Silence-Duck im OutputSanitizer.

### Clipper ohne Knistern (Best Practice)

| Regel | Warum |
|---|---|
| `softclip` / `diode` / `tube` vorziehen | Keine harte Ecke → weniger Aliasing |
| Hard nur als Ceiling: `hardclip(softclip(x, drive), ceiling)` | Soft-Pre dämpft HF-Müll vor dem Limit |
| **Immer LPF nach** hardclip/fold/bitcrush/`y_prev`-Dirt | FormulaQuality **error** ohne Recovery-LPF |
| Optional mildes HPF vor dem Clip | Weniger Eingangs-Höhen → weniger Verzerrungs-Obertöne |
| Parallel: `lerp(x, softclip(x, a), blend)` | Transparente Peak-Kontrolle |
| Niemals `clamp(x, -1, 1)` als „Clipper“ | Reiner Brickwall → maximales Knistern |

Templates in `resources/templates.json` und Utility-Presets (`Soft Clip Tone`, `Soft-Knee Ceiling`, `Hard Clip Pedal`, …) zeigen dieselben Rezepte.

Jedes Template dokumentiert:

| JSON-Feld | Inhalt |
|---|---|
| `name` | Anzeigename (Autocomplete) |
| `category` | `distortion` \| `eq` \| `dynamics` \| `delay` \| `reverb` \| `modulation` \| `utility` |
| `description` | Kurze Wirkung |
| `formulas` | **Zugrundeliegende Formeln/Blöcke** (z. B. `softclip`, `comp`, `y_prev`) |
| `formula` | Volles DSL-Skript inkl. Header-Kommentare `// Formulas: …` |

**Delay / Reverb:** Echte Blöcke `delay` (Ringpuffer, Sync, Ping-Pong) und `reverb` (Freeverb-Stil). Templates und Factory-Kategorien **Delay** / **Reverb** nutzen diese Blöcke. `y_prev`-Combs bleiben für Regen/Dirt optional.

### Multi-Stage + `y_prev` (performant)

Der Hybrid-Pfad hält **Filter/Comp blockweise** und Stages ohne Feedback auf dem SIMD-Pfad.  
Nur Stages, die `y_prev` / `x_prev` lesen, laufen sample-weise — und **nur diese Stage**, nicht die ganze Kette.

| Regel | Warum |
|---|---|
| **Höchstens eine** Stage mit `y_prev`/`x_prev` | Jede weitere verdoppelt den skalaren Hotpath und neigt zu Howling |
| Vorher/nachher: pure `tube`/`softclip`/`diode` | Bleiben SIMD-fähig |
| Immer LPF nach Regen | Stabilität + weniger HF/Aliasing |
| Feedback-Gain ≤ ~0.5 | Engine-Leak + musikalischer Kopfraum |
| Kein `osc` + multi-regen-Stack | Osc ist ok (modLane), aber unnötig teuer für „Echo-Feeling“ |

Factory-Beispiele: **Preamp Regen Stack**, **Slapback Drive**, **Cascade Loop Dirt**, **Dual Path Regen**, **Tight Metal Regen**, …

---

## Beispiel-Skripte

### Soft Overdrive (mit Tone-LPF)

```
param a = Drive [0.8, 8.0]
param b = Tone [800, 9000]
param c = Level [0.4, 1.2]
filter1: type = highpass; cutoff = 55; resonance = 0.3
stage1: y = softclip(x, a)
filter2: type = lowpass; cutoff = b; resonance = 0.4
stage2: y = y * c
```

### Soft-Knee Peak Ceiling

```
param a = Ceiling [0.6, 0.99]
param b = Drive [1.0, 2.5]
stage1: y = hardclip(softclip(x, b), a)
filter1: type = lowpass; cutoff = 14000; resonance = 0.22
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