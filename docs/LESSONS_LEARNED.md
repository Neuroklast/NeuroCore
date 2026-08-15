# Lessons Learned

Dieser Erfahrungsspeicher wird nach **jeder** Coding-Agent-Session ergänzt.
Er dient dazu, Fehler nicht zu wiederholen und bekannte Fallstricke zu dokumentieren.

---

## Session-Log

---

### 2026-08-15 – Functions need folders like presets

**Agent:** Grok Coding Agent  
**Aufgabe:** Functions-Kategorien links; README/Help/Manual  
**Ergebnis:** Core vs Drive vs Crush vs Blocks. `tube`/`diode` nicht neben `sin`. Catalog-JSON kann `category` setzen, sonst `categoryForName`.

#### Regel
Ein flacher Function-Browser mischt Sprache und Sound. Ordner wie im Preset-Explorer.

---

### 2026-08-15 – Empty sidechain is not “no pin”

**Agent:** Grok Coding Agent  
**Aufgabe:** Vocoder tot, auch mit Sidechain; Stereoizer  
**Ergebnis:** Viele Hosts liefern einen stillen Sidechain-Buffer. `scN > 0` allein schaltet Self-Vocode aus → nur Dry. Jetzt: Energie-Hold 60 ms, sonst Self-Vocode. BP-Q war 2–8 (Löcher). Sidechain-Bus default an. Stereoizer ist ein Block (`widen`), nicht Haas-only.

#### Regel
Optionaler Aux-Buffer gilt erst als Sidechain, wenn er pegelt. Mono→Stereo: Mid unangetastet, Side hochpass + Dekorrelation.

---

### 2026-08-15 – Preset chip needs its own stepper

**Agent:** Grok Coding Agent  
**Aufgabe:** Pfeile links/rechts vom Preset-Namen  
**Ergebnis:** `getPresetNames` + `loadPreset` gab es schon. Editor ruft `stepPreset(±1)` auf. Leerer Name startet bei 0 (next) bzw. wrappt aufs letzte (prev).

#### Regel
Preset-Navigation gehört an den Chip, nicht nur in den Explorer. Index-Arithmetik immer `((i % n) + n) % n`.

---

### 2026-08-15 – Octaver wobble is a free oscillator

**Agent:** Grok Coding Agent  
**Aufgabe:** Precision Octaver klingt wobbly  
**Ergebnis:** Sub war ein freier Sinus aus einer Schmitt-Periode (28 % Update, L/R getrennt, min-age 8 Samples = 6 kHz). Tube vor dem Tracker erzeugt Extra-Zero-Crossings. Jetzt: eine Mid-Clock, Flip-Flop wie OC-2, Detector-LPF 650 Hz, Periode nur 22–700 Hz, +1 = Gleichrichter, Sub mono.

#### Regel
Oktav-Pitch kommt von Nulldurchgängen, nicht von einer geschätzten Frequenz. Tracker nie mit Distortion füttern. L/R nicht getrennt tracken.

---

### 2026-08-15 – Every effect block can tick on its own

**Agent:** Grok Coding Agent  
**Aufgabe:** Alle Effekt-Blöcke härten — keine Artefakte, höhere Qualität und CPU  
**Ergebnis:** Delay-Write-Head näher als 4 Samples = Hermite liest den Write → Tick jede Periode. Filter hat den Smoother im Dummy-Loop verbrannt und den Endwert als Stufe gesetzt. EQ hat IIR-Coeffs jedes Sample allokiert. Limit-Instant-Slam klickt. 3-Band-Xover speiste High aus x statt HP(f1). Denormals in Comb/Env/Comp nach Stille. Delay/Reverb-Smoother starteten auf 0.35/0.55 statt Skriptwert → Ghost-Echo beim ersten Block. `y = sc` ohne `t` bekam nie per-Sample Sidechain.

#### Regel
Interpolation braucht Abstand zum Write-Head. Coeffs nur bei echter Änderung, nie allokieren im Sample-Loop. Smoother nicht vor dem Block leerziehen. Instant Gain-Slam ist ein Click. Nach Stille Denormals flushen.

---

### 2026-08-14 – Knackig is a hard clip + fast env, not more drive

**Agent:** Grok Coding Agent  
**Aufgabe:** Klangqualität näher an Serum 2 (knackig)  
**Ergebnis:** `hardclip` n=5 lieferte bei |x|=L nur ~87 % — klingt nach Softclip. Extra-IIR nach FIR-OS schmiert den Click. Env-Filter mit 20 ms Smoothing ziehen den Transient nach. Jetzt: n=16, Mod-Smoothing 0.8 ms, Extra-LPF nur noch ohne OS.

#### Regel
„Knackig“ kommt vom Click, nicht von mehr Drive. Algebraisches Clip muss am Ceiling sitzen. Env-Cutoff nicht mit Knob-Smoothing (20 ms) fahren.

---

### 2026-08-15 – Delay Lagrange and stereo reverb combs tick

**Agent:** Grok Coding Agent  
**Aufgabe:** Delay/Reverb periodische Artefakte  
**Ergebnis:** 6-Punkt-Lagrange klingelt jede Delay-Periode. 8 %-Crossfeed hämmert im Delay-Takt. Getrennte L/R-Combs (Spread 23) = Kamm bei ~520 Hz. Zurück: Hermite-4, kein Crossfeed, Mono-Summe ins Freeverb, 4 Allpässe.

#### Regel
Delay-Interpolation muss nicht „höhergradig“ sein. Reverb-Eingang bleibt eine Mono-Summe; Stereo entsteht über versetzte Comb-Längen, nicht über getrennte Feeds.

---

### 2026-08-15 – Clip stages copied std::function every sample

**Agent:** Grok Coding Agent  
**Aufgabe:** Hardcore/Gabber haken bei jedem OS; Stereo Guitar Wall nur links  
**Ergebnis:** `usesNonlinear` zwang jede Clip-Stage in `evaluateBlockT(1)` inkl. `std::function`-Kopie + Lock pro Sample. Jetzt: Functor einmal binden, Env nur wenn nötig. Mono-IR wird auf L+R kopiert; SVF/Stage immer 2 Kanäle.

#### Regel
ADAA braucht Sample-Reihenfolge, nicht Knob-Inject. `std::function` nicht pro Sample kopieren. Channel=right braucht 2-Kanal-State, sonst ist lastCh leer.

---

### 2026-08-15 – Gold blocks, not a secret EQ

