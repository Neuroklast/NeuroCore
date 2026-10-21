# NEUROKORE DSP Expansion — PHASE 2: Gemeinsame DSP-Infrastruktur (Audit)

**Version:** 0.7.1-beta  
**Branch:** `codex/neurokore-licensing` @ `b20f7d565`  
**Datum:** 2026-10-21

## Ergebnis

Die für Phase 2.1–2.4 geforderte gemeinsame DSP-Infrastruktur existiert bereits und wird wiederverwendet. Es wird **keine** zweite Implementierung geschrieben (keine speculative Abstraktion, keine Duplizierung bestehender Engine-Funktionen). Neue Effekte bauen auf den vorhandenen Helfern auf.

## 2.1 Realtime-sichere Parameterverarbeitung

| Bedarf | Bestehende Umsetzung | Datei |
|---|---|---|
| Parameteränderungen ohne hörbare Sprünge | `juce::SmoothedValue<float>` (reset auf sampleRate + kSmoothingTime) | `src/core/Config.h` (kSmoothingTime, kModSmoothingTime, kXoverSmoothingTime, kLfoSmoothingTime), genutzt durch alle Blocks |
| Lineare und logarithmische Werte | `DSPUtils::dbToLinear` / `linearToDb` | `src/dsp/DSPUtils.h` |
| Frequenzen | Bereichsbegrenzung (z. B. Filter-Cutoff ∈ [20, Nyquist), Resonanz ≤ kQMaxModulated 2.2) | `src/dsl/SignalChain.cpp` |
| Pegel in dB | `DSPUtils::dbToLinear` / `linearToDb`, `rms`, `measureDynamicRange` | `src/dsp/DSPUtils.h` |
| Zeitparameter | Sekunden-Bereiche in den Block-Configs (z. B. Gate attack/release) | `src/dsl/blocks/*.cpp` |
| Diskrete Betriebsmodi | Enums (nicht numerisch interpoliert) — z. B. Eq type, Osc shape, Meter mode | `src/dsl/SignalChain.cpp` + `web/src/assemble/blockCatalog.ts` |
| Morphing-Parameter | Lineare Interpolation über `SmoothedValue` zwischen Zielwerten | bestehende Block-Implementierungen |
| Modulationssignale | Vorgerenderte Mod-Lanes (Osc `modLane`, Env `modLane`), einmal pro Block | `src/dsl/SignalChain.h` (Osc/Env) |
| Tempo-Synchronisation | `setTempo` → `applyTempo` (Osc, Delay, Pitch); Note-Knobs veröffentlichen Millisekunden | `src/dsl/SignalChain.cpp` |

Diskrete Modi werden nicht numerisch interpoliert (Enum-Whitelist im Parser + chipSpec); kontinuierliche Parameter erhalten `SmoothedValue`-Glättung.

## 2.2 Realtime-Sicherheit (Audio-Thread)

| Verbot | Bestehende Sicherung | Datei |
|---|---|---|
| Heap-Allokationen | Puffer/Filter/FFT-Pläne/LUTs werden in `prepare` vorallociert; `processBlock` arbeitet auf Callback-`n`-Slices, kein `setSize`/`new` | `src/dsl/SignalChain.cpp` (prepare/processBlock) |
| Dateioperationen | Keine im Callback; IR-Laden außerhalb (Host-State / `loadImpulseResponse`) | `src/dsl/SignalChain.cpp` |
| Mutex-Wartezeiten | `SpinLock` nur für `loadScript` vs. Prozessblock (try-lock im Callback; bei Sperre letzte Werte beibehalten) | `src/dsl/SignalChain.cpp` |
| UI-Aufrufe | Keine `juce::String` im Callback; Variablen über gecachte `float*`-Zeiger (HotSlots) | `src/dsl/SignalChain.h` (HotSlots) |
| String-Verarbeitung | Aus dem Audio-Thread entfernt (Formeln → `ExprTape` / vorbereitete Ausdrücke) | `src/dsl/SignalChain.cpp` |
| Neue Container mit dynamischer Allokation | Keine im Callback; Rings sind vorbereitet | `src/dsp/DSPUtils.h` (alignedRing) |
| Unbegrenzte Berechnungen | Block-basiert, feste Höchstwerte (z. B. kMaxChannels=2, kMaxBands=32) | `src/core/Config.h` |
| IR-/FFT-Laden im Callback | Vorbereitet in `prepare`; FFT (Pitch) Größen fest | `src/dsl/blocks/PitchBlock.cpp` |
| Alte-Zustand-Freigabe im Callback | Alte `Chain`/`BusGraph` als `retired*` gehalten, erst freigegeben wenn Audio-Thread nicht mehr referenziert (atomic shared_ptr) | `src/dsl/SignalChain.cpp` |

DSP-Neukonfiguration erfolgt über kontrollierten Austausch vorbereiteter Zustände (`loadScript` baut neue `Chain`, publiziert atomar). Destruktoren großer Strukturen laufen nicht im Audio-Thread.

## 2.3 Oversampling

**Position:** Ein einziges, globales Oversampling existiert in der Engine-Sanitationskette (`SanitationChain`: DC-Blocker 5 Hz → AntiAliasLowpass (fc = 0.45 · hostSr) → Verarbeitung bei OS-Rate → True-Peak-Limiter → OutputSanitizer → Dither). Die OS-Rate wird über `osSpec` an `SanitationChain::processOversampled` übergeben.

