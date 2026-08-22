# NEUROKORE

NEUROKORE is an insert or send effect by Neuroklast. Load a factory sound, turn six knobs, or build your own chain on the board. Version **0.6.0-beta**. Formats: Standalone, VST3, and on Mac an Audio Unit.

This file is the operator guide. The same text is what Help shows inside the plugin.

Put NEUROKORE on a track, or on a send return. Three views share one sound:

- **Unit** — meters, Mix, knobs, and the live path. Play here.
- **Circuit** — the board. Blocks, jacks, cables.
- **Terminal** — the same sound as text. Edit, then Save.

Click the wordmark to open neuroklast.net in your browser. There is no address bar inside the plugin.

---

## Install

Windows
1. Run `NEUROKORE-0.6.0-beta-Setup.exe` as administrator. Choose English or German. Pick **VST3** (every 64-bit DAW) and/or **Standalone**.
2. If Microsoft Edge WebView2 is missing, the setup installs it first. Without that runtime the window is empty.
3. The VST3 is always `C:\Program Files\Common Files\VST3\NEUROKORE.vst3` (same folder on every version — not a new plug-in each time). Rescan in the DAW. Older `NEUROKORE-<version>.vst3` folders are removed.
4. Standalone is in the Start menu under Neuroklast. Your presets and license stay in AppData if you uninstall.
5. A SHA-256 file sits next to the Setup.exe so you can check the download.

Mac
- VST3: `/Library/Audio/Plug-Ins/VST3/`
- Audio Unit: `~/Library/Audio/Plug-Ins/Components/`
- Then rescan, or log out once if Logic does not see it yet.

Linux is VST3 or Standalone. You need a WebKitGTK web view on the machine.

The first unlicensed launch starts a **14-day demo**. Help → License shows the end date. After that, Mix stays dry until you install a license.

---

## Quickstart

1. Click the preset name in the top bar. Double-click a factory row (amp, delay, reverb, vocal, pitch, multiband).
2. Play audio. Raise Mix until you hear the effect. Mix at 0 is dry.
3. Turn knobs **a–f**. Names and ranges come from the loaded sound.
4. Want your own? Open Terminal, click Edit, change the text, then Save. Or build it on Circuit. Then open the preset name and **Save As...**

On an insert, start Mix around 40–70%. On a send, push Mix high and use the host send level.

L / BOTH / R chooses which input side the sound hears. BOTH is the default.

---

## Playing

Top bar
- Preset name — click for the library. Untitled if nothing is loaded. `<` / `>` step through factory, then your sounds.
- Functions — look up formula words and insert them.
- Stages — blocks in the current sound. Select an IR block to open that cabinet slot.
- Settings — motion, Live / Studio, theme, frame rate, unsaved prompt, cables, tempo, standalone audio device, About, License, Help. These apply to every instance on this machine and are kept after you close the DAW. Resize the window by dragging the frame.
- LIVE / STUDIO — Live keeps delay low so playing feels immediate. Studio uses linear-phase oversampling for mix and master work (more delay; the host shows it as latency). Same switch lives in Settings.
- Bypass — Mix to 0 (dry) and locks the Mix slider. Turn Bypass off and the previous Mix comes back.
- Help — this guide.

Knobs
- Six knobs a–f, automatable from the host. On Circuit they sit under the board. On Terminal they sit on the left.
- Click the name to rename it. Scroll the wheel to turn. Type a number or a note length (`1/4`) into the value.
- Right-click a knob: Name / Min / Max / Unit / Note / MIDI Learn. Not the browser menu.
- If a knob does nothing, that letter is not used in the current sound.

Oversampling (OS)
- 1×, 2×, 4×, 8×. Higher OS is cleaner on heavy drive, and costs CPU and delay.
- Pick OS on the Unit face. Studio vs Live is how that OS is built, not a second OS menu.

---

## Presets

Click the preset name.

