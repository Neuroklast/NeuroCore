# NeuroCore – Architektur-Übersicht

## RT-Verträge (Crackle → Timeline/State, nicht Thresholds)

Wenn Knistern/Crackle auftritt: **Architektur härten**, keine Magic-Number-Workarounds.

| Vertrag | Owner | Regel |
|--------|--------|--------|
| **Eine Timeline** | `LatencyAlignedSidechain` | Dry-Sidechain-Delay == OS-Wet-Latenz. AutoGain/Sanitizer/Mix vergleichen nie raw Dry mit delayed Wet. |
| **Eine Residual-Policy** | `OutputSanitizer` | Nur hier Residual-Mute. AutoGain **nie** muten. Kein NoiseGate in der Chain. |
| **Eine Peak-Safety** | `OutputSanitizer` wenn Polisher=None | Polisher Limiter/HardClip → Sanitizer Peak **aus** (`setPeakSafetyEnabled`). |
| **AutoGain optional** | APVTS `autoGain` 0…1 | Default **0** (off). Strength skaliert mildes RMS-Match. |
| **Kontinuierliche Control-Rate** | knob lanes + `mixDryWetContinuous` | Knobs und Dry/Wet sample-rate, nicht block-constant. |
| **Filter-Timebase** | `advanceCoeffsOnce` + `processSample(ch)` | Coeff 1×/Sample; pro Kanal eigener SVF-State. |
| **Kein Dual-Chain-Audio** | `DspEngine` | Nur `signalChain`; Formula-Wechsel = `switchRamp`, kein old+new Blend. |
| **DSL Multi-Bus** | `BusGraph` + `SignalChain` | Max. 4 Named Buses + `in`/`main`. Send nur rückwärts (DAG). Mixdown **in** der DSL, eine Engine-Timeline. |
| **State-Reset** | `clearRuntimeState` / prepare | ADAA/Delay/Reverb nur bei Formula-Load/prepare, nie pro Block. |

Defaults: OS **2×** (`Config::kDefaultOversamplingIndex = 1`), Diagnostics **off**, AutoGain **off**.

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
                                └─► DC-Blocker
                                      └─► SignalPolisher (optional musical ceiling)
                                            └─► LPF anti-alias
                                                  └─► [Oversampling ↓]
                                                        └─► mixDryWetContinuous (aligned dry × wet)
                                                              └─► AutoGain (strength, default off)
                                                                    └─► OutputGain
                                                                          └─► OutputSanitizer (aligned dry)
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

### `PluginEditor` (`src/core/PluginEditor.h/.cpp`)
- `juce::AudioProcessorEditor`-Erbe
- Instanziiert alle UI-Komponenten und verbindet sie mit dem APVTS
- Fenster: **1600 × 900 px** (fest, Resizing noch nicht implementiert)

### `Config.h` (`src/core/Config.h`)
Zentrale Konfigurationskonstanten in anonymen Namespaces:

| Kategorie | Beispiel-Konstanten |
|---|---|
| GUI | `kWindowWidth`, `kWindowHeight`, `kKnobSize` |
| DSP | `kDefaultSampleRate`, `kMaxBlockSize`, `kOversamplingFactor` |
| Parser | `kMaxFormulaLength`, `kMaxStages` |
| Presets | `kPresetFileExtension`, `kEncryptionKey` |
| Licensing | `kEnableLicensing`, `kLicenseServerUrl` ← Placeholder! |

### `EffectParameters` (`src/core/EffectParameters.h`)
Parameter-IDs für den APVTS:

| Parameter | ID | Beschreibung |
|---|---|---|
| Knob A–D | `"paramA"` … `"paramD"` | Frei belegbare DSL-Variablen |
| Input Gain | `"inputGain"` | Eingangs-Verstärkung |
| Output Gain | `"outputGain"` | Ausgangs-Verstärkung |
| Mix | `"dryWetMix"` | Dry/Wet-Verhältnis |
| Oversampling | `"oversamplingIndex"` | Oversampling-Faktor (kein UI-Control!) |

---

## DSL-System

### `DSLParser` (`src/dsl/DSLParser.h/.cpp`)
- Parst DSL-Text in `BlockDesc`-/`ParamDesc`-Strukturen
- Unterstützte Block-Typen: `stage`, `filter`, `comp`, `env`, `osc`, `param`
- Liefert strukturierte Fehler mit Zeilen-/Spaltenangabe
- ⚠️ Keine Unit-Tests vorhanden

### `SignalChain` (`src/dsl/SignalChain.h/.cpp`)
- Führt die geparsten Blöcke als Audio-Processing-Chain aus
- Innere Klassen (als Member der `Chain`-Klasse):
  - `Stage` – mathematische Formel via `ExpressionEvaluator`
  - `Filter` – `juce::StateVariableTPTFilter`
  - `Comp` – Kompressor mit Attack/Release
  - `Env` – Envelope Follower (RMS oder Peak)
  - `Osc` – Sinus/Saw/Triangle/Square LFO
- Unterstützt **Cross-Fade** (`formulaBlend`) beim Formel-Wechsel
- ⚠️ `std::shared_ptr<Chain>` ohne `atomic_load` → Race Condition möglich

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
| `SignalPolisher` | `SignalPolisher.h` | Noise-Gate + DC-Filter |
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
| `PluginLookAndFeel` (NeuroCoreLookAndFeel) | Globaler Look & Feel, Farben, Schriften |
| `DslTerminalEditor` | Code-Editor für DSL-Eingabe |
| `WaveformDisplayComponent` | Input/Output-Wellenform-Anzeige |
| `LoudnessMeterComponent` | Echtzeit-Loudness-Meter |
| `FormulaDisplayComponent` | Formel-Vorschau-Wellenform |
| `ParameterComponent` | Custom-Knob-Widgets (a–d) |
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

### `LicenseManager`
- Online-Aktivierung via HTTP-Request (blocking!) → ⚠️ friert UI ein
- Offline-Aktivierung via Lizenzdatei
- `HardwareFingerprint` – eindeutige Geräte-ID
- ❌ `kLicenseServerUrl = "https://licensing.example.com/activate"` ist ein Placeholder
- Für Dev-Builds: `kEnableLicensing = false` in `Config.h` setzen
