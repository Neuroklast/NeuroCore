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
| **Formel-Live** | `ExprTape` + asmjit | Parse/Fold/CSE auf dem Message-Thread; Tape ist IR. Windows x64 JITtet Load/Add/Sub/Mul/Neg. macOS VST3/AU: nur Tape (kein asmjit, AU-Sandbox). |
| **Chain-Dispatch** | `Block::processBlock` | Eine Virtual pro Chip pro Callback. Kein `process(ch,x)` mehr in der Basisklasse / per Sample. |
| **Audio-Layout** | `alignas(64)` + `DSPUtils::alignedRing` | History und Delay-Ringe cache-line aligned. Innere Sample-Schleife auf `float*` + `NK_RESTRICT`, kein `std::vector` dort. Knob-Lanes in `prepare`. |
| **Kein Dual-Chain-Audio** | `DspEngine` | Nur `signalChain`; Formula-Wechsel = `switchRamp`, kein old+new Blend. |
| **DSL Multi-Bus** | `BusGraph` + `SignalChain` | Max. 4 Named Buses + `in`/`main`. Send nur rückwärts (DAG). Mixdown **in** der DSL, eine Engine-Timeline. |
| **State-Reset** | `clearRuntimeState` / prepare | ADAA/Delay/Reverb nur bei Formula-Load/prepare, nie pro Block. VST3 sleep/wake (`setNonRealtime` change, JUCE `setProcessing` → `reset`) clears OS/sanitation/rings and fades 256 samples. |
| **Oversampling** | `juce::dsp::Oversampling` | Studio FIR / Live IIR. FIR skippt Null-Taps (`k += 2`). Kein zweites Half-Band. |
| **Release-LTO** | CMake IPO on plugin | `INTERPROCEDURAL_OPTIMIZATION_RELEASE` on NeuroKore + Standalone/VST3/AU. Never global `/fp:fast`. |

Defaults: OS **4×** (`Config::kDefaultOversamplingIndex = 2`), Diagnostics **off**, AutoGain **off**.

Diagnose-Heuristik: Crackle an `smp≈latency` → Timeline. `smp=0` → Control-Rate/State.

## Audio-Runtime (0.4.11) — Implementierung und Performance

Das Audio-Thread-Modell ist **eine Kette, eine Timeline, kein Heap im Callback**. Compile/Parse/Fold laufen auf dem Message-Thread. `processBlock` liest vorbereitete Pointer und feste Arrays.

### Formel-Pfad

| Schritt | Thread | Code |
|---|---|---|
| Parse → AST | Message | `ExpressionEvaluator::parseFormula` |
| Constant-Fold + CSE | Message | gleicher Parse |
| Absenkung auf Opcode-Tape | Message | `lowerToTape` → `ExprTape` (max. 256 Ops, 32 Slots, `alignas(64)`) |
| Optional JIT | Message, nur Windows x64 | `exprTapeJitCompile`: arithmetik inlined (`ss`); Div/Pow/Call/ADAA = `call` to `exprTape*` C helpers. No asmjit Invoke into C++ shapers. |
| Live-Eval | Audio | `liveJit.fn` **oder** `exprTapeEval` (kein virtueller AST, kein `evaluate()`-Lock) |

Dateien: `src/utils/ExprTape.h/.cpp`, `src/utils/ExprTapeJit.h/.cpp`, `src/utils/ExpressionEvaluator.h/.cpp`.

**Warum Tape vor JIT:** Der alte Live-Pfad war ein virtueller AST (`Node::eval`) plus `std::function`. JIT ohne flaches IR hätte zwei Runtimes erzeugt. Das Tape ist die ABI; asmjit emittiert daraus nativen x64-Code in einen RWX-Puffer **beim Load**, nicht im Callback.

**JIT Call/ADAA:** Emit `call` to `exprTapeDiv` / `exprTapePow` / `exprTapeCall1/2/3/5` / `exprTapeCallAdaa` (noexcept C helpers). Do not `invoke` into C++ shapers — that crashed factory load. Failed emit leaves `fn == nullptr` (interpreter).