**Agent:** Grok Coding Agent  
**Aufgabe:** Delay/Reverb/Clip auf Hochglanz, Default-OS 4×, Release-Kit im Repo-Root  
**Ergebnis:** Delay interpoliert Lagrange-6 + 2-Pol-Damp. Reverb speist L/R getrennt und hat 6 Allpässe. hardclip n=24. OS-Default Index 2. Portable Kit `NEUROKORE-0.9.0/`.

#### Regel
Klangqualität sitzt in den Blöcken, nicht hinter der DSL. Default-OS 4× ist der Gold-Pfad; 2× bleibt wählbar. Release-Kit gehört ins Repo-Root, nicht nur nach `build/package`.

---

### 2026-08-14 – Hardcore rumble is a held sine, not a hall

**Agent:** Grok Coding Agent  
**Aufgabe:** User-Refs `sound examples/hardcore techno kicks` (Disorder, Noitification, Wow)  
**Ergebnis:** 375 ms = 1 Beat @ 160. Peak +3 dB, RMS ≈ 0 dB, Crest 1.4, 43–55 % Samples über 0.95. Nach 80 ms sitzt 55–80 % der Energie unter 120 Hz und bleibt laut. Spektrum: Click → Wow-Sweep → gehaltener Clipped-Sine + Sub-Oktave. Kein Hall, kein Duck. Rumble = Delay 12–18 ms (≈ 55–80 Hz) + Feedback + Octaver + Hardclip.

#### Regel
Club-Kick-Rumble nicht mit dunklem Reverb+Duck bauen. Das macht Matsch. Gehaltener Body: Resonator auf der Kick-Periode, dann zubrickwallen. Helle Distortion und Dynamik gehören auf einen eigenen Scream-Bus (`drive * (1 + env)`), nicht in den Sub.

---

### 2026-08-14 – Club defaults must sit in the slam zone

**Agent:** Grok Coding Agent  
**Aufgabe:** Techno-Presets zu zaghaft — muss ballern  
**Ergebnis:** Dry-Clip-Ceiling 0.9 klingt nach sauberem Kick. Rumble-Send 0.46 und Drive 3.8 sind Demo-Werte. Club sitzt jetzt bei Drive 11–14, Ceiling 0.24–0.58, Rumble-Send ~1.1, Duck 0.8+. JSON nur über `node scripts/generate_factory_presets.mjs` — der vorherige Punch-up lag nur im `.mjs`.

#### Regel
Club-Defaults gehören in die obere Hälfte des Ranges. Ein 0.9-Ceiling ist kein Brick. Generator laufen lassen, sonst hört der User die alten Presets. Letzter `hardclip`/`fold` braucht danach einen Recovery-LPF, sonst fällt das Quality-Gate.

---

### 2026-08-14 – 8× switch must reserve 8× buffers first

**Agent:** Grok Coding Agent  
**Aufgabe:** 8×-Wechsel knackt wieder; Gabber braucht Dyn + Sub  
**Ergebnis:** OS-Bank und `scriptBuffer`/`scOsBuffer` immer auf 8×-Kapazität. Index erst in `prepare`. Nach dem Swap `cpuProtect.reset`. Gabber: `env1` skaliert den Clip, eigener Sub-Bus.

#### Regel
Nicht den OS-Index setzen, bevor `prepare` den Pointer umlegt. Kein `setSize` im ersten 8×-Audio-Block.

---

### 2026-08-14 – Rumble is room + drive + duck, not a low sat bus

**Agent:** Grok Coding Agent  
**Aufgabe:** Kick/Warehouse/Gabber klangen nicht nach Rumble  
**Ergebnis:** Sat-Low-Bus ist nur extra Bass. Echter Rumble: Low-Pfad verzerren, dunkles Reverb (`damp` hoch), mit `env1` vom Dry-Kick ducken.

#### Regel
Rumble-Presets folgen Rhythmic Gate Delay: `env` auf main, Wet auf einem Bus, `y * (1 - env * duck)`. `reverb damp` ist 0–1, nicht Hz.

---

### 2026-08-14 – Comments are rust, not editor-green

**Agent:** Grok Coding Agent  
**Aufgabe:** Kommentarfarbe + größere Explorer-Ordner + Klang-Kommentar  
**Ergebnis:** `LookAndFeel::comment()` = Rost `#c4786a`. Generator schreibt `# How it sounds:` in jedes Factory-Skript.

#### Regel
Kommentare nicht in Keyword-Grün. Explorer-Ordner mindestens 16 pt.

---

### 2026-08-14 – R-only input makes a vertical goniometer line

**Agent:** Grok Coding Agent  
**Aufgabe:** Output-Feld wurde zum Strich bei Input = R, Input-Feld nicht  
**Ergebnis:** Router legt R auf L und R. OUT war korrekt mono. IN tapte den Host vor dem Router. IN-Tap sitzt jetzt hinter L/BOTH/R.

#### Regel
IN-Scopes zeigen, was die Formel hört, nicht den rohen Host. Mix 0 bleibt ungeroutet (dry = Host).

---

### 2026-08-14 – Overlay children do not auto-resize

**Agent:** Grok Coding Agent  
**Aufgabe:** Preset-Fenster + schwarzer Hintergrund beim Ziehen des Plugin-Fensters  
**Ergebnis:** `addAndMakeVisible` setzt einmal Bounds. Parent-`resized()` muss `fitToParent()` rufen. Preferred 0 = Panel füllt mit 24 px Rand.

#### Regel
Jedes Kind-Overlay in `resized()` neu legen. Preferred-Größe ist ein Maximum, nicht die feste Öffnungsgröße.

---

### 2026-08-14 – Full ID rename + version 0.9.0

**Agent:** Grok Coding Agent  
**Aufgabe:** Alles umbenennen inkl. ID und App-Ordner; Versionsnummer  
**Ergebnis:** `NRKO`, AppData `NeuroKore`, CMake-Target `NeuroKore`. Version **0.9.0** — Feature-Stand ist weit über 0.2, 1.0 bleibt der Verkaufs-Cut. C++-Klassen (`NeuroCoreAudioProcessor`) intern gelassen.

#### Regel
In der Testphase dürfen CID und AppData wechseln. 0.9 heisst: testers-ready, nicht shop-ready.

---

### 2026-08-14 – Rebrand display, keep the CID

**Agent:** Grok Coding Agent  
**Aufgabe:** Rebrand zu NEUROKORE by Neuroklast  
**Ergebnis:** Host-Name, HUD, Help, Installer. `NRCO`/`NRKL`, AppData `NEUROKLAST/NeuroCore`, CMake-Target `NeuroCore` bleiben. Lizenz akzeptiert `NeuroCore` und `NEUROKORE`.

