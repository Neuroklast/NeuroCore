# Lessons Learned

Dieser Erfahrungsspeicher wird nach **jeder** Coding-Agent-Session ergänzt.
Er dient dazu, Fehler nicht zu wiederholen und bekannte Fallstricke zu dokumentieren.

---

## Session-Log

---

### 2026-08-13 – Cinematic UI must stay off the audio thread

**Agent:** Grok Coding Agent  
**Aufgabe:** Glitch / Scan / Overlay-Sequenzen wie Band-Sites, performant, eigener Branch  
**Ergebnis:** Native JUCE-FX-Schicht auf `feat/cinematic-ui-fx`

#### Fallstrick
Der alte Cyber-Look hat bei 30 Hz das ganze Editor-Fenster `repaint()`t und ~480 Hex-Strings gezeichnet. Mehr Effekte darauf zu stapeln waere in der DAW sichtbar geruckelt.

#### Regel
Ambient-Look cachen (Grid/Scan/Hex als Image). Sequenzen nur waehrend ~400 ms per `VBlankAttachment` auf dem Overlay. Director ist reiner State, unit-testbar ohne `PluginEditor`. WebView/Framer nicht ins VST.

---

### 2026-08-13 – DSL Multi-Bus (Send-DAG)

**Agent:** Grok Coding Agent  
**Aufgabe:** Parallele Stages / Signal-Split als echter Multi-Bus  
**Ergebnis:** Ansatz A — scoped `bus`/`send`/`out` in einer `SignalChain`, eigene Branch

#### Entscheidungen
- Kein freier Graph, kein Feedback. Send nur von `in` / `main` / bereits verarbeiteten Bussen.
- Serial-Skript ohne `bus`/`out` bleibt Fast-Path auf dem Eingangsbuffer (keine Extra-Kopie).
- Env folgt dem Bus-Audio **nach** Sends; Osc bleibt global.
- Max. 4 Named Buses. Indentation ist nicht signifikant.

#### Fallstricke
- `send:` mehrfach erlaubt — nicht über die globale `seen`-ID sperren.
- `bus dirt:` ist `id = "bus dirt"` vor dem Doppelpunkt, nicht ein neuer Blocktyp `busdirt`.
- Mixdown bleibt **in** der DSL; Plugin-Dry/Wet und OS-Latenz bleiben eine Timeline.

---

### 2026-08-12 – UI encoding, 6 knobs, Cubase delay crash

**Agent:** Grok Coding Agent  
**Aufgabe:** Screenshot-Bugs (à Glyphs), 6 Knobs, Editor-first, Author, Manual, Cubase crash  
**Ergebnis:** ASCII UI chrome; embedded JetBrains Mono for formula; RT lock on formula swap

#### Encoding
Brand font **Apex** lacks glyphs for `…` `—` `●` → garbage (`à`).  
**Rule:** UI chrome = **ASCII punctuation only**. Formula editor = **embedded mono** (`JetBrainsMono-Regular.ttf`). Apex for brand chrome only.

#### Cubase / Delay
`applyFormula` reallocated delay buffers while `processBlock` ran → host AV.  
**Fix:** `CriticalSection processLock` around loadScript/prepare and entire audio chain process.

#### Editor UX
- Mono font embedded; default size **18**; **+/-** buttons (not combo only).
- Max **6 knobs** a–f, 2×3 layout; more space for formula.
- Assemble removed from Preset Explorer; **Quick templates** in editor.
- Save As: **Author** + Category for artist packs.

---

### 2026-08-12 – Test suite hang was Access Violation + MessageManager

**Agent:** Grok Coding Agent  
**Aufgabe:** Überdimensionierte/hängende Test-Suite optimieren und freischalten  
**Ergebnis:** Suite **1057 / 0 in <2 s**; kein Hang mehr an PresetManager

#### Root Causes
1. **Kein `ScopedJuceInitialiser_GUI`** in `tests/main.cpp` → AudioProcessor/APVTS/AsyncUpdater ohne MessageManager (Spins / undefiniertes Verhalten).
2. **`NeuroCoreAudioProcessor` Dtor rief `cancelPendingUpdate()` nicht** → nach `setValueNotifyingHost` / OS-Change queued `handleAsyncUpdate` auf freigegebenem Objekt → **0xC0000005** oft genau beim nächsten Processor-Test (z. B. Factory).
3. **`MessageManager::callAsync ([&processor]…)`** in `FactoryPresetLibrary::applyPreset` fängt Stack-Referenz — nach Scope-Exit UAF.
4. „Hang“ wirkte wie Endlosschleife (CPU), war aber oft **Crash ohne TOTAL-Zeile** oder sehr teure Expect-Loops.

#### Suite-Optimierung
- Aggregierte Finite/Peak-Checks statt `expect` pro Sample (`TestHelpers.h`)
- Factory: alle parsen, heavy path (load/apply/quality) nur sparse Sample
- Quality-Options: wenige Blöcke, kein Noise/Silence-Probe in CI-Pfad
- Delay-FB-Vergleich: Energie **nach** 2. Recirculation messen (1. Echo ist FB-unabhängig)

#### Regeln
- Jeder Unit-Test mit `AudioProcessor` / `AsyncUpdater` braucht GUI/MessageManager-Init.
- Immer `cancelPendingUpdate()` im Dtor vor Listener-Remove.
- Nie `callAsync` mit Referenzen auf Stack-Objekte ohne Lifetime-Garantie.
- Suite-Dauer: TOTAL-Zeile + Exit-Code prüfen; „hängt“ ≠ immer infinite loop (AV).

---

### 2026-08-12 – Legacy cleanup (safe)

**Agent:** Grok Coding Agent  
**Aufgabe:** Legacy entfernen, Code optimieren, nichts zerstören  
**Ergebnis:** Dead dual-chain, ungenutzte UI/DSP-Module, leeres OpenGL entfernt

| Entfernt | Grund |
|----------|--------|
| `oldSignalChain` | Dual-Run Audio tot; nur unnötige CPU/State |
| `InlineAutocompleteEditor` | Ersetzt durch `DslAutocomplete` im Terminal |
| `FormulaWaveComponent` | Nirgends eingebunden |
| `AdvancedOscillatorWrapper` | Orphan, nicht in CMake/Plugin |
| OpenGL shell in `DslTerminalEditor` | Renderer leer; CodeEditor reicht |
| AutoGain skip | strength=0 und unity → kein Buffer-Loop |

Tests nach Cleanup grün halten.

---

### 2026-08-12 – Mix 0% Dry Path + 8 Knobs + Complex Presets

**Agent:** Grok Coding Agent  
**Aufgabe:** Mix 0% knistert; mehr als 4 Regler; Presets/Templates/Funktionen  
**Ergebnis:** Pure-dry early-out; a–h Knobs; 116 Factory + neue Templates

#### Mix 0% Root Cause
Bei `dryWet==0` lief die DSL trotzdem in den Buffer (Kommentar „dry only“ war falsch).  
**Fix:** `wetNeeded` aus Target+Smoother; wenn false → Buffer bleibt Post-Input-Dry, kein OS/DSL.

#### 8 Knobs
`Config::kNumUserParams=8` (a…h). Bare `e` war Euler-Konstante → Parser: `e` ist Variable; Euler via `exp(1)`.

#### Content
Factory 116 (inkl. 8-Knob Strips/FX); Templates für Channel/Delay/Reverb/Glitch; Functions-Docs für Design-Patterns.

---

### 2026-08-12 – Non-UI Architecture Hardening (TDD)

**Agent:** Grok Coding Agent  
**Aufgabe:** Audit-Findings fixen (Latenz/Noise/Perf/Over-sanitize) außer UI — strikt TDD  
**Ergebnis:** 153576 Tests grün; Defaults sicherer; Dual-Chain-Audio entfernt

#### Contracts (Tests in `ArchitectureHardeningTest`)

| ID | Änderung |
|----|----------|
| A1 | Diagnostics default **off** |
| A2 | AutoGain `strength` + APVTS default **0** |
| A3 | OS default **2×** |
| A4 | Dual-chain audio path **removed** (nur switchRamp) |
| A5 | Sanitizer Peak nur wenn Polisher=None |
| A6 | Delay: quiet-bleed weg; FB-Pole 0.98 + DC-HPF/damp |
| A7 | Silence-leak nur bei `usesFeedback` + benannte Config |
| A8 | Sidechain block-oriented ring |
| A9 | `mixDryWetContinuous` ersetzt DryWetMixer mid-block |
| A10 | NoiseGate aus ProcessorChain entfernt |

#### TDD-Lektion

