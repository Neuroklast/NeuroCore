# NEUROKORE

NEUROKORE is an insert or send effect by Neuroklast. Load a factory sound, turn six knobs, or build your own chain on the board. Version **0.6.1-beta**. Formats: Standalone, VST3, and on Mac an Audio Unit.

This file is the operator guide. The same text is what Help shows inside the plugin.

Put NEUROKORE on a track, or on a send return. Three views share one sound:

- **Unit** — meters, Mix, knobs, and the live path. Play here.
- **Circuit** — the board. Blocks, jacks, cables.
- **Terminal** — the same sound as text. Edit, then Save. The editor is inside the plugin; it does not load from the internet.

Click the wordmark to open neuroklast.net in your browser. There is no address bar inside the plugin.

---

## Install

Windows
1. Run `NEUROKORE-0.6.1-beta-Setup.exe` as administrator. Choose English or German. Pick **VST3** (every 64-bit DAW) and/or **Standalone**.
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

L / BOTH / R chooses which input side the sound hears. BOTH is the default. A dead host side under BOTH is filled before the chain so a mono guitar still hits left and right stages. Inside the chain, L and R stay as they are — a hard pan stays a hard pan on the chip lamps.

---

## Playing

Top bar
- Preset name — click for the library. Untitled if nothing is loaded. `<` / `>` step through factory, then your sounds.
- Functions — look up formula words and insert them.
- Stages — blocks in the current sound. Select an IR block to open that cabinet slot.
- Settings — motion, Live / Studio, theme (Signal, Gold, Azure, DIGICIDE), frame rate (30 or 60), unsaved prompt, cables, tempo, standalone audio device, About, License, Help. These apply to every instance on this machine, including inserts whose window is closed, and are kept after you close the DAW. Resize the window by dragging the frame. DIGICIDE is the industrial steel look; Unit then shows the Digicide mask instead of the Neurokore mark.
- LIVE / STUDIO — Live keeps delay low so playing feels immediate. Studio uses linear-phase oversampling for mix and master work (more delay; the host shows it as latency). Same switch lives in Settings. Changing it on one insert updates every other NEUROKORE on this machine.
- Bypass — Mix to 0 (dry) and locks the Mix slider. Oversampling and the output clipper stop; delay stays the same as the host latency so the track does not jump. Turn Bypass off and the previous Mix comes back.
- Help — this guide.

Knobs
- Six knobs a–f, automatable from the host. On Circuit they sit under the board. On Terminal they sit on the left.
- Click the name to rename it. Scroll the wheel to turn. Type a number or a note length (`1/4`) into the value.
- Right-click a knob: Name / Min / Max / Unit / Note / MIDI Learn. Not the browser menu.
- If a knob does nothing, that letter is not used in the current sound.
- On Circuit, drag the jack on top of a knob onto a chip. The chip fills with drop boxes for every bindable key. The crosshair is the drop point; the box under it glows. That is not an audio cable.

Oversampling (OS)
- 1×, 2×, 4×, 8×. Higher OS is cleaner on heavy drive, and costs CPU and delay.
- Pick OS on the Unit face. Studio vs Live is how that OS is built, not a second OS menu.