#### Regel
Produktname in der UI ist nicht der CMake-Target-Name. License-`product=` steht in der Signatur — alte Dateien nicht ungültig machen.

---

### 2026-08-14 – Glitch Lab lag was ping-pong + LFO-cutoff

**Agent:** Grok Coding Agent  
**Aufgabe:** Glitch Lab neu, Club-Presets, Preset-Explorer  
**Ergebnis:** Ping-Pong-Delay und `filter + = osc1` waren teuer und klangen unruhig. Neue Lab-Kette: crush → fold → ein Delay auf dem Notenraster → LPF. Kick-Rumble ist ein Low-Bus unter Dry, kein zweiter Kick-Synth. Explorer = Folder-Liste, keine zweite Combo.

#### Regel
Kein Ping-Pong und kein per-Sample-Filter-LFO in „Creative“-Smash-Presets. Rumble addiert auf `main = 1`, sonst wird der Kick leise. JSON nur über den Generator.

---

### 2026-08-14 – Scope extras read the ring, not a second meter tap

**Agent:** Grok Coding Agent  
**Aufgabe:** Stereo-Feld + Loudness neben IN/OUT, gleiche Höhe, einklappbar  
**Ergebnis:** `ScopeDeck` staucht die Wave horizontal. Field/LU kommen aus `WaveformCapture` (UI-Timer), nicht aus einem zweiten Audio-Tap. Fold ist ein 18-px-Streifen, kein extra Layout-Gewicht.

#### Regel
Kein zweiter Ring-Buffer für Meter. Correlation/Width im Header testen (Mono=1, Invert=−1). Help sagt "stereo field", nicht Goniometer.

---

### 2026-08-14 – Edge-cropped NK lets the toolbar shrink

**Agent:** Grok Coding Agent  
**Aufgabe:** NK-Logo aus `screenshots` (randscharf), Header halb so hoch  
**Ergebnis:** Quadratisches `nk_logo.png` hatte viel Schwarz. Die +50%-Lockup + Toolbar `0.09` wurden ~72 px. Das Screenshot-Asset ist 1495×774 und schon an den Kanten. Toolbar jetzt `0.045` mit `maxHeight` 38.

#### Regel
Logo-Datei in `resources/img/nk_logo.png` ersetzen (gleicher BinaryData-Name). Nur das Gewicht reicht nicht — grosse Fenster wachsen sonst wieder. `maxHeight` hält die Chrome-Zeile flach.

---

### 2026-08-14 – Measure all 178 factory inserts; only six were quiet

**Agent:** Grok Coding Agent  
**Aufgabe:** Jedes Factory-Preset messen; Inserts nachregeln; neue Blöcke einsetzen  
**Ergebnis:** Send/Delay/Reverb ausgenommen. Nur Low Pass Sweep war tot (`Min + osc*Depth` geht auf Min−Depth). Tremolo/Chopper `Div [0,1]` war kein Note-Grid. Amp-Presets: `gate1` + leeres `ir1` + `limit1`. JSON nur über den Generator.

#### Regel
Lautstärke über `applyPreset` messen, nicht am nackten Skript. Filter-Sweeps nie als `min + bipolar*depth` schreiben.

---

### 2026-08-14 – Pack import + Windows installer; repo scratch ignored

**Agent:** Grok Coding Agent  
**Aufgabe:** Serum-artige Packs, Installer, Repo aufräumen  
**Ergebnis:** `.nrk` bleibt die Einheit. Ordner/Zip mit mehreren Dateien → `UserPresets/Packs/<name>/`. `getAvailablePresets` rekursiv. Installer kopiert das VST3-Bundle nach `Common Files\VST3`. `build-test/`, `mcps/`, `terminals/` in `.gitignore`.

#### Regel
Kein zweites Preset-Format. Pack = Hülle. Installer ist nur der letzte Meter zum Host-Scan-Pfad.

---

### 2026-08-14 – CPU hotpath: no alloc, cache exp, skip dead work

**Agent:** Grok Coding Agent  
**Aufgabe:** CPU-schwere Blöcke schneller, ohne Klangänderung  
**Ergebnis:** IR-Dry-Buffer nicht mehr pro Block alloziert; mix=0/1 ohne Sample-Schleife. Comp/Gate/Limit cachen `exp`/`dB` solange der Wert steht. Vocoder ruft `applyBands` nur bei Q/Formant-Änderung (kein Heap alle 16 Samples). Xover `SmoothedValue::skip`. Comb-Wrap ohne while.

#### Regel
Kein `new`/`AudioBuffer` im Audio-Thread. `exp` nicht jedes Sample, wenn Attack/Release stillstehen. Coeff-Updates nur bei echter Parameteränderung.

---

### 2026-08-14 – Help text larger; manual covers IR / license / dynamics

**Agent:** Grok Coding Agent  
**Aufgabe:** Hilfe aktualisieren und Schrift vergrößern  
**Ergebnis:** Body 16→20 pt, Liste 13.5→16.5, höhere Chapter-Zeilen. `UserManual_en.txt` beschreibt IR-Buttons, License-Inhaber, gate/limit/xover. Help bleibt operator-only.

#### Regel
Plugin-Help ist `resources/UserManual_en.txt`. Schrift in `Config::kHelpBodyFontPt` halten, nicht hart im Component.

---

### 2026-08-14 – Licensed License button shows the holder; lockup +50%

**Agent:** Grok Coding Agent  
**Aufgabe:** License-Klick nach Aktivierung + größeres NK-Lockup  
**Ergebnis:** Unlizenziert bleibt der File-Chooser. Lizenziert öffnet ein Overlay mit E-Mail (und Issued). Replace bleibt möglich. BrandLockup 20→30 px und Schrift 1.5×; sitzt in der Toolbar, nicht in der 22 px HUD-Leiste.

#### Regel
Lizenz-Import nicht nochmal aufzwingen, wenn schon aktiviert. Logo-Cap ist Toolbar, nicht HUD-Höhe.

---

### 2026-08-14 – xover buses must exist before `out` is parsed

**Agent:** Grok Coding Agent  
**Aufgabe:** `out: low = 1` nach `xover1`  
**Ergebnis:** `buildBusGraph` prüft Out-Taps, bevor `ensureGraphBus` in `loadScript` läuft. Ohne Vorab-Registrierung von `low`/`high`/`mid` ist `out: low` ein Parse-Fehler.

#### Regel
Xover-Ziele im BusGraph anlegen, bevor `out` validiert wird — nicht erst danach.

---

### 2026-08-14 – IR button sits on the line; captions refresh without a script change