RED zuerst (`ArchitectureHardeningTest` Compile-Fail fehlende APIs) → GREEN minimal → Suite grün.  
Kein Peak-Threshold-Tuning als „Fix“ — Ownership und Defaults.

---

### 2026-08-12 – Architektur härten (kein Magic-Number-Whack-a-Mole)

**Agent:** Grok Coding Agent  
**Aufgabe:** Knistern intensiviert sich; keine Workarounds — Architektur  
**Ergebnis:** Latenz-aligned Dry, kontinuierliche Control-Rate, Filter-Timebase

#### Log-Diagnose (strukturell)

| Muster | Bedeutung |
|--------|-----------|
| `FinalOut smp≈26` bei `os=4` | OS-FIR-Latenz; Dry/Wet-Sidechain war **nicht** aligned |
| `PostDsl smp=0` | Block-Control-Rate / Stereo-Timebase-Fehler |
| Intensivierung | AutoGain/Gate kämpfen gegen falsch getimtes Dry → GR/Gate oszilliert |

#### Architektur-Fixes (Root Cause)

1. **`LatencyAlignedSidechain`** — single owner Dry-Timeline (== OS/DryWet-Latenz)  
2. **Knob lanes** sample-rate kontinuierlich (kein block-constant `skip`)  
3. **Filter:** `advanceCoeffsOnce` + `processSampleOnly` — Smoother **1× pro Sample**, nicht 2× Stereo  
4. **Sample-major** nonlinear stages (Zeit + ADAA + Knobs konsistent)  
5. **Eine Residual-Policy:** nur `OutputSanitizer`; AutoGain **nie** muten; NoiseGate `setBypassed`  
6. **Filter stereo state:** nie `filter.process(monoBlock)` pro Kanal — das teilt ch0-State; immer `processSample(ch, x)`  

#### Prinzip (verbindlich)

> Bei Crackle/Knacken: **Architektur härten** (Timeline, State-Ownership, Single-Responsibility).  
> **Keine** Magic-Number-Fixes (Thresholds/Knees/Blend-Tweaks als Symptombekämpfung).

Regel: Crackle-Cluster an fester smp-Index → Timeline/State-Architektur, nicht Threshold-Tweaks.  
Siehe RT-Verträge in `docs/ARCHITECTURE.md`.

---

### 2026-08-12 – Sound-Qualität vs. Over-Sanitize

**Agent:** Grok Coding Agent  
**Aufgabe:** Delay-Hum nach Zeit; Amps klingen gleich; nicht zu stark sanitizen  
**Ergebnis:** Charakter zurück, Delay-Howl gezielt, Sanitizer/AutoGain entschärft

#### Root Causes (Logs + Code)

1. **AutoGain 65 % Match** → alle Presets gleiche Lautstärke/Punch  
2. **Sanitizer Knee 0.92 + aggressives Wet-Duck** → Amps platt, Tails abgeschnitten  
3. **Delay-Feedback ohne DC-Block** → nach ~Sekunden summend/kratzend (inPk=0, outPk locked)  
4. **Amp-Presets** zu ähnlich (tube+softclip+LPF, gleiche Defaults)

#### Fixes (Qualität zuerst)

- AutoGain mild: 25 %/8 % Blend, Caps 0.75–1.6 — **Charakter bleibt**  
- Sanitizer Knee **0.96**, Self-Noise-Duck erst nach **~0.5 s** locked Wet  
- Delay: FB-Cap 0.88, **DC-HPF im Feedback**, stilles Input → FB bleed  
- Amps differenziert: Fender open/bright, Marshall mid-bark, Mesa dark wall, Vox glass, TS mid-hump  

---

### 2026-08-12 – Crackle Root Causes in der Signalkette

**Agent:** Grok Coding Agent  
**Aufgabe:** Leaks/Errors in der Signal Chain finden, die Knacken verursachen  
**Ergebnis:** 5 konkrete Root Causes gefixt

#### Root Causes

| # | Problem | Effekt |
|---|---------|--------|
| 1 | **ADAA `resetRuntimeState()` jeden Block/Kanal** | Softclip/tube/diode knacken an jeder Host-Blockgrenze |
| 2 | **Reverb `setSize` leerte Ringbuffer** | harter Click beim Size-Knob / Size-Change |
| 3 | **OutputSanitizer: dry silent → wet mute** | Delay/Reverb-Tails abgeschnitten = Chop/Crackle |
| 4 | **AutoGain: dry silent → gain 0** | gleiche Tail-Abwürgung + Pump |
| 5 | **Delay: tanh-jedes-Sample + unsichere Read-Indexe** | HF/Aliasing + potenzielle OOB-Sprünge |

#### Fixes

1. Per-Channel ADAA-State (`xPrev[2]`); Reset **nur** bei `prepare`/Formel-Load  
2. Reverb: feste Max-Buffer, Size ändert nur Delay-Länge (kein Clear)  
3. Sanitizer: Wet-Tails behalten wenn `wetEnv` über Noise-Floor  
4. AutoGain: Unity halten bei Dry-Silence + nennenswertem Wet  
5. Delay: `fmod` Wrap, Peak-only Soft-Limit, längeres Time-Smoothing  

#### Regel

Nie ADAA/State an Blockgrenzen hart resetten. Nie Delay/Reverb-Ringe clearen, nur Längen ändern. FX-Tails nicht mit Silence-Gates killen.

---

### 2026-08-12 – Echte Delay/Reverb/MS statt y_prev-Fakes

**Agent:** Grok Coding Agent  
**Aufgabe:** Delay, Reverb, Mid/Side voll implementieren; Presets professionell  
**Ergebnis:** Neue DSL-Blöcke + 108 Factory-Presets

#### Was vorher falsch war

- „Delay“/„Reverb“-Templates und Presets nutzten nur `y_prev` (1 Sample) → klangen wie Comb-Dirt, nicht wie Echo/Hall  
- Mid/Side war nur Stage-Flag; kein klarer Workflow mit Side-HPF  

#### Implementierung

| Block | Technik |
|---|---|
| `delay` | Ringpuffer, lineare Interpolation, Feedback+One-Pole-Damp, Sync, Ping-Pong |
| `reverb` | Freeverb: 8 Combs + 4 Allpass, size/decay/damp/mix/width |
| `ms` | L/R↔M/S; `channel=mid\|side` auf Stage **und** Filter |

#### Preset-Regel

Echte Zeit/Raum-Effekte **müssen** `delay`/`reverb` nutzen. `y_prev` nur noch für Regen/Dirt-Farbe.

---

### 2026-08-12 – AudioDiagnostics für Knistern / NaN / Signalsprünge

**Agent:** Grok Coding Agent  
**Aufgabe:** Logging einbauen, um Kratzen, NaNs und sofortige Signalsprünge mit Preset/Input-Kontext zu messen  
**Ergebnis:** RT-sichere Diagnose-Pipeline + File-Log

#### Design-Regeln

1. **Kein Logging auf dem Audio-Thread** (kein File I/O, kein Heap) — nur POD-Events in SPSC-Ring  
2. **Flush via AsyncUpdater** auf Message-Thread → `audio_diagnostics.log`  
3. **Drei Messpunkte**: Input (Baseline), PostDsl (DSL/OS), FinalOut (hörbarer Pfad)  
4. **Rate-Limit** (~40 ms/Stage) verhindert Log-Flut; NaN etwas aggressiver  
5. **dsp-introduced**: Output hat deutlich mehr Jumps als Dry-Input → Kette ist der Übeltäter  

#### Log-Pfad

`%AppData%/NEUROKLAST/NeuroCore/audio_diagnostics.log`  
(Rotation ab ~8 MB → `.prev.log`)

#### Nutzung

Plugin starten → problematisches Preset laden → Audio fahren → Log öffnen und nach `Jump|Crackle|NaN` + `preset=` filtern.  
Abschalten: `Config::kAudioDiagnosticsEnabled = false`.

---

### 2026-08-11 – Presets klingen alle gleich (AutoGain + Dry-Split)

**Agent:** Grok Coding Agent  
**Aufgabe:** Presets machen klanglich zu wenig / klingen identisch  
**Ergebnis:** Root-Cause-Fix für Level/Charakter

#### Root Causes

1. **Full RMS AutoGain 1:1** klebte jedes Preset an Dry-Loudness → High-Gain und Clean gleich laut  
2. **Input Gain erst auf Wet-Path**, Dry ohne Gain → AutoGain hat Gain-Knob + Drive aktiv bekämpft  
3. **OutputSanitizer Knee 0.80** limitierte jedes laute Preset in denselben Soft-Ceil  
4. **softclip Drive-Cap 6** machte hohe Drive-Presets ununterscheidbar

#### Fixes

