# Fortschritt zu `currentplan.md`

Bezugsplan: [`currentplan.md`](currentplan.md) (NEUROKORE Master Roadmap, Stand 2026-10-22).
Dieses Dokument ist der laufende Arbeits- und Nachweisstand. Es ersetzt den Plan nicht.

- **Produktversion:** 0.7.1-beta (unverändert)
- **Basis:** `main` @ `ea293eb972e5` (Merge PR #324)
- **Arbeitsdatum:** 2026-10-22
- **Plattform:** Windows 11, MSVC 2022, CMake 4.1.0-rc1, JUCE 8.0.6, Node 25.9.0

---

## Statusübersicht

| Paket | Status | Kurzstand |
|---|---|---|
| A01 Baseline und Git-Prüfung | `READY` | Baseline reproduziert, 5 native + 8 Web-Alttests dokumentiert |
| A02 Native DSP-Testpipeline | `VALIDATION_PENDING` | Parser-, Katalog- und Docs-Sync-Abdeckung lokal grün; GitHub-CI offen |
| A03 Modulationsarchitektur | `READY` | Gemeinsamer Control-Source-Vertrag, Zyklenprüfung, Depth/Polarity, Reset; Tests grün |
| C01 Step Sequencer | `VALIDATION_PENDING` | 1–64 Steps, Sync, Swing, Probability, Glide, Editor, Live-Step; DAW/pluginval offen |
| C02 ADSR | `VALIDATION_PENDING` | Lineare Phasen, Audio-/MIDI-Gate, Retrigger, Velocity, Kurve; DAW/pluginval offen |
| C03 Sample & Hold | `VALIDATION_PENDING` | Rate/Sync, Hold, Slew, Quantize, Reset, Mono-Kontrolle; DAW/pluginval offen |
| C04 Random Generator | `VALIDATION_PENDING` | Seed, Sync, Range/Swap, Shape, Smooth, Bipolar; DAW/pluginval offen |
| C05 Envelope Trigger | `VALIDATION_PENDING` | Hysterese, Holdoff, Peak/RMS, Puls, Live-Trigger; DAW/pluginval offen |
| B01–B11, C06–C08, D01–D05, E01–E02 | `OPEN` | Noch nicht begonnen |
| F01–F24 | `OPEN` | Noch nicht begonnen |
| G–I | `OPEN` | Noch nicht begonnen |

---

## A01 – Baseline und Git-Prüfung

**Status: `READY`**

### Git

- `main` = `ea293eb972e5e3c05b50ba8b3f821f7d45fda39d`
- PR #324 ist in `main` enthalten (`ea293eb` ist der Merge-Commit).
- Keine offenen Pull Requests (`gh pr list`: leer).
- Lokale Branches: `main`, `fix/dsp-expansion-completion`.
- Remote-Branches: `main`, `fix/dsp-expansion-completion`, `feat/circuit-signal-glow`,
  `feat/dsp-expansion-03-clipper`, `codex/neurokore-licensing`,
  `feat/dsp-expansion-02-katalog`, `feat/dsp-expansion-01-baseline`,
  `feat/circuit-inspect-terminal-overhaul`.
- Uncommittete Änderungen des vorherigen Branches wurden vor dem Wechsel nach `main`
  in `stash@{0}` gesichert (`wip before currentplan baseline (fix/dsp-expansion-completion)`).
  Darin enthalten: eine Testdatei, die 268 Zeilen DSP-Tests entfernt. Nicht übernommen.

### Build

| Prüfung | Befehl | Ergebnis |
|---|---|---|
| CMake-Konfiguration | `cmake -B build -S .` | OK, nur Deprecation-Warnungen |
| Native Tests bauen | `cmake --build build --target NeuroKoreTests --config Release` | OK |
| Web bauen/typprüfen | `npm run build` (in `web/`) | OK, nur Chunk-Size-Warnungen |

Native Artefakte: `build/NeuroKoreTests_artefacts/Release/NeuroKoreTests.exe`.
VST3/Standalone wurden in diesem Schritt nicht neu gebaut (`NOT RUN`).

### Native Tests

Befehl:

```powershell
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe
```

Ergebnis: **32910 passed, 5 failed** (Exit 1).

Vorbestehende Fehler, nicht durch diese Arbeit verursacht:

| Suite | Fehler | Details |
|---|---|---|
| `ArchitectureHardening` | 4 | IN-/XOver-Tap-Scratch-Tail-Erwartungen (`pk=0.8901`/`0.7790`) |
| `FactoryLoudnessTest` | 1 | `quiet inserts`: Ambient Swell Guitar, Telephone Then Room, Dark Atmosphere |

Vergleich mit `docs/evidence/dsp-expansion/00-baseline.md` (Commit `b20f7d565`):
damals 32041 passed / 5 failed mit denselben zwei Suiten. Die höhere Testzahl stammt
aus den mit PR #324 ergänzten Verträgen; die Fehlerbilder sind unverändert.
Rohprotokoll: `build/a01-native-test.log`.
Ein späterer Lauf zeigte zusätzlich einen `DemoClock`-Fehler; die Suite lief isoliert
und in Wiederholungsläufen grün. Das wird als Flake geführt, nicht als Regression.

### Web-Tests

Befehl: `npm test` (in `web/`)

Ergebnis: **987 passed, 8 failed** (172 Dateien, 9,4 s).

Vorbestehende Fehler (alle auf `main`, nicht durch diese Arbeit verursacht):

1. `src/assemble/boardView.regression.test.ts` – BoardChip abonniert das Knobs-Array
2. `src/assemble/chipSpec.test.ts` – Katalogzeile für jeden Chip-Typ
3. `src/assemble/visualEdges.test.ts` – OTT-Detektor teilt sich die ID mit Audio-IN
4. `src/store/hostStore.test.ts` – Shared-Prefs-Default aus nativen UiSettings
5. `src/theme/chromeSpec.test.ts` – Chrome-Ecken aus einem Token statt Rohpolygon
6. `src/theme/circuitCssInventory.test.ts` – Live-Chip-Chrome auf Theme-Variablen
7. `src/theme/crtFx.test.ts` – OS-Bloom als schwacher Wash
8. `src/theme/signalLab.test.ts` – Typecode auf IC-Headband-Größe

Rohprotokoll: `build/a01-web-test.log`.
`npm run build` (inkl. `tsc --noEmit` und Vite) ist grün; `web/dist` wurde neu erzeugt.
Playwright-E2E: `NOT RUN` (bekanntes 20-Minuten-Joblimit, siehe `docs/DEVELOPMENT_STATUS.md`).

### Registry und Presets

- Zentraler Block-Katalog: `web/src/assemble/blockCatalog.ts`, **55 Blöcke**, 204 Parameter.
- Add-Menü leitet sich aus `ADDABLE_ENTRIES` ab (`in`/`out` ausgenommen), keine zweite Liste.
- Factory-Katalog: `web/src/presets/factoryCatalog.gen.json`, **340 Presets** in 24 Kategorien.
- Es wurden keine Presets gelöscht oder verändert.

### CI

- Letzte Läufe auf `fix/dsp-expansion-completion`: „DSP expansion contracts“ und
  „Web layout“ erfolgreich (2026-10-22).
- Für `main` liegen keine aktuellen Läufe vor; `NOT RUN`.
- `native-alpha` ist `workflow_dispatch` und wurde nicht ausgeführt.

### Dokumentation

`node scripts/sync_living_docs.mjs` meldet beide Ziele als aktuell:
`resources/UserManual_en.txt`, `web/src/overlays/userManual.gen.txt`.

### Abnahme A01

- [x] `main`-Commit und PR #324 verifiziert
- [x] Offene PRs und Branches erfasst
- [x] Native- und Web-Build geprüft
- [x] Native- und Web-Tests ausgeführt und Alttests dokumentiert
- [x] Registry, Presets und Docs-Sync geprüft
- [x] Reproduzierbare Baseline mit Git-SHA und Rohprotokollen

---

## A02 – DSP-Testpipeline

**Status: `VALIDATION_PENDING`** (lokal grün; GitHub-CI-Lauf offen)

Umgesetzt in `.github/workflows/dsp-expansion.yml`:

- `DSLParserTest` läuft jetzt im nativen Contract-Binary (`tests/fx_main.cpp`).
- Katalog-Verträge laufen nach DSP-/DSL-Änderungen:
  `blockCatalog.test.ts` + `saturationContract.test.ts` (9 Tests).
- Living-Docs-Abgleich: `node scripts/sync_living_docs.mjs` und
  `git diff --exit-code -- resources/UserManual_en.txt`.
- Neue Trigger-Pfade: `tests/DSLParserTest.h`, `web/src/assemble/blockCatalog.ts`,
  `docs/manual/NEUROKORE.md`, `resources/UserManual_en.txt`.
- Push-Trigger zusätzlich auf `main`, damit der Baseline-Stand geschützt bleibt.

Lokale Nachweise:

| Prüfung | Ergebnis |
|---|---|
| Contract-Binary bauen (`cmake -S tests/dsp -B build-contracts -DJUCE_DIR=build/_deps/juce-src`) | OK |
| `NeuroKoreDspContracts.exe` | `Failures: 0`, Exit 0 |
| `NeuroKoreDspContracts.exe --expansion` | Exit 0 |
| Katalog-Verträge (`npx vitest run …`) | 9/9 pass |
| Living-Docs-Abgleich | beide Ziele aktuell, kein Diff |
| Absichtlich defekter Vertrag (`dry[0] != 5.f`) | 20 Fehler, Exit 1, danach zurückgesetzt |

Rohprotokolle: `build/a02-contracts-build.log`, `build/a02-contracts-test.log`.
Die absichtliche Verfälschung wurde vollständig zurückgenommen; `git status` ist für
`tests/dsp/SaturationModelsContract.cpp` sauber.

Offen bis `READY`: der Workflow muss einmal auf GitHub mit diesen Änderungen laufen.
Der lokale Bruchtest beweist die Fehlererkennung, ersetzt aber keinen CI-Lauf.

Nächster Schritt nach CI-Bestätigung: A03 (gemeinsame Modulationsarchitektur).

---

## A03 – Gemeinsame Modulationsarchitektur

**Status: `READY`** (nativer Vertrag)

Umgesetzt:

- Neuer gemeinsamer `Modulator`-Basistyp in `src/dsl/SignalChain.h` mit
  `name`, `modLane`, `destSlot`, `inputNames`, `inputSlots`, `controlDepth`,
  `controlInvert`, `publishLast()`, `clearControlLane()` und `shapedControl()`.
- `osc` und `env` erben jetzt von diesem Vertrag; ihre vorhandene
  `modLane`/`variables[name]`-Semantik bleibt unverändert.
- `stepseq`, `adsr`, `samplehold`, `random`, `modmath`, `envtrigger` und `modmix`
  schreiben keine Audiodaten mehr, sondern füllen eine Kontroll-Lane und
  veröffentlichen sie in `variables[name]`.
- Reine Generatoren (`stepseq`, `random`, `modmath`, `modmix`, `samplehold` mit
  Kontrolleingang) laufen einmal pro Block; Detektoren (`adsr`, `envtrigger`,
  `samplehold` ohne Kontrolleingang) laufen an ihrer Graph-Position.
- Ziele werden über die bestehende Ausdrucksbindung erreicht. `Stage`, `Filter`
  und `Eq` erkennen jetzt auch neue Modulatornamen und injizieren sie
  sample- bzw. blockgenau.
- `modmath`/`modmix` lesen ihre Quellen über `inA`/`inB` statt über Audio-L/R.
- Zyklenerkennung und Validierung unbekannter Quellen beim `loadScript`.
- `clearRuntimeState()` setzt alle veröffentlichten Kontrollwerte auf 0.
- `GraphModel` behandelt die neuen Typen als Modulatoren (nicht als serielle
  Audio-Durchgangsglieder).
- Fehler behoben: `levelA`/`levelB` im ModMix-Parser wurden wegen der
  DSL-Kleinschreibung nie gelesen; Lookup ist jetzt `levela`/`levelb`.

Neue Tests in `tests/ModulationBlocksTest.h`:

- Random steuert einen Stage-Parameter ohne Audiokanal.
- Modulator verändert das Audio nicht.
- ModMath kombiniert zwei benannte Quellen.
- Zyklischer Modulationsgraph wird beim Laden abgelehnt.
- Kontrollquelle moduliert ein Filter-Cutoff.
- Depth/Polarity skalieren den veröffentlichten Wert.
- Reset löscht den veröffentlichten Kontrollwert.

Ergebnisse:

| Prüfung | Ergebnis |
|---|---|
| `NeuroKoreTests.exe ModulationBlocksTest` | 377 passed, 0 failed |
| Gesamte native Suite | 33179 passed, 5 failed (dieselben 5 Alttests wie A01) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| `NeuroKoreDspContracts.exe --expansion` | Exit 0 |

Rohprotokolle: `build/a03-native-test4.log`, `build/a03-contracts-test3.log`.

Noch nicht Teil von A03 (bewusst den Modulen/Phase G zugeordnet):

- Port-Deklarationen in `blockCatalog.ts`/`chipSpec.ts` und die zugehörigen
  Circuit-Editoren für `inA`/`inB`, Step-Werte und Hüllkurven.
- Host-Sync für StepSeq/Random (C01/C04).
- Explizite Depth-/Polarity-Regler im Inspect-Panel (C01–C07/G).

Nächstes Arbeitspaket: **C01 – Step Sequencer**. Die vorhandene 4-Step-Lane ist
jetzt korrekt als Kontrollquelle verdrahtet; C01 ersetzt sie durch 1–64 Steps,
BPM-Sync, Swing, Probability und den Step-Editor.

---

## C01 – Step Sequencer

**Status: `VALIDATION_PENDING`** (Implementierung und lokale Abnahme fertig;
DAW/pluginval noch `NOT RUN`)

Umgesetzt in `src/dsl/blocks/StepSeqBlock.cpp`, `src/dsl/SignalChain.*`:

- 1–64 Steps über `steps`; Werte über `values = 0.5, -1, ...`; Legacy `step0..step3`
  bleibt gültig. Stummschaltung über `mutes = 0,1,...`.
- Frei laufend (`rate` 0–100 Hz) oder tempo-synchron (`sync = 1/16`, `1/8`, `1/4`, …).
- Host-BPM/-PPQ über `SignalChain::setTempo`; Start/Seek setzt den Sequencer zurück,
  Stop hält die Ausgabe bei 0.
- Swing auf geraden/ungeraden Steps, Gate-Länge, Glide, Probability mit Seed.
- Richtungen `forward`, `reverse`, `pingpong`, `random`.
- Ausgabe geht als Kontroll-Lane an beliebig viele Ziele, ohne Audiokanal.
- Parameter im zentralen Katalog und in `chipSpec` ergänzt.
- Keine Audio-Thread-Allokation; Lane wird in `prepare` reserviert.

UI:

- `web/src/overlays/StepSeqEditor.tsx`: 1–64 Balken, Ziehen setzt Werte,
  Mute-Schalter, Werte werden erst bei `pointerup` als `values`/`mutes` committed.
- Live-Position: `appendClipPeaks` liefert `step`/`steps` pro StepSeq-Tap;
  `hostStore.clipsStep` verteilt sie an Chip und Editor. `LfoFace` zeichnet die
  Schrittfolge mit aktivem Step, der Editor zeigt `Step n / count` bzw. `Stopped`.
- `InspectKnob` nutzte den von React reservierten `key`-Prop und crashte beim
  Öffnen des Inspectors; auf `field.key` umgestellt.

Tests:

| Prüfung | Ergebnis |
|---|---|
| `NeuroKoreTests.exe ModulationBlocksTest` | 411 passed, 0 failed |
| Gesamte native Suite | 33189 passed, 5 failed (dieselben Alttests) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| Web-Build (`tsc` + Vite) | OK |
| Katalog-/Sättigungs-/StepSeqModel-Verträge | 37 passed; 1 alter `cables`-Default-Fehler |
| Playwright StepSeq-Editor (1×/2×) | 2 passed |

Rohprotokolle: `build/c01-final3-native.log`, `build/c01-final3-contracts.log`.

Noch `NOT RUN` / offen:

1. Host-/DAW-Abnahme (VST3/Standalone, Transport-Sync im echten Host).
2. pluginval und Preset-Restore im Host.
3. Drei C01-Test-Patches als Factory-Presets.
4. CPU-Stresstest bei 64 Steps und vielen Zielen.

Nächstes Arbeitspaket: **C02 – ADSR** (Phasenwechsel, Retrigger, MIDI-/Audio-Gate,
Velocity, Gate-Länge, Inspector-Kurve).

---

## C02 – ADSR

**Status: `VALIDATION_PENDING`** (Implementierung und lokale Abnahme fertig;
DAW/pluginval noch `NOT RUN`)

Umgesetzt in `src/dsl/blocks/AdsrBlock.cpp` und `src/dsl/SignalChain.*`:

- Attack, Decay und Release laufen als lineare, samplegenaue Rampen mit exaktem
  Phasenwechsel; die alte asymptotische Annäherung konnte `env >= 1` bzw.
  `env <= sustain` nie sicher erreichen.
- Sustain hält den eingestellten Pegel; Release startet vom aktuellen Pegel.
- Gate-Quelle `audio`, `midi` oder `either`; Audio-Gate bleibt der Standard.
- Retrigger `on` startet die Attack-Phase bei jeder steigenden Gate-Flanke neu,
  auch während Attack/Release; `off` ignoriert Retrigger außer aus `Idle`.
- Velocity-Regler 0–1 skaliert die Lane mit der MIDI-Velocity (ohne MIDI-Note
  bleibt der Faktor 1).
- Kontinuierliche Modulations-Lane wie bei den übrigen Modulatoren.
- Parameter in `blockCatalog.ts`/`chipSpec.ts` ergänzt.
- ADSR-Kurve im Inspector und im Chip-Face (`adsrEnvelope.ts`).

Tests in `tests/ModulationBlocksTest.h`:

- Exakte Attack-/Decay-/Sustain-Zeitpunkte, Release, Retrigger während Release,
  Gate-Wechsel innerhalb eines Buffers, MIDI-Gate + Velocity und
  Buffergrößen-Unabhängigkeit.

Ergebnisse:

| Prüfung | Ergebnis |
|---|---|
| Gesamte native Suite | 33206 passed, 5 failed (dieselben Alttests) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| Web-Build (`tsc` + Vite) | OK |
| ADSR-/StepSeq-/Katalog-Verträge | 39 passed; 1 alter `cables`-Default-Fehler |

Rohprotokolle: `build/c02-final-native.log`, `build/c02-final-contracts.log`.

Noch `NOT RUN` / offen:

1. Host-/DAW-Abnahme (VST3/Standalone, MIDI-Gate im echten Host).
2. pluginval und Preset-Restore.
3. Drei C02-Test-Patches als Factory-Presets.
4. CPU-Messung bei sehr kurzen Zeiten und hoher Modulationsrate.

Nächstes Arbeitspaket: **C03 – Sample & Hold** (echte Kontrolleingänge, BPM-Sync,
Slew, Quantisierung, Rate 0, Stereo/Mono).

---

## C03 – Sample & Hold

**Status: `VALIDATION_PENDING`** (Implementierung und lokale Abnahme fertig;
DAW/pluginval noch `NOT RUN`)

Umgesetzt in `src/dsl/blocks/SampleHoldBlock.cpp` und `src/dsl/SignalChain.*`:

- Mono-Kontrolleingang über `inA`/`inB`; ohne Kontrollquelle wird Audiokanal 0
  getastet. Es gibt bewusst keinen separaten Stereo-Zustand.
- `rate` (0–1000 Hz) oder `sync = 1/16 … 1/1` über Host-BPM.
- `hold` verlängert die Haltezeit nach jedem Sample in Sekunden.
- `slew` 0–1 glättet den gehaltenen Wert (0 = sofort, 1 = ca. 0,5 s).
- `quantize` 0–1 reduziert auf 64 … 2 Stufen; das Ergebnis bleibt auf [-1, 1]
  begrenzt.
- Rate 0 friert den zuletzt gehaltenen Wert ein; `clearRuntimeState` setzt ihn
  auf 0 zurück.
- Parameter in `blockCatalog.ts`/`chipSpec.ts` ergänzt.

Tests in `tests/ModulationBlocksTest.h`:

- Halteintervall und nächster Sample-Zeitpunkt, Rate 0 + Reset, Host-Sync,
  Quantize/Slew, Buffergrößen-Unabhängigkeit.

Ergebnisse:

| Prüfung | Ergebnis |
|---|---|
| `NeuroKoreTests.exe ModulationBlocksTest` | 446 passed, 0 failed |
| Gesamte native Suite | 33224 passed, 5 failed (dieselben Alttests) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| Web-Build (`tsc` + Vite) | OK |
| ADSR-/StepSeq-/Katalog-Verträge | 39 passed; 1 alter `cables`-Default-Fehler |

Rohprotokolle: `build/c03-final-native.log`, `build/c03-final-contracts.log`.

Noch `NOT RUN` / offen:

1. Host-/DAW-Abnahme (VST3/Standalone, Host-Sync).
2. pluginval und Preset-Restore.
3. Drei C03-Test-Patches als Factory-Presets.
4. CPU-Messung bei hoher Rate und Quantize.

Nächstes Arbeitspaket: **C04 – Random Generator** (Seed, Rate/Sync, Range,
Shape, Smooth, Bipolar, mehrere Instanzen).

---

## C04 – Random Generator

**Status: `VALIDATION_PENDING`** (Implementierung und lokale Abnahme fertig;
DAW/pluginval noch `NOT RUN`)

Umgesetzt in `src/dsl/blocks/RandomBlock.cpp` und `src/dsl/SignalChain.*`:

- Benutzer-Seed (`seed`), deterministischer Reset auf diesen Seed.
- Frei laufend (`rate`) oder host-synchron (`sync = 1/16 … 1/1`).
- Range `min`/`max`; vertauschte Grenzen werden getauscht.
- Verteilungen `uniform`, `triangle`, `gaussian`.
- `smooth` 0–1 als Ein-Pol-Glättung; 0 = Step-Random, >0 = Smooth-Random.
- `bipolar` skaliert um 0 mit dem größten Absolutwert der Range, sonst uni-polar.
- Jede Instanz hat eigenen RNG-Zustand; mehrere Random-Blöcke sind unabhängig.
- Parameter in `blockCatalog.ts`/`chipSpec.ts` ergänzt.

Tests in `tests/ModulationBlocksTest.h`:

- Gleicher Seed = gleiche Folge, anderer Seed = andere Folge, Range/vertauschte
  Grenzen/Bipolar, Host-Sync, Smoothing und Reset.

Ergebnisse:

| Prüfung | Ergebnis |
|---|---|
| Gesamte native Suite | 33239 passed, 5 failed (dieselben Alttests) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| Web-Build (`tsc` + Vite) | OK |
| ADSR-/StepSeq-/Katalog-Verträge | 39 passed; 1 alter `cables`-Default-Fehler |

Rohprotokolle: `build/c04-final-native.log`, `build/c04-final-contracts.log`.

Noch `NOT RUN` / offen:

1. Host-/DAW-Abnahme (VST3/Standalone, Host-Sync).
2. pluginval und Preset-Restore.
3. Drei C04-Test-Patches als Factory-Presets.
4. CPU-Messung bei hoher Rate und vier Random-Instanzen.

Nächstes Arbeitspaket: **C05 – Envelope Trigger** (Hysterese, Holdoff,
Doppeltrigger, Detektorquelle, Trigger-Puls).

---

## C05 – Envelope Trigger

**Status: `VALIDATION_PENDING`** (Implementierung und lokale Abnahme fertig;
DAW/pluginval noch `NOT RUN`)

Umgesetzt in `src/dsl/blocks/EnvTriggerBlock.cpp` und `src/dsl/SignalChain.*`:

- Threshold in dBFS (-60…0), Sensitivity 0–1 als Detektor-Vorverstärkung.
- Hysterese 0–24 dB: Re-Arm erst unterhalb `threshold - hysteresis`.
- Holdoff 0–1000 ms als Refraktärzeit gegen Doppeltrigger.
- Stabile Puls-Länge (`hold`), Pegel (`level`), Peak- oder RMS-Detektor.
- Live-Trigger-Zustand über `appendClipPeaks` → `hostStore.clipsTrigger` →
  Chip-Face/`EnvFace`; Trigger-Schwelle und Puls werden gezeichnet.
- Parameter in `blockCatalog.ts`/`chipSpec.ts` ergänzt.

Tests in `tests/ModulationBlocksTest.h`:

- Ein Transient = genau ein Trigger, Holdoff blockiert Doppeltrigger,
  Hysterese erzwingt echtes Re-Arm, Threshold-Grenzen/Stille/Peak vs. RMS.

Ergebnisse:

| Prüfung | Ergebnis |
|---|---|
| Gesamte native Suite | 33251 passed, 5 failed (dieselben Alttests) |
| `NeuroKoreDspContracts.exe` | `Failures: 0` |
| Web-Build (`tsc` + Vite) | OK |
| ADSR-/StepSeq-/Katalog-Verträge | 39 passed; 1 alter `cables`-Default-Fehler |

Rohprotokolle: `build/c05-final-native.log`, `build/c05-final-contracts.log`.

Noch `NOT RUN` / offen:

1. Host-/DAW-Abnahme (VST3/Standalone).
2. pluginval und Preset-Restore.
3. Drei C05-Test-Patches als Factory-Presets.
4. CPU-Messung bei hoher Triggerrate.

Nächstes Arbeitspaket: **C06 – Modulation Math** (zwei echte Mod-Eingänge,
alle Operationen, Audio-L/R-Semantik entfernen).

---

## Arbeitsregeln für diesen Lauf

- Ein Modul/Paket pro Änderung; kein paralleler Umbau.
- Keine erfundenen Messwerte; nicht ausgeführte Prüfungen bleiben `NOT RUN`.
- Produktversion, VST3-Class-ID, Parameter-IDs und Presets bleiben unverändert.
- Vor jedem Branchwechsel werden uncommittete Änderungen gesichert.