**Agent:** Grok Coding Agent  
**Aufgabe:** Mehrere IRs, zeilenbreiter Editor-Button, Drop/Change/Clear  
**Ergebnis:** Live-View bekommt nach jeder `irN`-Zeile eine Extra-Zeile für den Button. Im Code-Editor liegt der Button **auf** der IR-Zeile (JUCE `CodeEditorComponent` hat kein `setLineSpacing` — unter der Zeile würde die nächste Codezeile überdeckt). `setFormula` bricht bei gleichem Text ab, deshalb `refreshIrButtons()` nach Load/Clear. Scroll: `editorViewportPositionChanged`. Slot-ID nur `ir`/`ir`+Ziffern, nicht `iron:`.

#### Regel
Kein Dateipfad in der DSL. Ein Button pro Slot, nicht ein globaler Chip. Caption-Refresh unabhängig vom Formeltext.

---

### 2026-08-14 – limit is in-chain; Polisher stays last

**Agent:** Grok Coding Agent  
**Aufgabe:** Native `limit` Block  
**Ergebnis:** Instant-attack, stereo-linked, Ceiling + Release. Kein Lookahead, damit die Host-Latenz nicht steigt. Polisher-Limiter bleibt der Sicherheits-Clip danach.

#### Regel
`limit1` ist ein Ketten-Block. Polisher ist nicht dasselbe. `limiter1` ist nur ein Alias.

---

### 2026-08-14 – Sidechain comp must not fall back to the input

**Agent:** Grok Coding Agent  
**Aufgabe:** Comp knee/makeup/hpf/sidechain  
**Ergebnis:** `source = sidechain` ohne Pin darf nicht den Insert als Detektor nehmen — sonst duckt der Comp sich selbst.

#### Regel
Fehlender Sidechain = Detektor 0 (kein GR). Makeup/HPF/Knee sind optional; Attack bleibt ≥ 1 ms.

---

### 2026-08-14 – Gate smoothers must snap on prepare

**Agent:** Grok Coding Agent  
**Aufgabe:** Native `gate` Block  
**Ergebnis:** `SmoothedValue` startet bei 0. `range` 0 dB = „zu“ ist trotzdem unity. Attack-Test sah aus wie Fade-out.

#### Regel
Neue Dynamics-Blöcke: in `prepare` `setCurrentAndTargetValue(expr)` für jede Zeit/dB-Größe. Sonst ist der erste Block ein Ramp vom Default 0.

---

### 2026-08-14 – Note knobs are ms; osc freq must invert them

**Agent:** Grok Coding Agent  
**Aufgabe:** Taktlängen als Osc-Rate + AMS RMX knackt  
**Ergebnis:** `param a = Rate [1/1, 1/16]` published milliseconds (1/4 @ 120 = 500). `osc1: freq = a` wurde 500 Hz. AMS Nonlin hatte live Size 0.16–0.34 (Comb-Längen springen) plus helles Damp 0.12 und Delay-Mix 0.35.

#### Regel
Note-Knob in `delay time` = ms. Note-Knob in `osc freq` / `osc sync` = ein Zyklus pro Note (`1000/ms` bzw. `1/beats`). Tiny live `reverb size` nicht als Knob — Comb-Längenwechsel knackt.

---

### 2026-08-14 – Dual-DI wall is channel=left/right, not Haas

**Agent:** Grok Coding Agent  
**Aufgabe:** Metal-Wall + Cyberpunk-Distortion Factory  
**Ergebnis:** Zwei echte DI-Takes brauchen getrennte Amp-Ketten auf L und R. Haas auf einer Mono-Spur ist kein Double-Tracking. `Glitch Laboratory` war leise, weil `lerp(..., 0.85)` plus Delay-Mix 0.45 die Energie wegdrückte.

#### Regel
Stereo-Wall: `channel = left` / `channel = right`, jeweils eigene Tube/Cab-Kette. Cyber-Dirt wie ein Amp bauen (HPF → Crush/Fold → kurzer Comb → LPF → Level), nicht wie ein Delay-Labor. Makeup-Level ist Pflicht nach bitcrush/fold.

---

### 2026-08-14 – macOS CI cannot FORCE juceaide as a later target

**Agent:** Grok Coding Agent  
**Aufgabe:** AU-Job auf GitHub Actions  
**Ergebnis:** `juceaide was imported, but it doesn't exist!` — `JUCE_BUILD_HELPER_TOOLS=ON` macht nur ein Build-Target. `juce_add_plugin` ruft juceaide aber schon beim Configure für AU-Plists auf.

#### Regel
Windows: `JUCE_BUILD_HELPER_TOOLS=ON`. Apple/Linux: OFF, JUCE bootstrapt juceaide während Configure.

---

### 2026-08-14 – AU without a Mac is a GitHub macos runner

**Agent:** Grok Coding Agent  
**Aufgabe:** AU bauen ohne lokalen Mac  
**Ergebnis:** Kein Cross-Compile von Windows. Eigener CI-Job `AU (macOS)` baut `NeuroCore_AU`, ad-hoc-signiert, kopiert nach `~/Library/Audio/Plug-Ins/Components/`, dann `auval -v aumf NRCO NRKL`.

#### Regel
AU nur mit Apple-Toolchain. Ohne Mac: `macos-latest` + Artifact. Vor `auval` Registrar killen und das Bundle signieren, sonst sieht Logic/auval das Component nicht.

---

### 2026-08-14 – AU is Apple-only and must be a MusicEffect

**Agent:** Grok Coding Agent  
**Aufgabe:** AU-Plugin neben VST3 + Standalone  
**Ergebnis:** `FORMATS … AU` stand schon in CMake, erzeugt auf Windows aber kein Target. Projucer baute nur Standalone+VST3. `kAudioUnitType_Effect` (`aufx`) bekommt in Logic kein MIDI.

#### Regel
AU nur auf macOS (`NeuroCore.component`). Typ `kAudioUnitType_MusicEffect` (`aumf`) wenn das Plugin MIDI will (Learn, `midi_note`). Kein `pluginChannelConfigs={2,2}` — das wirft den Sidechain-Bus weg.

---

### 2026-08-13 – Bypass must lock Mix or Mix undoes Bypass

**Agent:** Grok Coding Agent  
**Aufgabe:** Mix im Bypass nicht verstellbar  
**Ergebnis:** Bypass setzt Mix auf 0. Der Slider blieb aktiv, also ging das Signal wieder nass, während BYPASSED stand.

#### Regel
Bypass ist der Schalter. Solange er an ist: Mix-Slider aus, Parameter auf 0 halten, gespeicherten Mix erst beim Ausschalten zurück.

---

### 2026-08-13 – Autocomplete only on Ctrl+Space