- **Factory** sounds are locked. You can load them and tweak knobs. Save As... to keep a copy under your name.
- **Your** sounds live in your user preset folder as `.nrk` files.
- A star on the name means unsaved edits. Next / previous / load another will ask to discard, keep editing, or Save As... (turn that prompt off in Settings if you want).
- Discard restores the last loaded or saved version.
- Import artist packs (a folder of `.nrk`, or a zip) from the explorer.

---

## Circuit

The board is the same sound as Unit and Terminal.

Cables
- Drag a chip by its body. Audio jacks are the squares on the sides. Drag from an output (right) to an input (left).
- The line follows your pointer until you drop. After you drop, the board draws a clean path.
- Dots or dashes on a cable show that block’s output: they stand still with no signal or below −60 dB, and run faster as that chip gets louder.
- Right-click empty board to add a block. Right-click a cable to insert a block on that run. Right-click a chip for Details / Mute / Solo / Delete.

IN and OUT
- **IN** is locked on the left. Two outputs on the right: **out** is the track, **sc** is the host sidechain. `sc` is an output so you can feed a compressor, gate, or vocoder. The lamp next to IN is on when the host sidechain pin is enabled. If the pin is off, `sc` is silent.
- **OUT** is the mix back to the host. Expand it (chevron or right-click Details) for **gain** (−24 to +12 dB). Gain is not a slider on the closed tile.

Arrange and Compact
- Right-click the board → Arrange (spread) or Compact (tighten). Loading a preset also lays the board out. A small “Arranging…” tag can appear; the window stays usable.

Expand a chip for hidden parameters. The chip size does not jump.

---

## Terminal

The text is the sound.

- Live view follows the knobs.
- **Edit** opens the editor. Suggestions stay closed until Ctrl+Space (Cmd+Space on Mac).
- **Save** applies the text. If it does not parse, you stay on the last good sound.
- **Validate** lists checks without applying.

You never have to use Terminal. Circuit writes the same text for you.

---

## Sidechain and cabinets

Sidechain
- Enable the extra input on the plugin in your DAW (often “sidechain” or input 3/4).
- IN grows an **sc** output. Cable it into a compressor side input, a gate, a vocoder voice, or anything that should hear the other track.
- Factory sounds that need sidechain say so in the preset blurb.

Cabinets (IR)
- Impulse slots sit on cabinet / IR blocks. Select the block in Stages, or right-click the chip → Load cab.
- Drop a WAV, or pick a factory cabinet. Latency from a long IR is included in the status-bar delay figure.

---

## License

Unlicensed: **14-day demo** from the first launch on this machine. The status bar and Settings → License show remaining days and the end date (for example `DEMO 12d · 3 Sep 2026`).

When the demo ends, Mix is forced dry. Load a `.lic` via Settings → License (Install .lic, or paste a key if you bought online). Licensed machines show **LIC** and your buyer email.

The Software is licensed, not sold. Third-party pieces (including the audio engine toolkit and VST3) stay with their owners.

---

## Support

Status bar (left to right)
- **MODE** — STUDIO, LIVE, or SAFE. SAFE means the plugin went dry because the audio callback ran too long. It is a word, not a percent. CPU never reads above 100.
- **CPU** — 0–100, share of the audio buffer. If you ride near 100, drop OS, simplify the board, or switch Live vs Studio.
- **LAT** — delay the host should compensate (milliseconds and samples). Same number as the DAW’s plugin delay. Live is short. Studio plus a long cabinet is longer.
- **SR / BUF / BPM** — sample rate, block size, tempo. HOST follows the DAW; USER is the tempo you typed in Settings.

If something is wrong
- Empty window on Windows: install Microsoft Edge WebView2, then reopen the plugin.
- No sound: Mix, Bypass, and SAFE. If SAFE, lower OS or simplify, then keep playing — it retries.
- Sidechain silent: enable the extra input in the DAW, then cable IN **sc**.
- Preset name has a star: you have unsaved edits. Save As... before you load another factory row.
- Help search: type a word (license, save as, multiband, pitch, DEMO) to filter this guide.

Need a human: neuroklast.net