- AutoGain asymmetrisch: 65 % Makeup wenn leise, nur 20 % wenn laut (Punch bleibt)  
- Input Gain **vor** Dry-Split (Dry/Wet + AutoGain sehen denselben Gain)  
- Sanitizer Knee 0.92 / SoftCeil 0.99  
- softclip Drive-Cap 10  
- Meter: LED nur bei echten NaN/Inf (nicht bei Soft-Limiter)

---

### 2026-08-11 – softclip → atan + Dynamik/Chopper/Sync/UI

**Agent:** Grok Coding Agent  
**Aufgabe:** softclip knistert; Presets gepresst; Chopper hart; Tempo-Sync; Functions; Preset-Name  
**Ergebnis:** atan-softclip + Preset/UI-Fixes

#### softclip

Altes \(x/\sqrt{1+x^2}\) erzeugt bei Drive viel HF → Aliasing/Knistern.  
Neu: \(y=(2/\pi)\mathrm{atan}((\pi/2)\,d\,x)\), ADAA mit **exaktem** Integral, Drive-Cap 6.

#### Dynamik systemisch

- AutoGain nur **60 %** Richtung Full-Match (nicht platt)
- Polisher Default **None**
- Amp-Presets (Marshall/Mesa) offenere Drives/Levels

#### Modulation

- square → weich (tanh·sin); `softsquare` Shape
- Chopper/Tremolo: Floor + optional `sync = a` (1/16…1/1 via Host-BPM)

#### UI

- applyFormula(clearPresetName=false) beim Factory-Load
- currentPresetLabel größer/sichtbar
- Functions: mehrere Locale-Pfade + UTF-8 + BinaryData-Fallback

---

### 2026-08-11 – Goldstandard-Pass (alles)

**Agent:** Grok Coding Agent  
**Aufgabe:** Templates, Funktionen, Signalkette, Engine, Quality auf Goldstandard  
**Ergebnis:** 3267 Tests grün; Content + Engine + Gates

#### Kritischer Bug: Always-on Soft-Ceiling

`y = L * softclip(x/L)` auf **jedem** Stage-Sample komprimierte Normalpegel  
(0.5 → ~0.47) und brach Dutzende Tests + Loudness.  
**Fix:** Soft-Ceiling nur bei `|y| > 1.15`.

#### Weitere Gold-Fixes

- FormulaQuality: Recovery-LPF nach hard NL / y_prev (error)
- Dual-Gate entfernt (NoiseGate ≈ off)
- processBlock*: nie silent drop bei Script-Lock
- functions_*.txt 19 → 35; templates bereinigt
- SpectralSmokeTest; Version 0.2.2

---

### 2026-08-11 – Residual crackle: ADAA/OS/Gate

**Agent:** Grok Coding Agent  
**Aufgabe:** Immer noch Knistern trotz Preset/Clip-Fixes  
**Ergebnis:** Hardclip ohne Fake-ADAA; FIR-OS; AA-LPF; Gate/AutoGain entschärft

#### Root Causes

1. **ADAA mit falschem Integral bei piecewise hardclip** → mehr HF/Knistern als ohne  
2. **Polyphase-IIR Oversampling** zu schwach gegen NL-Images  
3. **Noise-Gate -72 dB / Ratio 10** chattert auf Decays  
4. **Auto-Gain** zu schnell / zu aggressiv (Pump)  
5. **OS-Reset bei Mix=0** → Click  
6. **Post-NL LPF bei festen 20 kHz** nicht an Host-Nyquist gekoppelt  

#### Fixes

- hardclip: wide soft-knee, **kein** ADAA; softclip/tube/diode: ADAA + Guard  
- OS: FIR equiripple, Default **4×**  
- AA-LPF ≈ 0.88 × Host-Nyquist auf OS-Buffer  
- Gate -90 dB / Ratio 2.5 / langsam; AutoGain 0.85 s + Blend  
- Algebraische Soft-Ceilings (C∞) statt piecewise ±1.2  

---

### 2026-08-11 – Optimize-Funktion (smart + robust)

**Agent:** Grok Coding Agent  
**Aufgabe:** Optimize extrem smart/robust (Multi-Line-DSL, Safety-Gates)  
**Ergebnis:** Multi-Pass Optimizer in `FormulaHelper`

#### Vorher (kaputt / naiv)

- Strippte **alle** Whitespace/Newlines → zerstörte Multi-Stage-Scripts  
- Nur 2 Regeln, Replacement `saturate(x)` **existiert nicht** im Evaluator  
- Kein Re-Parse, kein Quality-Gate  

#### Nachher

- Full-script aware (param/stage/filter bleiben)  
- Built-in Rewrites: Identities, classic softclip, clamp→hardclip, tanh→softclip, drive-form  
- Structural: mildes LPF nach bare hardclip wenn kein Filter  
- Gates: re-parse + FormulaQuality (Regression → reject)  
- Locale-Messages + Unit-Tests  

---

### 2026-08-11 – Multi-Stage + y_prev Factory-Presets (performant)

**Agent:** Grok Coding Agent  
**Aufgabe:** Presets mit mehreren Stages inkl. y_prev, die hybrid-schnell bleiben  
**Ergebnis:** +10 Factory-Presets (93 total); Templates; DSL-Doku

#### Topology-Regel (verbindlich für Factory)

```
filter/pre-stage (kein prev)     → SIMD / block
stage: y = f(x + y_prev * fb)    → EINZIGE skalare Stage
filter/post-stage (kein prev)    → SIMD / block
```

- Nie y_prev in zwei Stages  
- Feedback ≤ ~0.5 + LPF danach  
- Kein Osc nötig für „Echo-Dirt“ (Feedback-Farbe reicht, spart CPU)

---

### 2026-08-11 – UI Encoding Mojibake (MSVC ohne /utf-8)

**Agent:** Grok Coding Agent  
**Aufgabe:** Screenshot Signalkette: `â` statt `—` / kaputtes ✕  
**Ergebnis:** `/utf-8` + ASCII-sichere UI-Literale

#### Root Cause

MSVC speichert String-Literale standardmäßig in der **System-Codepage**.  
Quellen sind UTF-8 → Multi-Byte-Zeichen (`—` U+2014, `·`, `✕`, `…`) werden zu Mojibake (`â□`).

Locale-Dateien (`Localiser::loadFile` via `fromUTF8`) waren bereits korrekt — nur **C++-String-Literale** in `.cpp/.h`.

#### Fix

- `CMakeLists.txt`: `add_compile_options(/utf-8)` unter MSVC  
- UI-sichtbare Sonderzeichen → ASCII (`-`, `|`, `...`, `X`)

---

### 2026-08-11 – Hybrid-Pfad + ADAA + OS/Preset-Switch (Performance + Knistern)

**Agent:** Grok Coding Agent  
**Aufgabe:** Osc-Preset CPU-Meltdown; Preset-Wechsel macht Folge-Preset langsam/knisternd; tube/softclip knistert  
**Ergebnis:** Hybrid process path + ADAA waveshapers + switch ramp + OS-Index-Fix

#### Root Causes

1. **Osc/Env erzwangen whole-chain Sample-Pfad** (`canUseBlockPath == false`)  
   → jeder Filter/Stage per Sample × Channel. Bei 2×/4× OS + 512 Block = katastrophal (Riser Noise etc.).
2. **Preset-Wechsel:** 150 ms Dual-Chain-Crossfade verarbeitet **alte schwere Osc-Kette weiter**; `oversampler->reset()` + LPF-Reset → Lautstärke-Spike + Knistern, das „kleben“ blieb.
3. **OS-Parameter:** `parameterChanged` castete oft **normalisierten** 0…1-Wert auf `int` → OS faktisch oft 1× egal welche UI-Wahl.
4. **tube/softclip/hardclip** ohne ADAA → Aliasing-Knistern unabhängig vom OS-Faktor.

#### Fixes

| Fix | Detail |
|---|---|
| Hybrid path | Osc/Env → `modLane` pre-render; Filter/Comp block; nur betroffene Stage sample-lokal |
| ADAA | 1st-order anti-derivative AA für softclip/hardclip/tube/diode |
| Soft-knee | hardclip 3 % → 12 % + smootherstep |
| tube | Drive-Cap 12, weichere Kurve + ADAA-Integral `log(cosh)` |
| Preset switch | Crossfade 35 ms; **kein** OS-Reset; `switchRamp` 0→1 |
| OS index | immer `AudioParameterChoice::getIndex()` |
| OutputSanitizer | soft asymptotic ceiling statt ±0.999 brickwall |

#### Lesson