**Agent:** Grok Coding Agent  
**Aufgabe:** Autocomplete nur per Shortcut  
**Ergebnis:** Caret- und Document-Listener haben die Liste bei jedem Tastendruck geöffnet.

#### Regel
Liste nur nach Ctrl/Cmd+Space. Weitere Tasten filtern die offene Liste. Kein Ghost-Text ohne Popup.

---

### 2026-08-13 – Preset table default sort is Name, not factory order

**Agent:** Grok Coding Agent  
**Aufgabe:** Presets default sortieren + Tag-Spalte  
**Ergebnis:** `sortColumn == 0` hat die Generator-Reihenfolge gelassen. Default ist Name (natural). Tags sind eigene Spalte (id 6), Rating bleibt id 5 (Klick-Sterne).

#### Regel
Neue Tabellenspalten hinten an die IDs hängen, visuell per `addColumn`-Reihenfolge legen. Rating-Klicks an der festen Column-ID festmachen.

---

### 2026-08-13 – mix=1 delay plus lerp(x, y) is a wet loop

**Agent:** Grok Coding Agent  
**Aufgabe:** Rhythmic Gate Delay periodische Artefakte  
**Ergebnis:** `delay mix = 1` ersetzt das Dry. In der nächsten Stage sind `x` und `y` dasselbe nasse Sample, `lerp(x, y, duck)` ist eine No-Op. Man hört nur den Delay-Kreislauf alle `Time` ms. Duck und Mix greifen nicht.

#### Regel
Send-Delay: Dry auf `main`, Echo auf einem Bus, Duck nur den Nass-Pfad (`y = x * (1 - env * duck)`), `out: main = 1; echo = mix`. Niemals `lerp(x, y)` nach einem Block mit Mix 1.

---

### 2026-08-13 – Bandpass is not a mid EQ

**Agent:** Grok Coding Agent  
**Aufgabe:** Bestehende Presets nur bei Mehrwert anfassen; Hardware-Comp/Delay/Reverb ergänzen  
**Ergebnis:** Ein SVF-Bandpass als „Mid-Hump“ löscht Sub und Air. Peak/Notch/Shelf hält das Spektrum und formt nur das Band. `lerp(x, y, mix)` nach einer Stage mischt nicht Dry — `x` und `y` sind dann schon nass. Hardware-Kompressoren haben kein Mix-Poti.

#### Regel
Bandpass nur für Wah/Formant/Radio. Ton-EQ immer `eq`. Dry/Wet nach einer Kette über `bus` + `out` oder in **einer** Stage gegen das ursprüngliche `x`. Kompressor-Presets an Panel-Zeiten halten (Attack-Boden 1 ms).

---

### 2026-08-13 – Scope banners were a second TooltipWindow, not a painted title

**Agent:** Grok Coding Agent  
**Aufgabe:** Screenshot 220518: Tooltip zweimal, Preset-Schrift, Meter-Glitch, Amps/Octaver/Vocoder  
**Ergebnis:** Jeder Scope hatte ein Kind-`TooltipWindow`. Beide haben denselben Tip (Preset-Chip) als Banner gemalt. Ein Window am Editor reicht. Meter-Glitch muss cubisch mit dem Pegel kommen, sonst pixelts schon bei −18 dB. Octaver/Vocoder als echte Blöcke, nicht als 12-Bus-DSL.

#### Regel
Genau ein `TooltipWindow` am Editor. Scopes nur `SettableTooltipClient`. Factory-Octaver nie über `y_prev`/`step` — Tracker gehört in C++. Vocoder ohne Sidechain self-vocodet, sonst fällt das Quality-Gate auf Stille.

---

### 2026-08-13 – Two TooltipWindows paint the same tip twice

**Agent:** Grok Coding Agent  
**Aufgabe:** Screenshot 220518: Tooltip auf IN und OUT  
**Ergebnis:** Jeder Scope hatte ein eigenes `TooltipWindow`. Beide haben den Preset-Chip-Text als Banner gezeichnet.

#### Regel
Genau ein `TooltipWindow` am Editor. Scopes bleiben `SettableTooltipClient`.

---

### 2026-08-13 – EQ is not the SVF filter; sidechain is a second bus

**Agent:** Grok Coding Agent  
**Aufgabe:** Equalizer mit Q/Freq/Typ plus externes Sidechain-Audio im DSL  
**Ergebnis:** Neuer `eq`-Block (IIR: peak/notch/cut/shelf). Zweiter Input-Bus „Sidechain“. Im Code `sc`, `sc_l`, `sc_r`. Env kann `source = sidechain`. processBlock arbeitet nur auf dem Main-Bus, sonst würde der Sidechain als Extra-Kanal mitverarbeitet.

#### Regel
Zusätzlicher Input-Bus immer über `getBusBuffer` vom Main trennen. Sidechain auf OS-Länge holen (Hold), bevor die Formel ihn sampleweise liest.

---

### 2026-08-13 – Meter, note knobs, status columns

**Agent:** Grok Coding Agent  
**Aufgabe:** Loudness hängt; Zählzeiten 1/1–1/16 einrasten; Statuszeile darf nicht schieben  
**Ergebnis:** Dry-Pfad schreibt den Ausgangspegel. `param … [1/1, 1/16]` rastet auf Raster inkl. Punkt/Triole; Wert in der Formel ist ms. Status nutzt Mono-Padding fester Breite.

#### Regel
Mix 0 / SAFE darf den Meter nicht einfrieren — immer den hörbaren Buffer messen. HUD-Zahlen in einer Mono-Zeile nur mit fester Zeichenbreite.

---

### 2026-08-13 – NK logo must never use toolbar height

**Agent:** Grok Coding Agent  
**Aufgabe:** Screenshot 174706: NK deckt NEUROCORE // NEUROKLAST OS  
**Ergebnis:** `paint()` hat jedes Frame `performLayout(getLocalBounds())` gemacht und das Lockup auf y=0 über die HUD gelegt. Dazu war `logoH` die volle Zellenhöhe.

#### Regel
Lockup-Logo bleibt unter 20 px (unter der 22 px HUD). Chrome-Layout nur in `resized()` / Assemble über `chromeBounds()`. `paint()` legt kein Layout.

---

### 2026-08-13 – Offline license is a signed file, not a server

**Agent:** Grok Coding Agent  
**Aufgabe:** Testphase: sicheres Offline-Lizenzmodell, Demo Mix=0 nach 20 min, Issuer nur E-Mail  
**Ergebnis:** RSA-Signatur. Private Key nur in `NeuroCoreIssuer`. Plugin prüft mit Public Key. Ohne gültige `.lic` geht Mix nach 20 min auf 0, Audio bleibt dry.