Unit meters
- Right-click the floor plot: Input / Output / Both, Samples / Time / Frequency, Linear / Decibel, grid, invert Y, delta.
- Frequency is logarithmic from 20 Hz to Nyquist. The 20 / 50 / 100 / 1k / 10k ticks sit on that map. Height on Frequency is dB (−72 to 0).
- The stacked bars next to the goniometer are peak and RMS in dB (−60 to 0 dBFS), not LUFS. The HUD RMS line is the same: dBFS.

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
- Drag a chip by its body. Contacts sit on the left and right frame, equally spaced. Drag from an output (right) to an input (left).
- While you drag, the cable is a free curve and snaps to a contact from 30 px away. After you drop, the board etches a PCB path (right angles, 45° corners). Alt+click a contact to cut. Dropping on a busy input replaces the old cable.
- A stereo run is two parallel packet lanes (left cyan, right red) until a split block. After Mid/Side split the mid lane stays the main stroke and the side lane peels off at 45°. Packet spacing and speed follow that channel’s RMS; brightness follows the peak. Below −60 dB the stream freezes. A clip (above 0 dBFS) flashes the tube and splits it red/cyan. The peak lamp in the title stays dark until that chip clips, then it glows red. A yellow outlined warning triangle (with a bang) and a dB reading sit just outside the output that actually clips. Send and bus tubes follow the input, because those chips are taps — they have no own meter.
- Right-click empty board to add a block (parked, no cable). Right-click a cable or click it to insert a block on that run. Right-click a chip for Insert after / Inspect / Delete. Delete or Backspace removes the selected chip. Mute and solo stay on the chip face.
- Circuit titles are not selectable text. Search and inspect fields still are.

IN and OUT
- **IN** is locked on the left. Two outputs on the right, stacked in the middle of the tile: **out** is the track, **sc** is the host sidechain. They sit under the title stripe and above the footer. There is no input jack on IN. `sc` is an output so you can feed a compressor, gate, or vocoder. The lamp next to IN is on when the host sidechain pin is enabled. If the pin is off, `sc` is silent.
- **OUT** is the mix back to the host. Named buses (a Send row) land on their own west mix jacks — `main`, then each bus — not a leftover `in`. Expand it (chevron or right-click Details) for **gain** (−24 to +12 dB). Gain is not a slider on the closed tile.

Blocks
- A closed chip is a lamp to the left of the name, then a type code, mute/solo. Contacts are labelled `[ IN ]` `[ OUT ]` `[ MID ]` `[ SIDE ]`. The footer line is the instance id and live peak, not a fake serial — it sits above the south parameter sockets. The chevron opens Inspect (every parameter). Dragging a knob over the chip opens the bind list; south labels hide while that list is open.
- Envelope: the lamp fills with the live level. Open it to pick **source** (`in` or `sidechain`) and **unit** (`lin` 0–1, or `db` for a compressor built from env + a formula stage).
- Phaser is a real allpass cascade (rate, depth, center, feedback, mix, stages). Flanger is a short delay comb; invert is through-zero. Filter **allpass** is one phaser stage if you want to stack them yourself.
- Loading a preset closes open details and fits the whole chain above the mix bar.

Arrange and Compact
- Right-click empty board → Arrange (one row, left to right) or Compact (wrap to the window, still left to right — never a snake). The wrap cable runs between the rows, not around the whole board. Compact keeps a Send bus on its own row under the main chain and parks OUT at the right. Ctrl+A Arrange, Shift+A Compact. Loading a preset arranges, and wraps only if the chain is wider than the window. Dragging a chip stops auto-layout until you Arrange/Compact again.

Expand a chip for hidden parameters. The chip size does not jump.

---

## Terminal

The text is the sound.

- Live view follows the knobs.
- **Edit** opens the editor. Suggestions stay closed until Ctrl+Space (Cmd+Space on Mac).
- **Save** applies the text. If it does not parse, you stay on the last good sound.
- **Validate** lists checks without applying.

The code editor is packaged in the plug-in. Opening Terminal does not need a network connection.

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
- Cubase hangs while scanning plug-ins: you have an old 0.6.1. This build does not start the web view during scan — copy only `NEUROKORE-0.6.1-beta.vst3` into Common Files, not the whole artefact folder, then rescan.
- Terminal stuck on Loading: same, old build. This one uses the packaged editor.
- No sound: Mix, Bypass, and SAFE. If SAFE, lower OS or simplify, then keep playing — it retries.
- Sidechain silent: enable the extra input in the DAW, then cable IN **sc**.
- Preset name has a star: you have unsaved edits. Save As... before you load another factory row.
- Help search: type a word (license, save as, multiband, pitch, DEMO) to filter this guide.

Need a human: neuroklast.net
