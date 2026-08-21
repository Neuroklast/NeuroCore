# NeuroKore – Architektur-Übersicht

## RT-Verträge (Crackle → Timeline/State, nicht Thresholds)

Wenn Knistern/Crackle auftritt: **Architektur härten**, keine Magic-Number-Workarounds.

| Vertrag | Owner | Regel |
|--------|--------|--------|
| **Eine Timeline** | `LatencyAlignedSidechain` | Dry-Sidechain-Delay == OS-Wet-Latenz. AutoGain/Sanitizer/Mix vergleichen nie raw Dry mit delayed Wet. |
| **Eine Residual-Policy** | `OutputSanitizer` (via `SanitationChain`) | Nur hier Residual-Mute. AutoGain **nie** muten. Kein NoiseGate in der Chain. |
| **Eine Peak-Safety** | `SanitationChain` True-Peak brickwall (−0.3 dBTP) | Immer an. Soft-Clip ist optional davor. Reihenfolge ist fest. |
| **AutoGain optional** | APVTS `autoGain` 0…1 | Default **0** (off). Strength skaliert mildes RMS-Match. |
| **Kontinuierliche Control-Rate** | knob lanes + `mixDryWetContinuous` | Knobs und Dry/Wet sample-rate, nicht block-constant. |
| **Filter-Timebase** | `advanceCoeffsOnce` + `processSample(ch)` | Coeff 1×/Sample; pro Kanal eigener SVF-State. |
| **Kein Dual-Chain-Audio** | `DspEngine` | Nur `signalChain`; Formula-Wechsel = `switchRamp`, kein old+new Blend. |
| **DSL Multi-Bus** | `BusGraph` + `SignalChain` | Max. 4 Named Buses + `in`/`main`. Send nur rückwärts (DAG). Mixdown **in** der DSL, eine Engine-Timeline. |
| **State-Reset** | `clearRuntimeState` / prepare | ADAA/Delay/Reverb nur bei Formula-Load/prepare, nie pro Block. |

Defaults: OS **4×** (`Config::kDefaultOversamplingIndex = 2`), Diagnostics **off**, AutoGain **off**.

Diagnose-Heuristik: Crackle an `smp≈latency` → Timeline. `smp=0` → Control-Rate/State.

## Signalkette

```
Input
  └─► InputRouter
        └─► InputGain (host rate, vor Dry-Split)
              ├─► dryBuffer ──► LatencyAlignedSidechain ──┐
              └─► [Oversampling ↑] (wenn Mix > 0)
                    └─► DSL SignalChain (nur current; kein Dual-Run)
                          optional: in-snapshot → main + named buses (Send-DAG) → out mixdown
                          └─► Post-DSL NaN-hold only
                                └─► SanitationChain::processOversampled
                                      1. 1-pole DC (5 Hz)
                                      2. AA LPF (96 dB/oct Live / 128 dB/oct Studio, fc = 0.45·hostSr)
                                            └─► [Oversampling ↓]  (only after AA)
                                                  └─► mixDryWetContinuous (aligned dry × wet)
                                                        └─► AutoGain (strength, default off)
                                                              └─► OutputGain
                                                                    └─► SanitationChain::processHost
                                                                          3. optional Soft Clip
                                                                          4. True-Peak brickwall (−0.3 dBTP)
                                                                          5. residual mute + NaN hold
                                                                          6. TPDF dither IFF integer bit-depth drop
                                                                            └─► switchRamp
                                                                                  └─► Output
```

---

## Core-Module

### `PluginProcessor` (`src/core/PluginProcessor.h/.cpp`)
- Zentraler `juce::AudioProcessor`-Erbe
- Hält den **APVTS** (AudioProcessorValueTreeState) mit allen Host-automatisierbaren Parametern
- `processBlock()` führt die gesamte DSP-Kette aus
- Verwaltet `SignalChain`, `PresetManager`, `LicenseManager`, Waveform-Capture und Validierungs-Logik
- ⚠️ God-Class (~34 KB) – Refactoring geplant (siehe `docs/ROADMAP.md`)

### `PluginEditor` (`src/ui/PluginEditor.h/.cpp`)
- `juce::AudioProcessorEditor`-Erbe
- Instanziiert alle UI-Komponenten und verbindet sie mit dem APVTS
- Workspace: **Graph** (Platine, Snap, Chips) und **Script** (Text-Hack, Live-`[value]`)
- L/Both/R sitzt in der Knob-Spalte, gleiche Breite, gleiche Zeile wie Graph/Script
- Fenster skalierbar bei festem Seitenverhältnis (Settings 100 / 125 / 150)

### `Config.h` (`src/core/Config.h`)
Zentrale Konfigurationskonstanten in anonymen Namespaces:

| Kategorie | Beispiel-Konstanten |
|---|---|
| GUI | `kWindowWidth`, `kWindowHeight`, `kKnobSize` |
| DSP | `kDefaultSampleRate`, `kMaxBlockSize`, `kOversamplingFactor` |
| Parser | `kMaxFormulaLength`, `kMaxStages` |
| Presets | `kPresetFileExtension`, `kEncryptionKey` |
| Licensing | `kEnableLicensing`, `kDemoDurationSeconds` (20 min, Mix=0) |