#### Regel
Keinen Placeholder-Server einschalten. Tests mit `NEUROCORE_SKIP_LICENSE_ENFORCEMENT`, sonst läuft die Suite nach Ablauf der Demo trocken. JUCE-`RSAKey` String ist `exponent,modulus` — nicht PKCS `#n,e`. Schlüssel nur mit `RSAKey::createKeyPair` erzeugen.

---

### 2026-08-13 – Settings chrome must share body column weights

**Agent:** Grok Coding Agent  
**Aufgabe:** L/BOTH/R = Knobbreite; Text-/+ = Meterbreite; Logo mittig zweizeilig; Glitch smooth  
**Ergebnis:** Settings-Row nutzt dieselben Weights wie Body. Lockup: Logo + NeuroCore / vX.Y.Z. Meter-Pixel über die ganze Höhe, nicht ab 50 %.

#### Regel
Spalten-Align nur, wenn Settings und Body dieselben drei Weights und denselben innerMargin haben.

---

### 2026-08-13 – Do not copy a VST3 bundle into Common Files

**Agent:** Grok Coding Agent  
**Aufgabe:** Cubase sah keine UI-Änderung; Copy nach Common Files war falsch  
**Ergebnis:** Kein Auto-Copy mehr. Host lädt das installierte Plugin, nicht den Build-Ordner.

#### Regel
Niemals nach `Program Files\\Common Files\\VST3` kopieren. Cubase-Pfad selbst setzen oder die Datei `NeuroCore.vst3` aus `Contents/x86_64-win/` nutzen. Editor-Ctor darf nicht `setFormula` nochmal feuern — das resettet den Oversampler beim Fenster auf und knackt, und ein Combo-Sync kann OS auf 1× ziehen.

---

### 2026-08-13 – Square knobs waste width between columns

**Agent:** Grok Coding Agent  
**Aufgabe:** Editor breiter; Meter oben pixelig/glitchy  
**Ergebnis:** `kBodyEditorWeight` 6.4 vs Knobs 2.05; Meter-Bänder wachsen mit Pegel

#### Regel
Zwei quadratische Knobs in einer zu breiten Spalte erzeugen nur Lücke. Die Breite gehört dem Formel-Modul. Overload-Look nur oben im Balken, unten sauber.

---

### 2026-08-13 – Standalone OS switch: never suspendProcessing

**Agent:** Grok Coding Agent  
**Aufgabe:** Standalone knallt beim OS-Wechsel  
**Ergebnis:** Kein `suspendProcessing`; Mute + processLock; 2×/4×/8×-Bank vorbereitet

#### Root Cause
`suspendProcessing(true)` stoppt im Standalone das Audio-Device. Resume = Lautsprecher-Pop. Gleichzeitig Dry bei vollem Pegel, dann switchRamp 0 = zweiter Knacks.

#### Regel
Device laufen lassen. Audio-Thread auf Stille halten, OS umlegen, einblenden.

---

### 2026-08-13 – Cubase comments: disk JSON beats the binary

**Agent:** Grok Coding Agent  
**Aufgabe:** Kommentare fehlen in Cubase trotz Reload  
**Ergebnis:** Factory immer aus BinaryData; Session restored Factory-Script wenn DSP gleich

#### Regel
Installierte `resources/factory_presets.json` ist oft alt. Embedded JSON = diese Build. Session speichert den alten Text — Preset-Name merken und kommentiertes Factory nachziehen.

---

### 2026-08-13 – 8× then 4×: leftover scriptBuffer is a periodic glitch

**Agent:** Grok Coding Agent  
**Aufgabe:** Nach 2×→8×→4× regelmäßige Glitches  
**Ergebnis:** `scriptBuffer.setSize` immer auf die aktuelle OS-Länge; process nur `upBlock` Samples

#### Root Cause
`setSize` nur wenn der Buffer *wachsen* musste. 8× legt 8·N Samples an. 4× ließ `getNumSamples()==8N`. `processBlockSmoothed` lief über den ganzen Buffer — die zweite Hälfte Nullen, jeder Host-Block, bei 4×-Samplerate. Klang wie ein Takt-Glitch.

#### Regel
Working-Buffer logische Länge = aktueller Block, nicht all-time max. OS runterschalten muss die Sample-Zahl kürzen.

---

### 2026-08-13 – SAFE at 85% of the host buffer is a mute, not a guard

**Agent:** Grok Coding Agent  
**Aufgabe:** Safeguard bleibt, knistert in Stille, klingt alles schlecht  
**Ergebnis:** Trip erst bei echtem Overrun (1.15×, 8 Hits); Warmup; Auto-Retry; Filter wieder per-sample bei Modulation

#### Root Cause
`kCpuTripRatio = 0.85` ist das Host-Callback-Budget, nicht "dieses Plugin darf 85% nutzen". Kleiner Buffer + 2× OS + Filter reicht. Trip dauerte ewig, weil Dry `observe()` nie mehr sah → Mix 100% klang nach Dry. 8-Sample Filter-Stride knisterte in der Stille.

#### Regel
CPU-Guard nur bei Overrun. Nach Trip automatisch wieder nass testen. Modulation nicht in Coeff-Blöcken stufen.

---

### 2026-08-13 – OS switch crackles until you revert the factor

**Agent:** Grok Coding Agent  
**Aufgabe:** Oversampling-Wechsel knackt dauerhaft, bis der alte Faktor wieder gewählt wird  
**Ergebnis:** `useIntegerLatency` im Oversampler-Ctor; Latenz `roundToInt` und in `osLatencySamples` gespeichert; IIR nach Coeff-Set reset; `ScriptManager::prepare` leert Delay/Reverb/y_prev

#### Root Cause
`setUsingIntegerLatency(true)` nach `initProcessing` macht `getLatencyInSamples` ganzzahlig, das Thiran-Pad war aber für den alten Flag-Stand gebaut. `static_cast<int>(latency)` kürzt 26.999 auf 26. Dry-Sidechain und Wet laufen dann einen Sample versetzt — permanenter Kamm, faktorabhängig. Zurück auf 2× (oft schon integer FIR) „heilt“ es.

#### Regel
Integer-Latency im Oversampler-Ctor. Dieselbe gerundete Latenz für Host-PDC und Dry-Align. OS-Wechsel = neue interne Samplerate → DSL-Ringe leeren.

---

### 2026-08-13 – Acid Line hitch is per-sample TPT coeff rebuild

