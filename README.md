# NEUROKORE

Programmable real-time audio effect by **Neuroklast**. You write a short DSL formula (stages, filters, delay, reverb, compressors, OTT, octaver, vocoder, stereoizer) and map six host knobs **a–f** into it.

The UI is English-only. Formats: Standalone, VST3, and on macOS an Audio Unit (`.component`).

## Build

Needs JUCE 8.0.6+ (set `JUCE_DIR` or CMake will fetch it) and a VST3 SDK under `juce_audio_processors/format_types/VST3_SDK`.

```bash
cmake -B build -S .
cmake --build build --config Release --target NeuroKore_Standalone
cmake --build build --config Release --target NeuroKore_VST3
cmake --build build --target NeuroKoreTests
ctest --test-dir build
```

On **macOS** also build the AU (Logic, GarageBand, Ableton AU slot):

```bash
cmake --build build --config Release --target NeuroKore_AU
```

Artefacts land in `build/NeuroKore_artefacts/Release/` (`Standalone/NEUROKORE-0.5.1-alpha.exe`, `VST3/NEUROKORE-0.5.1-alpha.vst3`, and on macOS `AU/NEUROKORE-0.5.1-alpha.component`). The web editor is embedded in those binaries. A macOS build also copies the AU to `~/Library/Audio/Plug-Ins/Components/`.

Audio Units are an Apple format: a Windows or Linux CMake run still lists `AU` in `FORMATS`, but JUCE skips the target. You cannot produce a `.component` on Windows.

Without a Mac, use GitHub Actions: the **AU (macOS)** job builds, signs, runs `auval -v aumf NRKO NRKL`, and uploads `NEUROKORE-AU-macOS`. Trigger it with a push/PR or **Actions → Build → Run workflow**. Download the artifact, then copy `NEUROKORE.component` to `~/Library/Audio/Plug-Ins/Components/` on the Mac that will load it.

On Windows, double-click `build_release.bat` (or `build_debug.bat`). That configures `build/` if needed and builds `NeuroKore_All` — VST3 + Standalone + embedded Vite UI. Pass `/nopause` for scripts. Keep the `.bat` version string in sync with `NEUROKORE_VERSION_LABEL` in `CMakeLists.txt`.

Windows zip / installer (after a Release build):

```powershell
powershell -File scripts/package_windows.ps1
```

That stages `NEUROKORE-0.5.1-alpha.vst3` and `NEUROKORE-0.5.1-alpha.exe`, writes `build/package/NEUROKORE-0.5.1-alpha-win64.zip`, fills the portable kit `NEUROKORE-0.5.1-alpha/` in the repo root (VST3, Standalone, Docs, EULA), and compiles `installer/NeuroKore.iss` if [Inno Setup 6](https://jrsoftware.org/isinfo.php) is installed (`ISCC.exe`). The installer copies the VST3 to `C:\Program Files\Common Files\VST3\`. Testers only need the VST3 and the exe — no `web/` folder.

Oversampling defaults to **4×**. Drop to 2× or 1× if the CPU is tight.

## Using it

1. Load the Standalone, the VST3, or on macOS the AU (`NEUROKORE.component`) in a host.
2. Open **Presets** and pick a factory preset (amp, delay, vocal chain, …). Use **<** / **>** next to the name chip to step through the library. If the loaded script already matches a factory preset, that name is shown.
3. Tweak knobs **a–f**. Names and ranges come from `param` lines in the formula. The live value sits under the pointer.
4. **Circuit** is the board: 16 px snap. IN top-left, OUT bottom-right. Bound knobs print their letter on the chip (no cables from the knob column). Ctrl+wheel zooms. Cables are Manhattan. Right-click to add Bus, Noise Gate, Mid-Side Split, L/R Split, Crossover.
5. **Terminal** is the hack: the same construct as text. Live view shows `a[3.20]`. **Edit** opens the editor; **Save** applies it.
6. **Settings → Tempo**: follow the host BPM or type your own. The footer shows HOST or USER.
7. **Functions** is a catalog with folders on the left: **Core** (math), **Drive** (tube/diode/clip), **Crush** (fold/bitcrush), **Blocks** (ott, widen, vocoder, …).

Example:

```
param a = Drive [0.5, 6.0]
param b = Tone [800, 9000]
stage1: y = softclip(x, a)
filter1: type = lowpass; cutoff = b; resonance = 0.25
```

Full language notes: in-plugin **Help**, `docs/USER_MANUAL.md`, `docs/DSL_REFERENCE.md`.

## Factory content

305 factory presets covering professional production techniques. Includes amp models, analog octaver, vocoder (voice on Sidechain), OTT Smash, Mono to Stereo (`widen`), 3-band glue, and hardware-character comps/delays/rooms (1176, LA-2A, SSL bus, Fairchild, dbx 160, CL 1B, Space Echo, Memory Man, EMT 140, Lexicon hall, AMS NonLin). Dual-DI metal (`Stereo Guitar Wall`), digital guitar dirt (`Cyberpunk Drive`), and five cyberpunk Mesa-style amps (`Neon Rectifier`, `Night City Stack`, `Chrome Bit Mesa`, `Roundhouse Lead`, `Priest Bit Split`) are factory rows. **New:** 30 advanced genre-specific presets for hardstyle, cyberpunk, midtempo, and industrial (Zatox, DJ Hyper, Sub Zero Project, rekkt, zerosum, SWARM styles) featuring pre-distortion EQ sweeps, clip-to-zero loudness, LFO grooves, phase alignment, multiband processing, transient shaping, and granular effects. Each preset has tags inferred from the formula; the Presets search matches name, tags, and DSL. Templates live in `resources/templates.json`.

## Agent workflow

See `AGENTS.md` and `docs/agents/workflow.md`. Every coding session: plan first, small changes, tests, update `docs/DEVELOPMENT_STATUS.md` and `docs/LESSONS_LEARNED.md`.

## License

**Proprietary.** Copyright (c) 2024–2026 NEUROKLAST. All rights reserved.

Testers get a signed `.lic` file. Build `NeuroKoreIssuer`, type the email, save the file. In the plugin click **License** and pick that file. Without a license, Mix drops to 0 after 20 minutes (audio stays dry).

See [LICENSE](LICENSE). You may not copy, modify, or distribute this software without a written agreement from NEUROKLAST. JUCE and the VST3 SDK remain under their own licenses.