**macOS VST3/AU:** `NK_HAS_EXPR_JIT=0`. Kein asmjit, kein RWX in der AU-Sandbox. Dieselbe Tape-Maschine.

Alte Chains (JIT-Code, Delay-Ringe) hängen an `retiredChain` und sterben auf dem **Message-Thread** beim nächsten `loadScript`, nicht wenn `processBlock` seinen `shared_ptr` fallen lässt.

### Chain-Dispatch

`SignalChain::Block` hat genau eine virtuelle Audio-API: `processBlock`. `process(ch, x)` ist eine nicht-virtuelle Hilfsfunktion für Tests. Eine Virtual pro Chip pro Host-Callback, nicht pro Sample.

### Speicher / innere Schleife

| Was | Modell |
|---|---|
| Stage/Filter `xPrev`/`yPrev` | `alignas(64) float[kMaxChannels]`, kein `vector::assign` |
| Delay/Comb/Allpass/Widen-Ringe | `DSPUtils::alignedRing`: Pointer 64-Byte, **Wrap-Länge = n** (Padding nur vor dem Pointer) |
| Knob-Lanes | Größe in `prepare` (`maximumBlockSize × 8`), kein `resize` im Callback |
| LUT | `DSPUtils::lutInterp(const float* NK_RESTRICT, int, float)` |
| Macros | `NK_FORCEINLINE` / `NK_RESTRICT` in `Config.h` |

`delayRead` ist **linear + Integer-Wrap** auf `float*`. Hermite-4-Punkt mischte Index 0 mit `N-1` und knackte einmal pro Periode.

### Oversampling (gemessen, nicht ersetzt)

Studio = JUCE half-band FIR Equiripple, Live = polyphase IIR. Die FIR-Convolution läuft bereits `k += 2` (Null-Taps ausgelassen). Eigenes Skip-Zero-OS entfällt — sonst zwei Owner neben der Sanitation-AA. Idle-Skip gibt **latenz-alignierte Dry**, nicht `memset` Nullen (Host-PDC / Mix=0).

Prepare legt die Decke: Host-Puffer auf `maximumBlockSize`, OS-Bank auf `jmax(maximumBlockSize, 1024)` × 8. `processBlock` ruft kein `setSize` / `new` auf. Wenn der Host `n` > Prepare liefert (Cubase ASIO Guard), wird **vor** `processSamplesUp` in Scheiben `≤ preparedHostMax` geschnitten — dieselbe Wet-Kette, kein zweiter OS. Vertrag: `tests/CrackleFixesTest.h` (prepare 64, process 256 und 1024, `scriptBuffer` wächst nicht).

### Compiler

Release-LTO (`INTERPROCEDURAL_OPTIMIZATION_RELEASE`) nur auf NeuroKore + Standalone/VST3/AU. Tests bleiben ohne LTCG. **Kein** globales `/fp:fast` (bricht `isfinite`, NaN-Hold, ADAA). PGO nicht im Baum.

### Bewusst nicht gebaut

PGO-Skript, LLVM, SIMD-JIT (`ps` 4-wide), zweites Half-Band, globales FMA/`fast-math`.

**Tape SIMD:** `exprTapeEvalSimd` is the SIMD path. Arithmetic + LUT (`sin`/`tanh`/…) is lane-independent. ADAA (`softclip`/`tube`/`diode`) stays serial (`exprTapeCanSimd` false, `needsSampleLoop`). Delay rings and saturation LUTs are 64-byte aligned and SIMD-padded. Phase: SIMD vs scalar peak index identical. Contract: `ExpressionEvaluatorTest` “tape SIMD matches scalar and keeps impulse phase”.

Verträge: `tests/ExpressionEvaluatorTest.h` (Tape-Identität, JIT `x*2+1` / `-x`, LoadVar-OOB, Tape-SIMD), `tests/ArchitectureHardeningTest.h` (64-Align, Delay-Wrap, Knob-Lanes, CPU 0–100), `tests/DelayReverbTest.h`, `tests/WebShellTest.h` (JIT-Flag, Mac-Zip-Pfad).

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
- Hält den **APVTS** mit Host-Parametern; DSP liegt in `DspEngine` + `SignalChain`
- `processBlock()` misst CPU, ruft die Engine, schreibt Telemetrie
- Editor: **nur** `WebPluginEditor` (`src/ui/WebPluginEditor.*`), a thin frame. The `WebBrowserComponent` is owned by `WebViewHolder` on this processor so it outlives IPlugView. Native `PluginEditor` ist nicht der Produkt-Default.