### `EffectParameters` (`src/core/EffectParameters.h`)
Parameter-IDs für den APVTS:

| Parameter | ID | Beschreibung |
|---|---|---|
| Knob A–D | `"paramA"` … `"paramD"` | Frei belegbare DSL-Variablen |
| Input Gain | `"inputGain"` | Eingangs-Verstärkung |
| Output Gain | `"outputGain"` | Ausgangs-Verstärkung |
| Mix | `"dryWetMix"` | Dry/Wet-Verhältnis |
| Oversampling | `"oversamplingIndex"` | 1× / 2× / 4× / 8× (Settings + Statuszeile) |

---

## DSL-System

### `DSLParser` (`src/dsl/DSLParser.h/.cpp`)
- Parst DSL-Text in `BlockDesc`-/`ParamDesc`-Strukturen
- Blöcke: `stage`, `filter`, `eq`, `comp`, `gate`, `limit`, `delay`, `reverb`, `ir`, `ott`, `widen`, `ms`, `xover`, `bus`/`send`/`out`, `env`, `osc`, `param`
- Liefert strukturierte Fehler mit Zeilen-/Spaltenangabe

### `PcbRouter` (`src/dsl/PcbRouter.h/.cpp`)
- UI-freier orthogonaler Router für Circuit-Kabel (keine JUCE-Typen)
- A* auf einem 4-Nachbar-Gitter; State ist Zelle + Ankunftsrichtung
- Kantengewicht `1 + turnPenalty` bei Richtungswechsel; Heuristik ist Manhattan
- Hindernisse: Achsen-parallele Node-Boxen. Boxen, die Start oder Ziel enthalten, bleiben passierbar
- Nachbearbeitung: colineare Punkte entfernen, 90°-Ecken als quadratische Beziers, paralleler Lane-Offset bei geteilten Tracks
- Ausgabe: `PcbRoute` mit Waypoints und `Move`/`Line`/`Quad`-Kommandos. Das Canvas mapped das auf `juce::Path`

### `GraphModel` (`src/dsl/GraphModel.h/.cpp`)
- Editor-Datenmodell für den 2D Node-Patcher (`GraphCanvasComponent`)
- `parse` nutzt `DSLParser` und hängt `#`-Header/Trailing-Kommentare inkl. `@x,y` an
- `emit` schreibt kanonische DSL; Formel bleibt Source of Truth
- `jacksFor` / `jacksForInput` / `jacksForVirtualOut`: sichtbare Ports (Audio, Mix-Bus, Knob, SC, Mod, MID/SIDE, L/R, Xover)
- `visualRail` / `channelRail` / `visualAudioEdges`: Bus- und MS-Rails in der UI
- `isMsEncode` / `isMsDecode`: Mid-Side forkt wie ein Bus (MID/SIDE), `widen` ist Stereo-Width, nicht MS
- `connectJack` patched eine konkrete Buchse (Mix-Bus, LFO→Parameter, sonst `connectAudio`)
- `semanticallyEqual` vergleicht Typ/Name/Bus/Args und Param-Ranges, ignoriert Kommentartext
- `moveNode` verschiebt Blöcke mit Bus-Regeln (`send` nur im Named Bus, `out` zuletzt)

### `GraphCanvasComponent` (`src/ui/GraphCanvasComponent.h/.cpp`)
- Circuit-Modus: Platine. Karten rasten auf 16 px (`snap` / `snapPoint`). Ctrl+Rad zoomt (0.55–2.4)
- Karten-Drag schreibt nur `@x,y` (`setPosition`). Reihenfolge nur Overlay/Kabel, nie `commitNodeDrop`
- Audio-Kabel: weiss/grau, orthogonal über `PcbRouter` (gerundete 90°-Ecken, Bus-Lanes). Energie aus WaveformCapture (gleiche Quelle wie die Scopes); optional Tap-Welle
- Bus-Kopf ist verdrahtet: IN -> BUS -> send
- Live-Knobwerte stehen rot auf dem Chip
- Doppelklick öffnet `NodeInspectComponent` (alle Args, a–f, Apply/Remove)
- Terminal bleibt der Text-Hack desselben Konstrukts

### `SignalChain` (`src/dsl/SignalChain.h/.cpp`)
- Führt die geparsten Blöcke als Audio-Processing-Chain aus
- Delay: Hermite-Interpolation, samplegenaue Zeit/Feedback/Mix/Damp, Write-Head ≥ 8 Samples
- Studio-Oversampling: Host-Nyquist-AA-LPF läuft immer vor dem Downsample (auch bei 8× FIR)
- Innere Klassen: Stage, Filter, Comp, Gate / noisegate, Limit, Delay, Reverb, IR, Ott, Widen, MS, Xover, Env, Osc
- Nach jedem Block: `writeNodeTap` (64 Samples) für die Circuit-Kabel
- Formelwechsel über `switchRamp` (kein Dual-Chain-Blend im Audio-Thread)