**Priorität (wie gefordert):**
1. Vorhandenes globales Oversampling wird wiederverwendet — neue Verzerrungsblöcke aktivieren **kein** zweites, unabhängiges Oversampling.
2. Nichtlineare Algorithmen laufen auf der tatsächlich aktiven Processing-Sample-Rate.
3. Aliasing wird durch Messungen (Phase 12.2) geprüft, nicht durch Behauptung.
4. Lokales Oversampling wird nur eingeführt, wenn technisch notwendig und latenzkompensiert (bisher nicht der Fall).

Es wird vermieden, mehrere Up-/Downsampling-Stufen innerhalb derselben Signalkette aufzuteilen.

## 2.4 Gemeinsame DSP-Hilfsfunktionen (keine Duplizierung)

| Funktion | Bestehende Umsetzung | Datei |
|---|---|---|
| dB- und Linear-Konvertierung | `dbToLinear`, `linearToDb` | `src/dsp/DSPUtils.h` |
| DC-Blocking | `DcBlocker1p` (5 Hz, prepare/reset/process) | `src/dsp/DcBlocker1p.h` |
| Gleichspannungsfreie Transferkurven | `satFb`, `sanitationSoftClip`, `softCeilSample` | `src/dsp/DSPUtils.h` |
| Sample-Rate-abhängige Koeffizienten | `onePoleAllpassA` (Bilinear), Filter-Koeffs in `prepare` | `src/dsp/DSPUtils.h` |
| Bandbegrenzte Oszillatoren | `juce::dsp::Oscillator` mit soft edges (Sine/Triangle/Square/Saw/Noise) | `src/dsl/SignalChain.cpp` (makeOsc) |
| Phasenakkumulatoren | `Osc.phase` / `Phaser.phase` (intern) | `src/dsl/SignalChain.h` |
| Ringbuffer | `DSPUtils::alignedRing`, `alignedTable`, `delayRead` (Catmull-Rom) | `src/dsp/DSPUtils.h` |
| Interpolation für Delay-Lines | `delayRead` (4-Punkt Catmull-Rom, Integer = ein Sample) | `src/dsp/DSPUtils.h` |
| Envelope-Detektoren | `smoothMeterDb`, `rms`, `measureDynamicRange`, `detectDCOffset` | `src/dsp/DSPUtils.h` |
| Parameter-Smoothing | `juce::SmoothedValue` (per Block, kSmoothingTime/kModSmoothingTime) | `src/core/Config.h` |
| Dry/Wet-Mixing | `mixDryWetContinuous`, `mixDryWetPhaseFree` | `src/dsp/DSPUtils.h` |
| Latenzausgleich | `LatencyAlignedSidechain`, `prepareBusLatency`, `getIrLatencySamples` | `src/dsp/LatencyAlignedSidechain.h`, `src/dsl/SignalChain.cpp` |
| Numerische Schutzfunktionen | `flushDenorm`, NaN/Inf-Prüfung, `OutputSanitizer` (Residual-Mute, kein Clamping musikalischer Pegel) | `src/dsp/DSPUtils.h`, `src/dsp/OutputSanitizer.h` |

Keine zweite Implementierung wird geschrieben, solange eine geeignete vorhanden ist.

## 2.5 DSP-Verträge

Jeder (neue) Effekt unterstützt:
- Mono und Stereo (`useCh = jmin(nCh, 2)` in allen Blocks)
- Mindestens 44,1 / 48 / 96 / 192 kHz (alle Blocks lesen `spec.sampleRate`; Filter/EQ begrenzen auf Nyquist)
- Unterschiedliche Host-Buffergrößen (Buffer wird auf `maximumBlockSize` vorbereitet, `processBlock` schneidet auf `nS`)
- Buffergröße 1 (Block-Code schützt `nS <= 0`)
- Stille (stabile Verarbeitung, keine NaN/Inf; `OutputSanitizer` als Sicherheitsnetz)
- Sehr kleine Eingangssignale (kein willkürliches Clamping bei ±1, außer absichtlichem Clipping)
- Hohe interne Floating-Point-Pegel (Sanitation-Kette, True-Peak −0.3 dBTP)
- Parameteränderungen während der Wiedergabe (SmoothedValue + prepareBlockEval)
- Reset nach Transport-/Presetwechsel (`clearRuntimeState` auf jedem Block)
- Wiederholtes Prepare und Release (prepare ist idempotent, Filter resetten)
- Korrekte Latenzmeldung (`prepareBusLatency`, `getIrLatencySamples`, `tailSeconds` auf Effekt-Blöcken)
- Korrekte Tail-Semantik (Tail über längsten hörbaren Bus-Pfad; geparkte Busse nicht)

Alle Ausgaben sind bei gültigen Eingaben endlich; interne Pegel werden nicht willkürlich bei ±1 abgeschnitten, sofern der Effekt kein absichtliches Clipping vorsieht.

## Tests

Bestehende Tests decken die Infrastruktur ab (alle grün im Baseline-Lauf, 0.7.1-beta):
- `DSPUtilsTest`, `SanitationChainTest`, `SanitationChainTest`, `ArchitectureHardeningTest`, `SignalChainTest`, `WaveShaperTest`, `DelayReverbTest`, `ModulationBlocksTest`, `FxRuntimeTest`.

**Keine neue Implementierung erforderlich.** Neue Effekte (ab Phase 3) verwenden diese Helfer und erbringen ihre eigenen Tests (Phase 12).

## Abnahme PHASE 2

- [x] Gemeinsame Helfer identifiziert (keine Duplizierung)
- [x] Globales Oversampling als einziger OS-Punkt bestätigt; kein per-Block-OS
- [x] Realtime-Sicherheitsverträge dokumentiert (keine Allokation/Mutex/String/UI im Callback)
- [x] DSP-Verträge (2.5) dokumentiert als Basis für alle neuen Effekte