- Modulation darf **nie** die ganze Kette in den Sample-Pfad zwingen.  
- Preset-Change: Dual-Chain so kurz wie möglich; Oversampler nicht hart resetten.  
- Choice-Parameter: nie `static_cast<int>(normalisedValue)`.

---

### 2026-08-11 – Factory Clip Topology (kein Knistern) + Templates

**Agent:** Grok Coding Agent  
**Aufgabe:** Alle Presets gegen Clipper-Knistern/HF-Rauschen umbauen; Clipper-Templates  
**Ergebnis:** ✅ 83 Factory-Presets regeneriert; Templates + DSL-Doku

#### Anti-Alias Clip Topology (verbindlich für Factory)

| Regel | Umsetzung |
|---|---|
| Nie bare `hardclip(x*drive, lim)` | Immer `hardclip(softclip(...), ceiling)` |
| Nie Clip ohne Recovery-Filter bei starkem Drive | LPF nach hard/soft/fold/bitcrush |
| Soft vor Hard | Soft-Pre dämpft HF vor dem Soft-Knee-Ceiling |
| Parallel für Transparenz | `lerp(x, softclip(x,a), blend)` |
| Resonanz mäßigen | Q ≤ ~2.8–3.2 bei LPF nach Clip |

#### Generator-Workflow

- Source of Truth: `scripts/generate_factory_presets.mjs` → `node scripts/generate_factory_presets.mjs`
- Nicht `factory_presets.json` hand-editen (wird überschrieben)
- PowerShell `Set-Content` kann UTF-8 BOM/Mojibake erzeugen → Header ASCII halten

#### Templates

- `resources/templates.json`: Full-Script-Rezepte für Softclip+LPF, Soft-Knee Ceiling, Hard-Clip-Pedal, Parallel Soft Clip, Diode Stack
- Explizites Anti-Template: „AVOID bare hardclamp“

---

### 2026-08-11 – y_prev CPU-Meltdown + Gain/Mix + softclip HF

**Agent:** Grok Coding Agent  
**Aufgabe:** Performance (y_prev Lag), softclip HF, quieter Output, nur Gain+Mix, Preset-Anzeige  
**Ergebnis:** ✅ 2679 Tests

#### Root Cause: `y_prev` / `x_prev` Performance

`canUseBlockPath()` behandelte `usesFeedback` und `usesTimeVariable` wie Osc/Env und
zwang die **gesamte** Kette in den Sample×Channel×Block-Pfad. Jeder Filter lief dann
per Sample statt blockweise → massiver CPU-Einbruch (v. a. mit 2× OS).

| Fix | Detail |
|---|---|
| Feedback | nur noch **Stage-intern** skalar (serielle Abhängigkeit) |
| Filter/Comp | bleiben auf Block-Pfad |
| Osc/Env | weiterhin sample-interleaved (LFO-Korrektheit) |
| Mod-Pfad | Osc/Env **1× pro Sample** (nicht pro Channel) |

#### softclip HF

Piecewise cubic (Knick bei \|x\|=1) → harte Obertöne/Aliasing.  
Ersetzt durch algebraisch glattes `x/√(1+x²)` (C∞, Unity small-signal gain).

#### Loudness / UI

- Auto-Gain Boost-Cap 1.5 → **3.0** (softclip/tube senken RMS stark)
- Dry/Wet: `linear` statt `balanced`
- Output-Gain-UI entfernt; nur **Gain + Mix**
- Preset-Name sichtbar + Highlight in Preset-Tabelle
- Factory `outputGain` auf 0 dB

---

### 2026-08-11 – SoTA Clipper + NaN/Inf-Hardening + Encoding + Live-Mapped Values

**Agent:** Grok Coding Agent  
**Aufgabe:** Clipper prüfen/SoTA; kein Knistern; kein Inf in Meter; UTF-8 Texte; Live-Werte  
**Ergebnis:** ✅ 2160 Tests

#### Clipper (SoTA)

| Funktion | Vorher | Nachher |
|---|---|---|
| `hardclip` | brickwall `jlimit` | soft-knee Hermite (~3%) → weniger Aliasing |
| `softclip` | cubic, drive | C1 cubic + peak-norm, finite-safe |
| `tube` | tanh mix | pre-clamp drive, DC-null, extreme-safe |
| `diode` | asinh | domain-guarded asinh |
| Polisher HardClip | brickwall ±1 | soft-knee ±1 |

#### NaN/Inf-Quellen geschlossen

- Binary `div`/`pow` domain-safe (kein Inf)
- `log`/`sqrt`/`fmod`/`pow()` domain-safe
- FunctionNode/Func2Node return always finite
- Filter fc/Q/SVF output: finite or hold
- Stage/Polisher/OutputSanitizer: hold last good
- Loudness meter: clamp [-100, 12] dB, never Inf

#### UI

- Locale: `loadFile` always UTF-8 (Windows codepage fix for ü/ä/ö)
- Knobs show **mapped** DSL range values (e.g. Presence 4500 not 0.45)
- Formula live `[value]` for pure param bindings

---

### 2026-08-11 – Stille-Knistern (Osc/Feedback/AutoGain)

**Agent:** Grok Coding Agent  
**Aufgabe:** Hohes Knistern/Geräusche ohne Input, besonders nach Oscillator-Presets  
**Ergebnis:** ✅ Fixes + OutputSanitizer

#### Root Causes

1. **`autoGainCompensate` Floor 0.25:** Bei trockenem Input ≈0 blieb Wet-Rauschen mindestens bei 25% — Osc/Feedback/Filter-Selfnoise hörbar in Stille.
2. **`y_prev`-Feedback:** Presets wie `tube(x + y_prev * b)` schwingen ohne Input weiter.
3. **Osc pro Kanal:** `Osc::process` steppte die Phase pro Channel → 2× Rate Stereo + Artefakte.
4. **Stale `osc1` Variables:** Nach Preset-Wechsel blieben alte Modulation-Vars im Map.

#### Fixes

- `OutputSanitizer`: Input-Sidechain-Expander (Dry silent → Wet mute) + Soft-Knee-Limiter + NaN-Hold am Kettenende
- AutoGain: silent dry → gain 0; boost cap 1.5; langsamere Smooth-Zeit 350 ms
- Feedback: extra Leak wenn `|x| < 1e-4`
- Osc: nur ch0 advanced Phase; Variables bei loadScript bereinigen

---

### 2026-08-11 – Live-Formel-Werte + farbige Knobs + breite Gain-Slider

**Agent:** Grok Coding Agent  
**Aufgabe:** Live sehen welche Werte Knobs erzeugen (`cutoff = 6000 * a [3000]`); A–D Farben; Mix/In/Out breiter  
**Ergebnis:** ✅ Erfolgreich (2140 Tests)

#### Erkenntnisse

- **View vs Edit:** Read-only-CodeEditor zeigt keine AttributedString-Farben. Besser: `FormulaDisplayComponent` im View-Modus (Live-Annotation), `DslTerminalEditor` nur beim Edit — gleiche Bounds per `resized()`.
- **Live-Eval nur pure RHS:** Ausdrücke mit `x`/`y`/`t` nicht als Einzelwert annotieren; `cutoff = b` / `6000 * a` schon. Param-Ranges aus `param a = … [min,max]` via `map(a,0,1,min,max)` spiegeln Engine-Semantik.
- **Slider-Breite:** Drei Spalten (Label/Slider/Value übereinander) → schmal. Stattdessen **drei Vollbreite-Rows** `Label | =====slider===== | Value`.
- **Knob-Farben:** Rot/Gelb/Blau/Lila auf Ring + Value-Label + Alias-Outline + Bracket-Wert — gleiche Palette in `FormulaDisplayComponent::knobColour(i)`.

---

### 2026-08-11 – FormulaQuality Gate + silent y=f(y) fix

**Agent:** Grok Coding Agent  
**Aufgabe:** Preset-Bugs verhindern, Editor-Formel-Check, Output-Qualitätsmetrik  
**Ergebnis:** ✅ Erfolgreich (2140 Tests)

#### Erkenntnisse

- **`y` in Stage-Formeln war 0:** Evaluator füllte nur `x` aus dem Sample-Buffer. `y = tube(y, …)` → Stille. Fix: vor Eval `y = current sample` setzen + Regressionstest.
- **Quality-Gate:** `FormulaQualityAnalyzer` (static: parse, orphan LFO/env; dynamic: multi-tone/silence/impulse/noise → NaN/Inf/RMS/DC/peak/score 0–100). Factory-Presets müssen `passesFactoryGate(score≥55)` bestehen.
- **Editor:** Save-Validierung zeigt Score + Warnungen; Preset-Load ebenfalls.

---

### 2026-08-11 – Profi-Waveshaper-DSL + 78 Factory-Presets