### `ExpressionEvaluator` (`src/dsl/ExpressionEvaluator.h/.cpp`)
- AST-basierter Mathe-Parser
- **Optimierungen:**
  - Constant Folding (Konstanten werden zur Parse-Zeit berechnet)
  - CSE-Elimination (Common Subexpression Elimination)
  - SIMD-Support (evaluateBlockSimd für Blöcke)
- Hauptmethoden: `parseFormula()`, `evaluateBlock()`, `evaluateBlockSimd()`
- Unterstützte Funktionen: siehe `docs/DSL_REFERENCE.md`

---

## DSP-Module (`src/dsp/`)

| Modul | Datei | Funktion |
|---|---|---|
| `InputGain` | `InputGain.h` | Eingangs-Verstärkung mit Smoothing |
| `InputRouter` | `InputRouter.h` | Kanal-Routing (Mono/Stereo) |
| `WaveShaper` | `WaveShaper.h` | Waveshaping via LookupTable |
| `SanitationChain` | `SanitationChain.h` | DC → AA → optional clip → True-Peak → residual → dither |
| `LowPassFilter` | `LowPassFilter.h` | Einfacher Tiefpass |
| `DSPUtils` | `DSPUtils.h` | Hilfs-Algorithmen (siehe unten) |

### `DSPUtils` – Algorithmen
- **Kahan-Summation RMS** – numerisch stabile RMS-Berechnung
- **DC-Offset-Erkennung** – Analyse auf Gleichspannungsanteil
- **Auto-Gain-Compensation** (`autoGainCompensate`) – ⚠️ per-Sample SubBlock (Performance-Bottleneck)
- **FFT-Analyse** – Frequenzspektrum-Berechnung
- **LUFS-Berechnung** – Loudness-Messung nach EBU R128

---

## UI-Komponenten (`src/ui/`)

| Komponente | Beschreibung |
|---|---|
| `PluginLookAndFeel` (NeuroKoreLookAndFeel) | Globaler Look & Feel, Farben, Schriften |
| `DslTerminalEditor` | Code-Editor (Terminal-Edit). IR-Button in Zeilenhöhe. Keine Live-`[value]`-Annotation |
| `GraphCanvasComponent` | Circuit-Platine: Snap, Zoom, rose Kreuze, Chip-Karten, orthogonale PCB-Kabel |
| `NodeInspectComponent` | Overlay pro Block: alle Parameter, Knob-Bind, Remove/Apply |
| `WaveformDisplayComponent` | Input/Output-Wellenform-Anzeige |
| `LoudnessMeterComponent` | Echtzeit-Loudness-Meter |
| `FormulaDisplayComponent` | Script-Live-Ansicht: Farbe + `a[value]` |
| `ParameterComponent` | Knobs a–f; Wert unter dem Zeiger; Drag hebt Knob-Kabel |
| `ModalOverlay` | Overlay-Container für Dialoge |
| `ValidationContentComponent` | Formel-Validierungs-Dialog |
| `PresetContentComponent` | Preset-Browser-Panel |
| `FunctionsContentComponent` | DSL-Funktions-Referenz-Panel |
| `StagesContentComponent` | Signalkette-Übersicht (parse Blocks aus aktuellem Skript) |
| `WeightedLayout` | Flexibles Layout-System mit gewichteten Spalten/Zeilen |
| `CyberFxDirector` | Message-Thread-only FX-State (Glitch, Pulse, Visibility, Reduced-Motion) |
| `CyberSequence` | Overlay Auf-/Abbau (Scrim → Slice → Reveal, umgekehrt) |
| `CyberBackdropCache` | Grid/Scanlines/Hex-Streifen, Rebuild nur bei Resize |
| `BootSequenceOverlay` | Skippbarer Erst-Open-Boot, Kind des Editors |

Cyber-UI-Regeln: kein Audio-Thread, kein WebView, kein Vollfenster-`repaint()` ohne Dirty-Flag. Ambient-Paint ist ein Image-Blit plus wenige Rects. Overlays bleiben Editor-Kinder (kein Desktop-Peer).

---

## Utils (`src/utils/`)

### `PresetManager`
- Speichert/lädt Presets als Dateien
- **Blowfish-Verschlüsselung** für Preset-Daten
- ⚠️ Erwäge Upgrade auf AES-256 in Phase 2

### `FormulaHelper`
- Formel-Optimierungs-Hilfsfunktionen
- Sanitierung und Vorverarbeitung von DSL-Eingaben

### `Localiser`
- i18n-System (DE/EN)
- Sprachdateien in `resources/`
- Lokalisierte Fehlermeldungen für den DSL-Parser

---

## Licensing (`src/licensing/`)

### Offline `.lic`
- Issuer `NeuroKoreIssuer`: E-Mail eintragen, signierte Datei speichern
- Plugin prüft RSA-Signatur mit dem Public Key
- Private Key nur in `tools/license_issuer/LicensePrivateKey.h`
- Unlizenziert: nach 20 Minuten Mix = 0 (dry). Import über den License-Button
- Unit-Tests schalten die Sperre mit `NEUROKORE_SKIP_LICENSE_ENFORCEMENT` aus
