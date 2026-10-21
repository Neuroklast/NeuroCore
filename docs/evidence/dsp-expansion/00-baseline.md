# NEUROKORE DSP Expansion — Baseline (PHASE 0.3)

**Repository:** Neuroklast/NeuroCore  
**Produkt:** NEUROKORE  
**Version:** 0.7.1-beta (nicht eigenmächtig geändert)  
**Branch:** `main`  
**Commit:** `b20f7d565` (Merge-PR #318 Circuit/Inspect/Terminal overhaul)  
**Arbeitsbranch für diese Expansion:** noch nicht erstellt (wird pro PR angelegt)  
**Datum:** 2026-10-21  
**Plattform:** Windows 10/11, MSVC (Visual Studio 2022), CMake, Ninja  
**JUCE:** 8.0.6 (bereits im Build-Tree unter `build/JUCE` fetcht)

---

## 1. Git-Zustand

```
Branch: main
Commit: b20f7d565 Merge pull request #318 from Neuroklast/feat/circuit-inspect-terminal-overhaul
Working tree: sauber (keine ungecommitten Änderungen)
```

Uncommitted zum Zeitpunkt der Bestandsaufnahme (vor ersten Codeeingriffen):
- `M Resources/moduleinfo.json`
- `M docs/LICENSING_V2.md`
- `?? neurokore_unit_ring_gunmetal_v2.svg`

Diese Änderungen betreffen das Lizenz-Paket und sind nicht Teil der DSP-Expansion.

---

## 2. Kompilierstatus (Baseline)

**Befehl:**
```
cmake -B build -S .
cmake --build build --config Release --target NeuroKoreTests
```

**Ergebnis:** Build erfolgreich, Ziel `NeuroKoreTests.exe` erzeugt unter  
`build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe`.

**Hinweise:** Nur deprecation-Warnings (`std::atomic_load_explicit` auf `shared_ptr`, C++20, STL4029) — keine Fehler.  
Der Build erfordert `npm run build` zuerst (Web-Editor als Hard-Dependency; `web/dist` ist bereits vorhanden).

**NATIVE BINARY (VST3/Standalone):** Wurde für diesen Baseline-Lauf **nicht** neu gebaut (Native-Delivery ist laut DEVELOPMENT_STATUS bis zur Fertigstellung der DSP-Arbeit zurückgestellt). `build\NeuroKore_artefacts\Release\` enthält Artefakte, die Standalone/VST3 repräsentieren.

---

## 3. Teststatus (Baseline)

**Befehl:**
```
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe
```

**Ergebnis (TOTAL):** `32041 passed, 5 failed` → Exit-Code 1.

Die 5 Fehler sind **vorbestehend auf `main`** (nicht durch die DSP-Expansion verursacht) und werden hier dokumentiert, damit spätere Änderungen sie nicht als eigene Regressionen ausweisen:

| Test-Suite | Falls | Art (Baseline-Problem) |
|---|---|---|
| `ArchitectureHardening` | 4 | Vorbestehende Erwartungen (CEILING/routing etc.) |
| `FactoryLoudnessTest` | 1 | `quiet inserts` — Pegel-Erwartung für Factory-Presets |

Bemerkung im Testlauf: `!!! Test 3 failed: quiet inserts: Ambient Swell Guitar ... -- Expected value: 0, Actual value: 3` (gehört zu FactoryLoudnessTest).

Alle anderen 32041 Tests grün, darunter:
- `FxRuntimeTest`, `DSLParserTest`, `GraphModel`, `ExpressionEvaluatorTest`, `SignalChainTest`, `WaveShaperTest`, `PresetManagerTest`, `DSPUtilsTest`, `DelayReverbMs`, `CrackleFixes`, `DynamicsBlocksTest`, `ModulationBlocksTest`, `IrXoverTest`, `AstJson`, `WebShell`, `WebCompile`, `HostSnapshot`, `SanitationChainTest`, `ModulationBlocksTest`.

**Web (Vitest):** Nicht als Teil dieses Baseline-Laufs ausgeführt (Playwright-E2E wird laut DEVELOPMENT_STATUS am 20-Minuten-Job-Cap abgebrochen; `npm run test:layout` ist lokal verfügbar). Die Vitest-Layout-Kontrakte (`web/src/assemble/*.test.ts`) sind Teil des TypeScript-Builds.

---

## 4. DSP-CPU / Speicher / Latenz (Baseline)

Keine nativ instrumentierten DAW-Messungen verfügbar (kein konfigurierter nativen/JUCE-Build für DAW-Validierung; pluginval nicht ausgeführt).

Was messbar ist (aus dem Testlauf und Code):
- **Sample-Raten:** 44.1 / 48 / 96 / 192 kHz werden in `prepare` unterstützt (alle Blocks lesen `spec.sampleRate`).
- **Oversampling:** Global bestehend (Sanitation-Kette: 1-Pol-DC 5 Hz → AA → Downsample → optional Soft Clip → True-Peak −0.3 dBTP). Nicht pro Block dupliziert.
- **Latenz:** Wird über `prepareBusLatency` / `getIrLatencySamples` / Pitch-/Limit-/Comp-Latenz an den Host gemeldet (`SignalChain::getIrLatencySamples`, `DspEngine`).
- **Tail:** `getMaxTailTime()` über Comp/Env; `Ott::tailSeconds()`, `Delay::tailSeconds()`, `Reverb::tailSeconds()` existieren.

**Markierung:** `NOT RUN` für DAW-CPU-Messungen, pluginval, Standalone/AU-Validierung, Preset-/DAW-Projekt-Restore (keine native Build-Umgebung für Host-Tests konfiguriert; werde nicht erfunden).

---

## 5. Bestehende Latenz / Presetwechsel / Zustandswiederherstellung

- `clearRuntimeState()` existiert auf jedem Block (Zeroing von Delay-/Reverb-Ringen, y_prev/x_prev, Filter-Historie, ADAA). Wird auf Preset-Switch aufgerufen.
- `prepare` / `prepareToPlay` wird bei Sample-Rate- und Buffer-Änderungen erneut durchlaufen; Puffer/FFT/Filter werden in `prepare` vorallociert (keine Allokation im Callback).
- `loadScript` baut eine neue `Chain` und publiziert sie atomar (`std::atomic_store`), sodass der Audiothread nie mitten in einer Neukonfiguration liest. Alte Chain wird als `retiredChain` gehalten und erst freigegeben, wenn der Audiothread sie nicht mehr referenziert.
- Host-State (`setStateInformation` / `getStateInformation`) bewahrt Skript, Makros, Mix, IRs. DEVELOPMENT_STATUS: „Host state restore does not overwrite global oversampling or polisher“.

---

## 6. Abnahme PHASE 0.3

- [x] Git-Commit und Branch dokumentiert (`main`, `b20f7d565`)
- [x] Kompilierstatus dokumentiert (Build erfolgreich, nur Deprecation-Warnings)
- [x] Teststatus dokumentiert (32041 passed / 5 failed, alle vorbestehend)
- [x] DSP-CPU / Speicher / Latenz: wo messbar dokumentiert, sonst `NOT RUN`
- [x] Presetwechsel / Zustandswiederherstellung dokumentiert
- [x] Baseline unter `docs/evidence/dsp-expansion/` abgelegt

Gegen diesen Istzustand wird jede spätere Änderung überprüft.