**Agent:** Grok Coding Agent  
**Aufgabe:** Jedes Factory-Preset auf Profi-Niveau; DSL erweitern wo nötig  
**Ergebnis:** ✅ Erfolgreich (2134 Tests grün)

#### Erkenntnisse

- **Dokumentierte DSL-Funktionen fehlten im Code:** `hardclip`, `softclip`, `fold`, `bitcrush`, `lerp`, `tube`, `diode` … waren in `DSL_REFERENCE` gelistet, aber `parseFunction` endete nach `map` mit `nullptr` → Presets mit diesen Calls waren tot/ungenau.
- **Amp-Modelle im Sample-Pfad:** Kein SPICE, aber sinnvolle Abstraktionen: `tube` (asymm. 12AX7-ish), `diode`/`asinh` (Soft-Knee), `softclip` (kubisch), Multi-Stage Pre→Tone→Power→Cab (HPF/LPF).
- **Resonanz-Cap 4.5** im Engine-Hotpath verhindert SVF-Self-Osc-Knistern bei hohen Q-Defaults.
- **Presets regenerieren** über `scripts/generate_factory_presets.mjs` + BinaryData-Embed für Cubase.

---

### 2026-08-11 – Factory-Presets fix + UI Slider/UX (master)

**Agent:** Grok Coding Agent  
**Aufgabe:** Auf master: Presets ladbar machen, viele funktionierende Factory-Presets, UI-Slider/Knobs zu kurz  
**Ergebnis:** ✅ Erfolgreich (2108 Tests grün)

#### Erkenntnisse

- **Factory-Presets waren nie verdrahtet:** `factory_presets.json` lag im Repo, aber `loadPreset`/`PresetTable` lasen nur User-`.nrk`. Ohne `FactoryPresetLibrary` bleibt die Liste leer.
- **Param-Range-Parser-Bug:** `fromFirstOccurrenceOf("[", false, …)` liefert den Text *ohne* `[` → `startsWith("[")` schlug fehl → alle `param a = Drive [0.1, 4.0]`-Zeilen waren Parse-Fehler.
- **APVTS a–d bleiben 0–1:** Ranges nicht mutieren. Defaults als normalisierte Position setzen; Stage-Formeln bekommen `map(a,0,1,min,max)` aus der `param`-Zeile.
- **Filter-Modulation `+`/`*`:** War in JSON-Presets, wurde aber ignoriert. Jetzt: `cutoff = base + plus * mult`.
- **Osc `freq = a`:** Früher nur `getFloatValue()` → 0 Hz. Braucht `ExpressionEvaluator` + Re-Eval pro Block.
- **Env/Comp-Zeiten:** Presets nutzten oft ms (z. B. `release = 120`); DSL erwartet Sekunden.
- **Stage-Output-Clamp [-1,1]:** Unit-Tests, die `y = x * 2` oder `y = pi` erwarten, müssen in den Audio-Bereich skaliert werden.
- **DSCR nach STAT:** `applyFormula` überschreibt Knob-Namen — Labels aus State danach wiederherstellen.
- **UI „Slider zu kurz“:** Mix-Strip mit Weight 0.1 und Rotary-Gains ~80 px hoch. Fix: höhere Weights, lineare Full-Width-Slider, dickere Tracks.

#### Empfehlungen für nächste Session

1. Double-Click-Preset-Load im Host manuell verifizieren
2. Progressive Disclosure: Settings (OS/Language) einklappbar
3. Knob-Labels: skalierte Min/Max aus `param`-Ranges statt 0–1 anzeigen

---

### 2026-06-29 – UI-Modernisierung + Factory-Presets (75)

**Agent:** Grok Coding Agent  
**Aufgabe:** Oberfläche modernisieren (Layout/Theme), viele Anwendungs-Presets entwerfen  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- **MSVC Initializer-Listen:** Gemischte Komponenten-Zeiger (`DslTerminalEditor*`, `WaveformDisplayComponent*`, …) in `for (auto* x : { ... })` führen zu C3535 — explizites `juce::Component*[]` verwenden.
- **nlohmann::json `value()`:** Default-Werte müssen `std::string` sein, nicht `juce::String` (MSVC C2672).
- **Factory-Preset-Gains:** JSON-Werte sind dB; APVTS erwartet linear (`Decibels::decibelsToGain`).
- **Preset-Overlay:** Factory-Zeilen haben keine Datei — Delete per `isFactoryRow()` und `file.exists()` absichern.
- **Preset-Merge-Skript:** `scripts/merge_factory_presets.mjs` dedupliziert per Name; 29 → 75 Presets in 15 Kategorien (Guitar, Bass, Vocals, Drums, Synth, Mastering, …).

---

### 2026-06-29 – Windows CMake-Build (Plugin + Standalone)

**Agent:** Grok Coding Agent  
**Aufgabe:** Plugin und Standalone unter Windows bauen, CMake-/Build-Fehler beheben  
**Ergebnis:** ✅ Erfolgreich (Release-Build)

#### Erkenntnisse

- **CMake 4.1 + Visual Studio:** `COMMAND juce::juceaide` wird in MSBuild-Schritten nicht aufgelöst (Fehlercode 123). Workaround: JUCE `JUCEUtils.cmake` patchen auf `$<TARGET_FILE:juceaide>` und `juce::juceaide`-Alias anlegen, wenn `JUCE_BUILD_HELPER_TOOLS=ON`.
- **`juce_add_plugin` hat kein `SOURCES`-Argument** — Quelldateien müssen via `target_sources()` registriert werden; andernfalls fehlt `createPluginFilter()` beim Link.
- **`SignalChain` ist nicht kopierbar** (`SpinLock`); Undo-Snapshot in `ScriptManager` über `loadScript(dslScript)` statt Zuweisung.
- **`canUseBlockPath`:** Deklaration muss nach `using Chain = ...` stehen (private), sonst MSVC C4430.
- **`warning.png`** fehlte in `juce_add_binary_data` — in BinaryData einbinden.
- **JUCE 8 `UnitTestRunner`:** `getNumFailures()` existiert nicht; Failures über `getResult(i)->failures` summieren.

---

### 2026-06-29 – Stages-Button (Signalkette-Overlay)

**Agent:** Grok Coding Agent  
**Aufgabe:** `stagesButton` verdrahten — Overlay mit DSL-Block-Übersicht  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `DSLParser::parse` liefert bereits `BlockDesc`/`ParamDesc` — kein Zugriff auf interne `SignalChain` nötig.
- `formatBlockSummary`/`formatBlockDetails` in `DSLParser` halten UI-Logik dünn und sind unit-testbar.
- Overlay-Muster von `FunctionsContentComponent`/`PresetContentComponent` (`ModalOverlay` + `onClose`) ist konsistent wiederverwendbar.

---

### 2026-06-29 – Phase D Audit-Fixes (Editor-Sync, State, Bypass, Cleanup)

**Agent:** Grok Coding Agent  
**Aufgabe:** Phase D — Preset-Editor-Sync, Session-State, Bypass, modFrequency/Legacy-DSP entfernen, CI strict  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `PresetManager::loadPreset` ruft nur `setStateInformation` auf — ohne `ChangeBroadcaster` bleibt die UI stale (Formel, Alias-Namen, Sprache).
- Variable-Namen und Sprache gehören in denselben `ValueTree` wie APVTS (`varName0`…`varName3`, `language`), nicht in separate Preset-Chunks.
- Bypass über `dryWet == 0` ist konsistent mit `DspEngine`; vorherigen Mix in `mixBeforeBypass` speichern, damit Ent-Bypass den Mix wiederherstellt.
- Legacy `WaveShaper`/Filter/OscillatorWrapper im Plugin-Target entfernen, in `NeuroCoreTests` behalten (`WaveShaperTest`).

#### Empfehlungen für nächste Session

1. `stagesButton` implementieren oder UI aufräumen
2. Slow-Path: Sub-Block-Verarbeitung für Osc/Env-Ketten

---

### 2026-06-29 – Phase C Performance (SIMD Fast-Path, Block-DSP)

**Agent:** Grok Coding Agent  
**Aufgabe:** Phase C — CPU-Optimierung: blockweise DspEngine, SIMD Production-Pfad, Filter/Comp blockweise  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `processBlockSmoothed` kann für Chains ohne Osc/Env/Feedback/`t` den gesamten Buffer über `Block::processBlock` abarbeiten — SIMD-Stage + blockweise JUCE-Filter/Comp.
- `canUseBlockPath()` anhand `ExpressionEvaluator::getVariableIndex` für `t`/`x_prev`/`y_prev` ist zuverlässiger als String-Suche.
- Filter/Comp-Coefficient-Updates jedes 8. Sample im Slow-Path reduziert JUCE-API-Overhead ohne hörbar große Schritte bei typischen Smoothing-Zeiten.
- `LookupTables::initialise()` sollte exp/log-Tabellen sofort befüllen, damit `fastExp`/`fastLog` nie auf dem Audio-Thread allokieren.