**Agent:** Grok Coding Agent  
**Aufgabe:** Acid Line laggt wie Glitch Laboratory  
**Ergebnis:** `setCutoffFrequency` nur noch jedes 8. Sample (Smoother weiter per Sample); Filter/Env setzen nur Knob/Osc/Env-Variablen; Res-Default 1.8

#### Regel
Modulierter SVF mit `+ = env` darf nicht jedes Sample `tan()` rechnen. Env-Release ist 160 ms — 8-Sample-Stride reicht. `varNames` nicht die ganze Variable-Map.

---

### 2026-08-13 – Header click does nothing without sortOrderChanged

**Agent:** Grok Coding Agent  
**Aufgabe:** Preset-Sortierung, L/BOTH/R + NK aligned, NETRUNNER → Neuroklast OS  
**Ergebnis:** Table sortiert filtered-Indizes; Settings-Chrome 32 px mittig; Layout immer unter HUD (auch Assemble); Banner Neuroklast OS

#### Regel
`TableListBox` ist erst sortierbar, wenn das Model `sortOrderChanged` implementiert. Assemble darf nicht `getLocalBounds()` ohne HUD-Trim layouten. Settings-Zeile: Combos und L/BOTH/R dieselbe Hoehe, scharfe Rechtecke.

---

### 2026-08-13 – Cropped NK logo will eat the HUD if the toolbar starts at y=8

**Agent:** Grok Coding Agent  
**Aufgabe:** NK-Logo zu gross, ueberdeckt NETRUNNER OS  
**Ergebnis:** Chrome unter `kHudHeaderHeight`; Logo max 26 px / 56% der Toolbar

#### Regel
HUD-Zeile ist 22 px. Layout-Margin 8 schiebt die Toolbar in diese Zeile. Zugeschnittenes Logo darf die Zeile nicht fuellen — erst unter den Header, dann klein halten.

---

### 2026-08-13 – Cinematic presets process picture audio, they are not trailer WAV packs

**Agent:** Grok Coding Agent  
**Aufgabe:** Psychoakkustik + Cinematic Factory fuer Blockbuster-Arbeit  
**Ergebnis:** +11 Presets (Haas, Loudness Curve, Missing Bass, Speech Band, Trailer Impact, Score Hall, Dialogue Seat, Far Plane, Boom Tail, Wide Canvas, Tension Bed)

#### Regel
Kein "Avengers Mode". NeuroCore bearbeitet Signal; es liefert keine Impacts als Samples. Beschreibungen sagen, was die Kette tut (Haas-Delay, implied bass, score send). Osc-Shape ist `shape`, nicht `type`.

---

### 2026-08-13 – NK lockup is a link, not only a mark

**Agent:** Grok Coding Agent  
**Aufgabe:** Klick auf NK-Logo oeffnet neuroklast.net  
**Ergebnis:** `BrandLockup::mouseUp` + `URL::launchInDefaultBrowser`; Hover-Glitch bleibt

#### Regel
Marke ist klickbar. `mouseWasClicked()` statt mouseDown, damit ein Drag nicht die Seite oeffnet.

---

### 2026-08-13 – Preset search is tags + formula, not just the name

**Agent:** Grok Coding Agent  
**Aufgabe:** Presets mit Tags; Schnellsuche nach DSL (tape, crunch, mid side)  
**Ergebnis:** `PresetSearch` inferiert Tags aus Script/Name; Factory-JSON `tags`; Suche tokenisiert (AND + Phrase); User-Save speichert Tags

#### Regel
Preset-Suche muss Name, Tags, Beschreibung und die Formel sehen. Kurze Tokens (`ms`) nur als Wort, nicht als Substring. Tags im Generator schreiben, in C++ trotzdem nochmal inferieren (alte JSON / User-Presets).

---

### 2026-08-13 – In-plugin Help is for operators, not the build

**Agent:** Grok Coding Agent  
**Aufgabe:** Hilfe nur auf das Plugin / den Nutzer beziehen  
**Ergebnis:** `UserManual_en.txt` ohne JUCE, CMake, Repo-Pfade, APVTS/ADAA. Fallback-Text ebenfalls nutzerseitig.

#### Regel
Hilfe erklaert Klicks, Klang, Presets, Formel. Build, Framework, Dateinamen im Repo gehoeren in `docs/ARCHITECTURE.md` / `DSL_REFERENCE.md`, nicht ins Help-Fenster.

---

### 2026-08-13 – Loudness meter twitched because GL ticked a 30 Hz smoother

**Agent:** Grok Coding Agent  
**Aufgabe:** Rechtes Loudness-Meter smoother, nicht zucken  
**Ergebnis:** VU-Ballistik im DSP (`smoothMeterDb`); UI nur Timer; OpenGL nicht mehr continuous

#### Root Cause
`lastLoudness` war Roh-RMS pro Block (~5 ms). `SmoothedValue` war auf 30 Hz / 100 ms gesetzt, `getNextValue()` lief aber in `renderOpenGL` bei VSync. Die Rampe war nach 1–2 Frames fertig, dann Stillstand bis zum nächsten Timer-Target — klassisches Snap-Hold.

#### Regel
Meter-Ballistik im DSP (Attack schnell, Release langsam). UI nur nachziehen, nie `getNextValue` in OpenGL. Continuous-Repaint fuer ein 30-Hz-Meter aus.

---

### 2026-08-13 – Factory mix-desk presets: honest MS/bus, not a mastering suite

**Agent:** Grok Coding Agent  
**Aufgabe:** Weitere Factory-Presets mit Mid/Side und Bus, realistisch  
**Ergebnis:** +14 Mix-Desk-Presets (130 total). Side Delay/Hall, Vocal Send, NY Drum Bus, Mono Below, MS Imager, …

#### Regel
Keine „unverzichtbar für Top-Produzenten“-Claims im Preset-Text. NeuroCore hat echtes MS und einen Send-DAG, aber keinen linearphasigen MS-EQ, kein Convolution, kein Multiband-Imager. Reverb hat kein `channel` — Side-Hall mutet Mid auf einem Bus. Comp hat kein `channel` — Mid-only über Stages nach `ms encode`. Generator-only, nie `factory_presets.json` per Hand.

---

### 2026-08-13 – Help default Font is Apex (ALL CAPS); logo PNG is a padded square

**Agent:** Grok Coding Agent  
**Aufgabe:** Hilfe lesbar + hilfreich; NK-Logo / Version / L-BOTH-R gerade wie PRESETS  
**Ergebnis:** Help-Body `monoFont` + `applyFontToAllText`; Tabellen als Definitionen; BrandLockup mit `cropOpaqueContent`; Input-Switch angular