### `WebPluginEditor` (`src/ui/WebPluginEditor.h/.cpp`)
- Thin frame around the processor-owned WebView (Windows WebView2, macOS WKWebView)
- Circuit/Terminal/Unit leben in `web/` (React Flow + elkjs)
- Web-Assets: Windows RCDATA `41001`; macOS `Contents/Resources/web` + `neurokore_web_dist.zip`

### WebView lifetime
- Bound to `NeuroKoreAudioProcessor` (`bridge::WebViewHolder`), **not** to the VST3 `IPlugView` / `AudioProcessorEditor`.
- Three lifetimes: processor (scan-safe), IPlugView frame (`getSize`, HWND, splash), Chromium backend. Chromium is never constructed on the host callback stack (`createView` / `attached`). `MessageManager::callAsync` later realizes it only if `shouldRealizeChromium` (still attached, has peer, ticket == epoch). Scan is attached+removed on one stack — the ticket is stale.
- Editor construct: reparent/show the existing browser (or spawn it if this is the first peer). Editor destruct: `removeChild` without deleting it.
- Windows: the WebView2 parent HWND is **never a child of the IPlugView HWND**. VST3 `removed()` `DestroyWindow`s the plugin peer *before* `~WebPluginEditor`. Park is a sibling of IPlugView (child of the host `systemWindow`). Standalone has no IPlugView — the editor peer **is** the app window, so that HWND is the park parent. The hidden processor-owned HWND is only for detach/close. `parentHierarchyChanged` / `visibilityChanged` park while the peer still exists. Each instance gets its own WebView2 user-data folder (`NEUROKORE-webview2/i<id>`).
- Hidden editor keeps the JS heap. Host telemetry at 8 Hz runs only while attached (SPSC `telemetry.bin` is still latest-value).
- Reopen does not rebuild the zip index and does not require a second `UI_READY` (latch is idempotent on the holder).

### Host / Cubase keyboard
- **Intercept + forward**, not focus theft. While the plugin WebView is focused, non-text keys still reach the DAW.
- Capture `keydown` in `web/`; `shouldForwardToHost` keeps text fields, plugin chords (undo / Arrange / Compact / Circuit Delete with selection / overlay / blocked browser chords), and modifier-only keys in the plugin.
- Native `hostKey` payload is `{ key, code, ctrl, alt, shift, meta }`. C++ maps `{ key, code }` via `bridge::HostKeys` (`hostKeyNameToVk` / `hostKeyNameToCgKeyCode`).
- **Windows:** `PostMessage` `WM_KEYDOWN`/`WM_KEYUP` to the Cubase HWND from `chooseHostHwnd` (standalone → nullptr, no-op). Do not synthesise `juce::KeyPress` to the top-level component — WebView2 already consumed the OS event.
- **Mac VST3/AU:** `CGEventPost` with Carbon key codes. **Mac Standalone:** no-op.

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

### `PcbRouter` (nicht der Editor)
- Native Circuit-Router. Produkt-Layout ist elkjs + A* in `web/`. Nicht anfassen für neue Circuit-Arbeit.