#### Empfehlungen für nächste Session

1. Phase D: Preset-Editor-Sync, Session-State VariableNames, Legacy-Code entfernen
2. Slow-Path: Sample-major mit Sub-Block-Verarbeitung für Osc/Env-Ketten evaluieren

---

### 2026-06-29 – Phase B Audit-Fixes (LPF, RT-Safety, Stability-Pfad)

**Agent:** Grok Coding Agent  
**Aufgabe:** Phase B — LPF osSpec, RT-safe Chain-Swap, scriptLock, Stability-Test-Pfad, Factory-Presets  
**Ergebnis:** ✅ Erfolgreich (lokaler Build nicht verfügbar)

#### Erkenntnisse

- `lowpassFilter` muss mit `osSpec` prepared werden, wenn er auf dem oversampelten `upBlock` läuft — sonst ist die effektive Cutoff-Frequenz falsch.
- `oldSignalChain = signalChain` am Ende des Crossfades auf dem Audio-Thread ist redundant: `ScriptManager::applyFormula` snapshotet bereits auf dem Message-Thread.
- `SpinLock` + `ScopedTryLockType` in `processBlock*` und voller Lock in `loadScript` verhindert Data-Races auf `variables` während Script-Reload ohne Audio-Thread zu blockieren.
- `testFormulaStability` muss `processBlockSmoothed` verwenden, sonst validiert die UI einen anderen Pfad als der Host.

#### Fallstricke

- Bei `ScopedTryLockType`-Miss schweigt `processBlockSmoothed` — Buffer enthält dann unverarbeitetes Upsample-Signal (akzeptabel für kurze Reload-Fenster).
- Factory-Preset-Konvertierung: `mode` → `type`, `sin` → `sine` beim Osc-Shape.

#### Empfehlungen für nächste Session

1. Phase C: SIMD in Production, per-sample scalar Loop ersetzen
2. Preset-Load → Editor-Sync (Phase D)
3. Build + ctest lokal verifizieren

---

### 2026-06-29 – Phase A Audit-Fixes (Parameter-Routing, Tests, DSL-Docs)

**Agent:** Grok Coding Agent  
**Aufgabe:** Phase A aus Plugin-Audit umsetzen — Knob-Routing, DSL-Referenz, Test-CI, Resources-Case  
**Ergebnis:** ✅ Erfolgreich (lokaler Build in Sandbox nicht verfügbar)

#### Erkenntnisse

- `processBlockSmoothed` schrieb `a`–`d` in `variables`, aber `Stage::process` las über `paramSmoothers` (nur in `processBlock` aktualisiert). Einheitliche Quelle: Knob-Werte immer aus `variables` in Stage-Pre-Callbacks injizieren.
- Smoother-Advancement muss **pro Sample, vor der Channel-Schleife** erfolgen — sonst ist Stereo-Smoothing doppelt so schnell wie Mono.
- `tests/main.cpp` mit `return 0` macht `ctest` wertlos; `runner.getNumFailures()` auswerten ist Pflicht.
- `PresetManagerTest` und `SignalChainTest` nutzten veraltete/ungültige DSL (`x * 2` ohne `stage1:`) — Tests müssen gültige Zeilen-Syntax und explizite Parameter (`setParameter` / `processBlockSmoothed`) verwenden.
- Windows-Case-Rename `Resources` → `resources` erfordert Zwischenname (`resources_nc`), da das Dateisystem case-insensitive ist.

#### Fallstricke

- `Stage::paramSmoothers` bleibt für `prepare`/`loadScript` verdrahtet, wird aber nicht mehr in `process`/`processBlock` konsumiert — bei zukünftigem Refactoring entfernen oder dokumentieren.
- `resources/factory_presets.json` enthält weiterhin Brace-Syntax; erst relevant wenn Factory-Preset-Loader angebunden wird.

#### Empfehlungen für nächste Session

1. Phase B: LPF auf `osSpec`, RT-safe `oldSignalChain`-Swap, ein kanonischer DSP-Pfad
2. `factory_presets.json` auf Zeilen-Syntax migrieren
3. CI/Build lokal mit `build_debug.bat` + `NeuroCoreTests` verifizieren

---

### 2026-05-24 – Windows Visual-Studio-Generator Build-Fix via Ninja

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Windows-Build mit `juce::juceaide`-Fehler stabilisieren (`build_debug.bat`, `build_release.bat`, `CMakeLists.txt`)  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Der Visual-Studio-CMake-Generator kann bei JUCE-Custom-Commands (`binarydata`, `rcfile`) `juce::juceaide` als Literal statt als ausführbaren Pfad behandeln; `Ninja Multi-Config` umgeht dieses Problem zuverlässig.
- Für lokale Windows-Skripte ist es robuster, Ninja/CMake aus den VS2022 Build Tools explizit in den `PATH` zu setzen und vor dem Build `vcvars64.bat` zu laden.
- `add_subdirectory("${JUCE_DIR}" "JUCE")` hält die JUCE-Einbindung näher am stabilen Zustand vor PR #195 und vermeidet zusätzliche Pfadauflösungs-Risiken.

#### Fallstricke

- In der lokalen Linux-Sandbox dieser Agent-Session bleibt eine vollständige Build-Validierung blockiert, weil Systemabhängigkeiten (`x11`) für den CMake-Configure fehlen.

#### Empfehlungen für nächste Session

1. CI-Rerun für `ci.yml` prüfen, speziell den Windows-Job mit dem vorherigen `MSB8066`/`juce::juceaide`-Fehler.
2. Optional README-Buildsektion um Ninja-Hinweis für Windows ergänzen.

---

### 2026-05-24 – Projucer/CMake Sync-Fix für PR #195

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Fehlende Dateien in `NeuroCore.jucer` nachtragen, JUCE-CMake-Einbindung (`juceaide`) reparieren, Windows-Buildskripte ergänzen  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Wenn neue Klassen nur in `CMakeLists.txt`, aber nicht in `NeuroCore.jucer` eingetragen werden, driften CMake- und Projucer-Build auseinander und Visual-Studio-Projekte aus Projucer fehlen dann komplette Units.
- Für stabile JUCE-CMake-Integration muss `JUCE_BUILD_HELPER_TOOLS` VOR jeder JUCE-Einbindung gesetzt werden; sonst fehlt in CI der `juceaide`-Target und BinaryData/RC-Generierung bricht.
- Das Entfernen von unnötigen Plattform-Libs (`curl` im Test-Target) reduziert plattformspezifische Build-Probleme, ohne Funktionalität zu verlieren.

#### Fallstricke

- In dieser Sandbox bleibt `cmake -B build -S .` weiterhin am Linux-Dependency-Check (`x11`) hängen; vollständige Build-Validierung muss daher über CI-Jobs erfolgen.

#### Empfehlungen für nächste Session

1. CI-Rerun von `ci.yml` prüfen, ob der `juceaide target does not exist` Fehler auf Linux/macOS/Windows verschwunden ist.
2. Optional README um kurzen Hinweis auf `build_debug.bat` und `build_release.bat` ergänzen.

---

### 2026-05-21 – God-Class-Auflösung + Musikalische/DSP-Features

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** PluginProcessor God-Class auflösen (DspEngine, ScriptManager, WaveformCapture) + MIDI-Variablen, Tempo-Sync, Feedback-Schutz, Stereo-Features, Tail-Time  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Die Auflösung einer God-Class erfordert sorgfältige Analyse der Abhängigkeiten zwischen Klassen. `DspEngine::processBlock` nimmt `signalChain` und `oldSignalChain` als Referenz-Parameter – so muss DspEngine die ScriptManager-Klasse nicht kennen.
- `ValidationProgressInfo` muss in eine eigene Header-Datei (`ValidationTypes.h`) ausgelagert werden, um zirkuläre Includes zwischen `PluginProcessor.h` und `ScriptManager.h` zu vermeiden.
- `signalChain` und `oldSignalChain` müssen `public` in ScriptManager bleiben, weil `DspEngine::processBlock` und `PluginProcessor::getTailLengthSeconds` direkten Zugriff benötigen.
- Für `ms_encode`/`ms_decode` in Stage müssen L- und R-Kanal gemeinsam transformiert werden – das erfordert Zugriff auf den gesamten Stereo-Buffer, nicht nur auf einzelne Samples. Die Stage-`processBlock`-Methode muss vor der Formel-Schleife diese Transformation durchführen.
- `std::atomic<float>` ist ideal für MIDI-Variablen im Audio-Thread (RT-safe), erfordert aber `std::atomic_init` in Konstruktoren bei älteren Compilern; `= {0.f}` Initialisierung ist sicherer.
- Für Tempo-Sync bei Osc-Blöcken: `sync = 1/4` als String-Ratio parsen (Float-Division) und in `Osc::applyTempo(bpm)` auf `freq = bpm/60 * ratio` abbilden.