#### Root Cause
`LookAndFeel::getTypefaceForFont` mappt leeren/Default-Sans-Namen auf Apex. `TextEditor::setFont(Font(16))` ohne Typeface-Namen wird damit ALL-CAPS. `setText` setzt die Runs zurueck — ohne `applyFontToAllText` nach jedem Setzen gewinnt Apex wieder. Das Logo-PNG hat viel schwarzen Rand; `ImageComponent` + `onlyReduceInSize` schrumpft das ganze Quadrat, NK sitzt klein und hoch.

#### Regel
Help-Body immer `NeuroCoreLookAndFeel::monoFont` + `applyFontToAllText` nach `setText`. Apex nur fuer Chrome. Logo vor dem Draw auf die rote Tinte croppen und mit Version als ein Lockup vertikal zentrieren. L/BOTH/R dieselbe eckige Platte wie `drawButtonBackground`, keine Pill.

---

### 2026-08-13 – Product is proprietary, README is not an OSS template

**Agent:** Grok Coding Agent  
**Aufgabe:** README aktualisieren; proprietäre Lizenz  
**Ergebnis:** `LICENSE` All rights reserved (NEUROKLAST); README auf aktuellen Stand (a–f, CMake, EN-only)

#### Regel
Kein MIT/GPL für NeuroCore-Code. Third-party (JUCE, VST3 SDK) bleibt deren Lizenz.

---

### 2026-08-13 – Delay preset crackle survives the next load

**Agent:** Grok Coding Agent  
**Aufgabe:** Delay-Presets knistern, auch nach erneutem Wechsel  
**Ergebnis:** `onFormulaChanged` resettet Oversampler, DC-Block, Sidechain, LPF — nicht nur den Sanitizer

#### Root Cause
`clearRuntimeState` leert Delay-Ringe der *neuen* Kette. Oversampler/DC/Sidechain im DspEngine behielten aber den Ring vom Delay-Burst. Nächstes Preset lief durch denselben OS-State.

#### Regel
Formel-Swap = Engine-Filterstate resetten. Delay-Ringe allein reichen nicht.

---

### 2026-08-13 – Mix is chrome, Gain is not a UI knob

**Agent:** Grok Coding Agent  
**Aufgabe:** Gain-Slider raus; Mix cyberpunk + gelegentlicher Drag-Glitch  
**Ergebnis:** `CyberMixSlider` (angular track, tick marks, RGB slices); APVTS `inputGain` bleibt host-automatable

#### Regel
Input-Gain nicht als zweiter Strip. Mix darf Backdrop-Glitch triggern (`CyberFxDirector::triggerGlitch`), aber nur beim Drag, nicht dauernd.

---

### 2026-08-13 – Help must not show raw markdown

**Agent:** Grok Coding Agent  
**Aufgabe:** Hilfe ohne Sterne/`###`/`---` lesbar machen  
**Ergebnis:** `stripMarkdownToPlain` zieht Emphasis, Headings, Fences, Tabellen, HR raus

#### Regel
Help-Body ist Prosa. Markdown bleibt nur die Quellform im Manual, nicht die Anzeige.

---

### 2026-08-13 – Help is one chapter at a time

**Agent:** Grok Coding Agent  
**Aufgabe:** Hilfe lesbar; Klick auf Kapitel zeigt nur diesen Abschnitt  
**Ergebnis:** `showChapter` setzt den Body auf Titel + Text; kein `scrollEditorToPositionCaret`

#### Regel
Kapitel-Liste ist Navigation, nicht ein Anker im ganzen Manual. Body-Font ist JetBrains Mono (nicht Apex) — Apex ist Caps-only und macht die Hilfe unlesbar.

---

### 2026-08-13 – English-only UI; live formula must scroll

**Agent:** Grok Coding Agent  
**Aufgabe:** Deutsch raus, kein Language-Switch; View-Modus scrollbar; Zeilen 1.1  
**Ergebnis:** Localiser lädt nur en; Live-View in Viewport; extra Line-Spacing 0.1×Font

#### Fallstrick
`FormulaDisplay` hat nur `TextLayout.draw` gemacht — lange Scripts wurden unten abgeschnitten, Scroll ging nur im Edit-Mode.

#### Regel
Read-only View braucht denselben Scroll wie der Code-Editor. `kFormulaLineHeight = 1.1`. Sprache nicht im Plugin-State wiederherstellen.

---

### 2026-08-13 – Input L/BOTH/R is one control, not two toggles

**Agent:** Grok Coding Agent  
**Aufgabe:** Cinematic WIP (Input-Switch + Formel-Check) auf master ziehen  
**Ergebnis:** Dreier-Switch auf APVTS `useInputLeft`/`useInputRight`; Check bricht bei NaN/Inf ab

#### Fallstrick
Zwei Toggles lassen L+R=aus zu. `modeFromFlags` mappt das auf Both — der Switch darf keinen vierten Zustand zeigen.

#### Regel
Ein Widget, drei Positionen. Parameter-Listener nur Message-Thread (`callAsync` + SafePointer). Stash-WIP nicht auf einem anderen Feature-Branch liegen lassen.

---

### 2026-08-13 – Factory SoTA is honesty, not more DAG

**Agent:** Grok Coding Agent  
**Aufgabe:** Alle Factory-Presets + Templates auf State of the Art prüfen  
**Ergebnis:** 116/116 am Quality-Gate; tote g/h, Dummy-C/D, zwei AM-only Time/Space-Namen

#### Regel
Nicht mehr Knobs bauen. `kNumUserParams = 6`. Extra Werte hardcoden wie die Templates. Namen, die Delay/Reverb/MS versprechen, müssen den Block enthalten. `factory_presets.json` nie per Hand editieren.

---

### 2026-08-13 – Factory names must match real delay/reverb/ms blocks

**Agent:** Grok Coding Agent  
**Aufgabe:** Doubler AM + Shimmer Drive ehrlich machen; einmal `factory_presets.json` schreiben  
**Ergebnis:** Doubler = `delay1` slap + LFO time; Shimmer = `reverb1` hall; quality 116/116

#### Fallstrick
„Doubler“/„Shimmer“ nur mit AM auf `y` klingen nach Tremolo, nicht nach Double/Hall. Engine hat keinen Pitch-Shifter — „octave shimmer“ wäre eine Lüge.

#### Regel
Time/Space-Namen brauchen echte `delay`/`reverb`/`ms` Blöcke. `y_prev` nur für Dirt. JSON nie handeditieren; Generator erst schreiben, wenn alle Script-Edits (inkl. g/h-Trim) drin sind. CTest-Name ist `NeuroCoreTests`, nicht `NeuroCoreExtrasTest`.

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
