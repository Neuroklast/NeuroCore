# NEUROKORE

Programmable real-time audio effect by **Neuroklast**. Load a factory sound, turn six knobs, or build your own chain.

English UI. Formats: **Standalone**, **VST3**, and on Mac an **Audio Unit**. Version **0.6.4-beta**.

Operator guide: [docs/manual/NEUROKORE.md](docs/manual/NEUROKORE.md). In-plugin Help is that guide minus Install.

## Install

Windows
1. Run `NEUROKORE-0.6.4-beta-Setup.exe` as administrator. Pick VST3 and/or Standalone (English or German).
2. If Microsoft Edge WebView2 is missing, the setup installs it. Without that runtime the window is empty.
3. Rescan plug-ins. The VST3 is always `C:\Program Files\Common Files\VST3\NEUROKORE.vst3`.
4. Standalone is in the Start menu under Neuroklast. Uninstall keeps your presets and license.

Mac — copy the VST3 to `/Library/Audio/Plug-Ins/VST3/` and the Audio Unit to `~/Library/Audio/Plug-Ins/Components/`, then rescan.

The first unlicensed launch starts a **14-day demo**. Settings → License shows the end date. After that, Mix stays dry until you install a `.lic`.

## 30-second path

1. Click the preset name. Double-click a factory row.
2. Play audio. Raise Mix.
3. Turn knobs a–f.
4. Circuit is the board (IN **out** / **sc** on the right; OUT gain under Details). Terminal is the same sound as text.

LIVE keeps delay low. STUDIO is linear-phase (more delay, mix-ready). CPU in the status bar is 0–100. LAT is the delay your DAW should compensate.

## License

Proprietary. Copyright (c) 2024–2026 NEUROKLAST. Testers receive a signed `.lic`. See [LICENSE](LICENSE). JUCE and the VST3 SDK stay under their own licenses.

## Build (developers)

Needs JUCE 8.0.6+ (`JUCE_DIR` or CMake fetches it) and a VST3 SDK.

```bash
cmake -B build -S .
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
```

Windows: `build_release.bat`. Mac also builds the AU. Artefacts: `build/NeuroKore_artefacts/Release/` (`NEUROKORE-0.6.4-beta.exe` / `.vst3` / `.component`).

Package Windows zip + installer (after Release). Needs [Inno Setup 6](https://jrsoftware.org/isinfo.php):

```powershell
powershell -File scripts/package_windows.ps1
```

That writes a clean `NEUROKORE.vst3` bundle, downloads the WebView2 bootstrapper, compiles Setup.exe, and writes a SHA-256 next to it. If `NEUROKORE_SIGN_PFX` (or `NEUROKORE_SIGN_THUMBPRINT`) is set, the VST3, Standalone, and Setup.exe are Authenticode-signed.

Formula language: [docs/DSL_REFERENCE.md](docs/DSL_REFERENCE.md). Agent rules: [AGENTS.md](AGENTS.md).