### `GraphModel` (`src/dsl/GraphModel.h/.cpp`)
- Editor-Datenmodell (Document). Layout/Routing ist `web/` (React Flow + elkjs), nicht native Canvas
- `parse` nutzt `DSLParser` und hängt `#`-Header/Trailing-Kommentare inkl. `@x,y` an
- `emit` schreibt kanonische DSL; Formel bleibt Source of Truth
- `jacksFor` / `jacksForInput` / `jacksForVirtualOut`: sichtbare Ports (Audio, Mix-Bus, Knob, SC, Mod, MID/SIDE, L/R, Xover)
- `visualRail` / `channelRail` / `visualAudioEdges`: Bus- und MS-Rails in der UI
- `isMsEncode` / `isMsDecode`: Mid-Side forkt wie ein Bus (MID/SIDE), `widen` ist Stereo-Width, nicht MS
- `connectJack` patched eine konkrete Buchse (Mix-Bus, LFO→Parameter, sonst `connectAudio`)
- `semanticallyEqual` vergleicht Typ/Name/Bus/Args und Param-Ranges, ignoriert Kommentartext
- `moveNode` verschiebt Blöcke mit Bus-Regeln (`send` nur im Named Bus, `out` zuletzt)

### Circuit-UI
- Produkt: React Flow in `web/` (`nodesDraggable`, `onConnect`, stored `data.route`). C++ `GraphModel` bleibt Document/emit/parse.

### `SignalChain` (`src/dsl/SignalChain.h/.cpp`)
- Führt die geparsten Blöcke aus. Audio: nur `processBlock` (eine Virtual / Chip / Callback)
- Neue Kette wird gebaut, dann atomar veröffentlicht. Die vorherige hängt an `retiredChain` (Message-Thread)
- Delay: linearer Tap, Integer-Wrap, 64-aligned Ring (`DSPUtils::alignedRing`)
- Sanitation-AA sitzt in der Engine **vor** dem Downsample, nicht im DSL-Delay
- Nach jedem Block: `writeNodeTap` (64 Samples) für Circuit-Glow
- Formelwechsel: `switchRamp` in `DspEngine` (kein Dual-Chain-Blend)

### `ExpressionEvaluator` (`src/utils/ExpressionEvaluator.h/.cpp`)
- Parser/Fold/CSE auf dem Message-Thread. AST ist nicht der Live-Pfad
- Live: `ExprTape` → optional asmjit (Windows x64, nur Arithmetik) → sonst Interpreter
- `evaluateLive` / `bindCompiledUnlocked`: kein Parse-Lock, kein ADAA-Reset pro Sample
- SIMD-`evalSimd` nur für Stage ohne Tape-Sample-Loop (kein Feedback/t/mod/ADAA)
- Funktionen: `docs/DSL_REFERENCE.md`

---

## DSP-Module (`src/dsp/`)

| Modul | Datei | Funktion |
|---|---|---|
| `InputGain` | `InputGain.h` | Eingangs-Verstärkung mit Smoothing |
| `InputRouter` | `InputRouter.h` | Kanal-Routing (Mono/Stereo) |
| `WaveShaper` | `WaveShaper.h` | Waveshaping via LookupTable |
| `SanitationChain` | `SanitationChain.h` | DC → AA → optional clip → True-Peak → residual → dither |
| `LowPassFilter` | `LowPassFilter.h` | Einfacher Tiefpass |
| `DSPUtils` | `DSPUtils.h` | RMS/Kahan, AutoGain, LUT-Interp, `alignedRing`, Soft-Clip |
| `LookupTables` | `LookupTables.cpp` | sin/cos/tanh/exp/log; Interp auf `float*` |

### `DSPUtils`
- **alignedRing** — `prepare`-only; Wrap-Länge unverändert
- **lutInterp** — lineare LUT auf raw `float*`
- **autoGainCompensate** — optional, default strength 0; mutet nie
- **Kahan RMS / DC** — Meter, nicht Audio-Hot-Path

---

## UI-Komponenten (`src/ui/`)

Produkt-Editor ist `WebPluginEditor` + `web/`. Die native Tabelle darunter ist Altbestand im Tree, nicht der Default.

| Komponente | Beschreibung |
|---|---|
| `WebPluginEditor` | Thin editor frame. Browser lives on the processor (`WebViewHolder`). Circuit/Terminal in `web/` |
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

### `ExprTape` / `ExprTapeJit`
- Flache Opcode-Maschine für `evaluateLive`. Interpreter immer; asmjit nur Windows x64 Arithmetik
- ADAA-State (`softclip`/`tube`/`diode`) lebt auf dem Tape, Reset nur bei Parse/prepare

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