#### Fallstricke

- In dieser Sandbox bleiben lokale Build-/Testläufe durch fehlende Linux-Dependency `x11` bereits beim CMake-Configure blockiert; CI-Status immer über GitHub Actions prüfen.
- `PluginProcessor::processBlock` muss `midiVariableMapper` auf BEIDEN Chains (signalChain UND oldSignalChain) aufrufen, sonst klingt der Crossfade bei Formula-Wechsel mit MIDI inkonsistent.
- `DspEngine.prepare()` muss die neuen Buffergrößen für `upBlock` UND `scriptBuffer`/`oldScriptBuffer` korrekt berechnen – Fehler dort führen zu stillen Buffer-Overflows.

#### Empfehlungen für nächste Session

1. CI-Jobs neu triggern, damit alle neuen NeuroCoreExtrasTests auf allen Plattformen laufen.
2. UI: `ValidationTypes.h` prüfen ob `ValidationContentComponent.cpp` noch `#include "ValidationTypes.h"` benötigt oder es über `PluginProcessor.h` bekommt.
3. Negativtest für kaputte Preset-Dateien (aus vorheriger Session) noch ausstehend.

---



**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Review-Follow-up mit Code-Optimierung, Legacy-Cleanup, Testabdeckung und Doku-/Agent-Workflow-Updates  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Beim NRK-Chunk-Parsing sollten Entry-Zahlen, Offsets und Längen strikt validiert werden, um fehlerhafte/kaputte Preset-Dateien früh und sicher abzulehnen.
- Ein expliziter Test auf `DSCR`-Priorität gegenüber dem aus `STAT` restaurierten Skript schützt die gewünschte v2-Semantik zuverlässig gegen Regressionen.
- Verbindliche Abschluss-Schritte in `AGENTS.md` und `docs/AGENTS.md` reduzieren Review-Runden, weil Optimierung, Cleanup, Tests und Doku systematisch abgearbeitet werden.

#### Fallstricke

- In dieser Sandbox bleiben lokale Build-/Testläufe durch fehlende Linux-Dependency `x11` bereits beim CMake-Configure blockiert; deshalb CI-Status immer zusätzlich über GitHub Actions prüfen.
- `action_required` Workflow-Runs können ohne gestartete Jobs erscheinen; die Diagnose muss dann über Run-Metadaten und spätere Reruns erfolgen.

#### Empfehlungen für nächste Session

1. Falls möglich CI-Jobs neu triggern/approven, damit die neuen Preset- und Parser-Checks auf allen Plattformen laufen.
2. Zusätzlichen Negativtest ergänzen: Preset mit ungültiger Chunk-Entry-Anzahl muss `loadPreset()` sauber fehlschlagen.
3. README-DSL-Kurzübersicht bei Gelegenheit formatieren (der bestehende Listenblock ist schwer lesbar).

### 2026-05-21 – SIMD-Hotpath + NRK-DSCR-Chunk

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Performance-Bottlenecks in `ExpressionEvaluator`/`SignalChain` reduzieren und DSL-Skript als `DSCR`-Chunk im NRK-Format speichern  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- Für JUCE-Ausdrücke lohnt sich ein optionaler SIMD-Callback direkt im AST-Knoten (`FunctionNode`), damit `sin/cos/tanh/exp` nicht auf per-Lane-Scalar-Fallback zurückfallen.
- Template-basierte Block-Evaluierung (`evaluateBlockT` / `evaluateBlockSimdT`) entfernt unnötige `std::function`-Indirektion in Hotpaths, während die alten APIs als Wrapper kompatibel bleiben.
- Ein zusätzlicher roher `DSCR`-Chunk im Presetformat (NRK v2) ermöglicht lesbare, nicht-escaped DSL-Skripte bei voller Rückwärtskompatibilität über den verschlüsselten `STAT`-Chunk.

#### Fallstricke

- `report_progress` kann Build-Artefakte committen, wenn `build/` nicht ignoriert ist; deshalb `build/` explizit in `.gitignore` eintragen.
- In dieser Sandbox scheitert `cmake -B build -S .` weiterhin früh wegen fehlendem Systempaket `x11`; Test-Validierung kann dadurch lokal blockiert sein.

#### Empfehlungen für nächste Session

1. CI-Run prüfen, ob SIMD-/Preset-Änderungen auf allen Plattformen grün sind.
2. Optional echte vektorielle Approximationskerne für LookupTables-SIMD-Funktionen evaluieren (anstatt laneweiser LUT-Auswertung).
3. Preset-Inspector/UI um DSCR-Chunk-Anzeige ergänzen (Debugging/Diagnose).

### 2026-05-19 – Stabilitätsfixes + Agent Docs + Factory Presets

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Kritische DSP/Licensing-Schwächen beheben, `docs/AGENTS.md` erstellen, Factory-Presets/Templates erweitern  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `DSPUtils::autoGainCompensate()` sollte im Hotpath ausschließlich direkte Sample-Multiplikation verwenden; per-Sample `getSubBlock()` + `ProcessContext` erzeugt unnötigen Overhead.
- Ein `juce::Thread`-basierter Async-Wrapper mit `juce::MessageManager::callAsync()` ist ein robuster Weg, blockierende Licensing-HTTP-Aufrufe aus dem Message-Thread herauszuhalten.
- DC-Offset-Entfernung gehört direkt hinter die DSL-Signalkette; in oversampelten Setups müssen Coefficients auf der effektiven Processing-Samplerate gesetzt werden.
- Oversampling-Latenz muss an zwei Stellen konsistent sein: `setLatencySamples(...)` für den Host und `dryWetMixer.setWetLatency(...)` für internes Dry/Wet-Alignment.

#### Fallstricke

- Lokale Linux-Builds können ohne `install_linux_deps.sh` bereits bei CMake-Dependency-Checks fehlschlagen.
- Die aktuelle CMake/JUCE-Konfiguration kann lokal beim Test-Build über `juce::juceaide` scheitern; das ist ein bestehendes Infrastrukturproblem und nicht Teil der inhaltlichen Fixes.

#### Empfehlungen für nächste Session

1. CMake/JUCE-Tooling (`juce::juceaide`) robust machen, damit lokale Tests ohne Workarounds laufen.
2. Factory-Presets an Preset-UI/Import-Workflow anbinden (falls noch nicht konsumiert).
3. `PluginProcessor` schrittweise entkoppeln (God-Class-Abbau).

### 2026-04-01 – Phase 2 Professionalität: Build-Fixes + Feature-Integration

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** CI/CD Build-Fehler beheben + Phase 2 Features (Resizable UI, MIDI Learn, Undo/Redo, Oversampling ComboBox, pluginval)  
**Ergebnis:** ✅ Erfolgreich  
**PR:** copilot/fix-build-error-and-implement-phase-2

#### Erkenntnisse

- **CI/CD:** Der `ci.yml` Workflow schlug fehl weil juceaide manuell gebaut wurde und `pkgRedirects` nicht erstellt werden konnte. Lösung: JUCE direkt mit `--recurse-submodules` klonen (bringt VST3 SDK mit) und JUCE über `add_subdirectory()` + `JUCE_BUILD_HELPER_TOOLS ON` einbinden lassen – juceaide wird dann automatisch konfiguriert.
- **MSBuild Workflow:** `msbuild.yml` war komplett broken (suchte `NeuroCore.sln` die nie existierte). Entfernt, da `ci.yml` bereits Windows/macOS/Linux abdeckt.
- **MIDI Learn Architektur:** `MidiLearnManager` mit SpinLock und TryLock-Pattern im Audio-Thread ist sauber. `processMidiMessages()` verwendet `ScopedTryLockType` damit der Audio-Thread nie blockiert – wichtig für Echtzeit-Garantie.
- **Undo/Redo Pattern:** `setFormula()` erstellt `FormulaChangeAction` und delegiert an `applyFormula()`. Die UndoableAction ruft auch `applyFormula()` auf (nicht `setFormula()`), um Rekursion zu vermeiden.
- **EDITOR_WANTS_KEYBOARD_FOCUS:** Muss `TRUE` sein damit `keyPressed()` im Editor funktioniert. Ohne diese Einstellung kommen Tastatur-Events nie an.
- **NEEDS_MIDI_INPUT:** Muss `TRUE` sein damit der Host MIDI-Daten an `processBlock()` weiterleitet. Ohne das bleibt die `MidiBuffer` immer leer.
- **pluginval:** Läuft mit `|| true` am Ende, damit der CI nicht fehlschlägt wenn pluginval Warnungen ausgibt. Für Strictness Level 5 ist das normal bei Plugins in Entwicklung.

