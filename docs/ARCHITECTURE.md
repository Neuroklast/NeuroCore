# NeuroCore – Architektur-Übersicht

## Signalkette

```
Input
  └─► InputRouter
        └─► InputGain
              └─► NoiseGate
                    └─► SignalPolisher
                          └─► [Oversampling ↑]
                                └─► DSL SignalChain
                                      ├── Stage   (mathematische Formel)
                                      ├── Filter  (lowpass / highpass / bandpass)
                                      ├── Comp    (Kompressor)
                                      ├── Env     (Envelope Follower)
                                      └── Osc     (LFO-Oszillator)
                                └─► [Oversampling ↓]
                                      └─► DryWetMixer
                                            └─► AutoGainCompensation
                                                  └─► OutputGain
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
| `WeightedLayout` | Flexibles Layout-System mit gewichteten Spalten/Zeilen |

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