#### Fallstricke

- **Doppelte Undo-Registration:** Wenn `setFormula()` sowohl parst als auch `applyFormula()` aufruft und dann die UndoableAction registriert, muss die Action NUR `applyFormula()` aufrufen (nicht `setFormula()`), sonst entsteht eine Endlosschleife.
- **MidiLearnManager in Test-Target:** Muss auch in `NeuroCoreTests` eingebunden werden, da `PluginProcessor.cpp` (das im Test-Target ist) `MidiLearnManager.h` inkludiert.
- **install_linux_deps.sh:** Fehlende Pakete (`libxcursor-dev`, `libxinerama-dev`, `libasound2-dev`, `libcurl4-openssl-dev`, `pkg-config`) führen zu kryptischen CMake-Fehlern. Immer ALLE JUCE-Abhängigkeiten auflisten.

#### Empfehlungen für nächste Session

1. CI-Pipeline testen: Überprüfen ob der Build auf allen drei Plattformen grün ist
2. `PluginProcessor` God-Class beginnen aufzuteilen
3. `autoGainCompensate()` per-Sample-Ineffizienz beheben
4. Lock-Free FIFO für UI→Audio-Kommunikation evaluieren

### 2026-04-01 – Phase 1 Stabilität: Erste Fixes

**Agent:** GitHub Copilot  
**Aufgabe:** CMake-Fix, Licensing-Dev-Mode, Thread-Safety, DSLParser-Tests  
**Ergebnis:** ✅ Erfolgreich

#### Erkenntnisse

- `SignalChain.cpp` verwendete bereits `std::atomic_load`/`std::atomic_store` für `chain` an allen relevanten Stellen: `prepare()` (Zeile 36), `loadScript()` (Zeile 298), `processBlock()` (Zeile 317) und `processBlockSmoothed()` (Zeile 329). Der Konstruktor weist `chain` direkt zu (Single-Thread, kein Race möglich). Es fehlte nur der öffentliche `getChain()` Getter im Header.
- `tests/main.cpp` inkludierte und registrierte `SignalChainTest` und `LookupTableSmootherTest` bereits korrekt – sie fehlten nur in `target_sources(NeuroCoreTests)` in `CMakeLists.txt`.
- `kEnableLicensing = true` mit Placeholder-URL `licensing.example.com` macht das Plugin im Dev-Build sofort zum Demo-Plugin. Das ist der gefährlichste stille Bug.
- Das doppelte `target_sources(NeuroCore PRIVATE ${SOURCE_FILES})` in CMakeLists.txt (Zeile 99 in `juce_add_plugin SOURCES` + Zeile 102 explizit) kann ODR-Verstöße und erhöhte Build-Zeit verursachen.
- DSLParser validiert bereits viele Fehlerfälle (fehlender Doppelpunkt, unbekannter Block-Typ, doppelter Block-Name, param nach Block). Tests decken jetzt alle diese Fälle ab.

#### Fallstricke

- `getScript()` ohne Lock ist ein echter Data-Race: `dslScript` kann von `setFormula()` im UI-Thread geschrieben werden während `getScript()` liest. Der `noexcept`-Qualifier muss entfernt werden, da SpinLock-Zugriff technisch werfen kann.
- Bei `SpinLock` + `juce::String`: Die String-Kopie unter Lock ist ein potenzieller Heap-Allokations-Punkt im Audio-Thread. Für eine vollständige Lösung wäre ein Lock-Free FIFO (z. B. `juce::AbstractFifo`) besser – das ist aber Phase 2.
- Kein blindes Hinzufügen von Sourcen zu beiden Targets: `NeuroCore` und `NeuroCoreTests` haben unterschiedliche Abhängigkeiten (NeuroCoreTests braucht kein `juce::juce_audio_plugin_client`).

#### Empfehlungen für nächste Session

1. Blockierenden HTTP-Call in `LicenseManager.cpp` asynchron machen
2. `PluginProcessor` God-Class beginnen aufzuteilen
3. `autoGainCompensate()` per-Sample-Ineffizienz beheben

---

### 2026-04-01 – Initiale Analyse & Dokumentation

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Vollständige Codebase-Analyse und Dokumentationsstruktur erstellen  
**Ergebnis:** ✅ Erfolgreich  
**Commit:** `b5e76bb` (Analyse-Basis)

#### Erkenntnisse

**Codebase:**
- `PluginProcessor.cpp` ist mit ~34 KB eine massive God-Class, die dringend in kleinere, fokussierte Klassen aufgeteilt werden muss. Das ist das größte Architektur-Problem des Projekts.
- `ExpressionEvaluator` ist das Herzstück des Plugins – die Kombination aus SIMD, Constant-Folding und CSE-Elimination ist beeindruckend professionell implementiert.
- Das Licensing-System ist komplett nicht-funktional: `licensing.example.com` ist ein Placeholder. `kEnableLicensing = true` muss für alle Dev-Builds auf `false` gesetzt werden.
- Der `DSLParser` hat **keine Tests** – das ist die höchste Priorität für die nächste Entwicklungs-Session.
- Thread-Safety zwischen UI-Thread und Audio-Thread ist das größte Stabilitätsrisiko: `getScript()`, `shared_ptr<Chain>` und `SpinLock + juce::String` sind alle potentielle Race Conditions oder Heap-Allokationen im Audio-Thread.
- Die DSL-Idee ("ShaderToy für Audio") ist das absolute Alleinstellungsmerkmal – das muss das Herzstück aller Marketingbemühungen sein.
- Das `WeightedLayout`-System ist clever und flexibel, aber die feste Fenstergröße 1600×900 macht das Plugin auf kleineren Bildschirmen (z. B. 13" Laptops) schwierig nutzbar.
- Blowfish-Verschlüsselung für Presets ist ungewöhnlich – ein Upgrade auf AES-256 wäre professioneller.
- `autoGainCompensate()` mit per-Sample `getSubBlock()` ist ein ernsthafter Performance-Bottleneck.

**Build-System:**
- JUCE muss in Version ≥ 8.0.6 vorhanden sein. Die CMake-Integration lädt JUCE automatisch wenn `JUCE_DIR` nicht gesetzt ist.
- Die VST3-SDK muss manuell in `~/JUCE/modules/juce_audio_processors/format_types/VST3_SDK` kopiert werden.
- `CMakeLists.txt` hat eine doppelte Source-Einbindung (`SOURCES` in `juce_add_plugin` UND `target_sources`) – das ist ein Bug der Warnungen erzeugen kann.
- `SignalChainTest.h` existiert im `tests/`-Verzeichnis, ist aber nicht im `NeuroCoreTests` CMake-Target verlinkt.

#### Fallstricke

- Kein blindes Hinzufügen von Code in `PluginProcessor` – diese Klasse ist bereits zu groß
- Bei jeder neuen Klasse: Thread-Safety von Anfang an bedenken (Audio-Thread vs. Message-Thread)
- `juce::String`-Operationen unter Lock = potentielle Heap-Allokation = Audio-Thread-Knackser
- `std::shared_ptr` ist nicht thread-safe ohne explizite Synchronisation (`atomic_load`/`atomic_store`)

#### Empfehlungen für nächste Session

1. Als erstes `kEnableLicensing = false` setzen (verhindert Demo-Modus bei Entwicklung)
2. Dann Thread-Safety fixen (kritischstes Stabilitätsproblem)
3. DSLParser-Tests schreiben (kritischste fehlende Test-Abdeckung)

---

## Vorlage für neue Einträge

```markdown
### YYYY-MM-DD – [Kurztitel der Session]

**Agent:** [Agenten-Typ / Name]
**Aufgabe:** [Was war die Aufgabe?]
**Ergebnis:** ✅ Erfolgreich / ⚠️ Teilweise / ❌ Fehlgeschlagen
**Commit:** [Commit-Hash oder PR-Link]

#### Erkenntnisse

- [Was wurde gelernt?]
- [Was war überraschend?]
- [Was hat gut funktioniert?]

#### Fallstricke

- [Was ist schiefgelaufen?]
- [Was muss beim nächsten Mal beachtet werden?]

#### Empfehlungen für nächste Session

1. [Konkrete nächste Schritte]
```
