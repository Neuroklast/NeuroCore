# NEUROKORE User Manual

This is the same text the plugin shows under **Help** (`resources/UserManual_en.txt`). It is written for people using the plugin, not for building it. Engine and DSL details live in `docs/DSL_REFERENCE.md` and `docs/ARCHITECTURE.md`.

---

# NEUROKORE

NEUROKORE is an insert or send effect by Neuroklast. Load a factory sound, turn knobs a–f, or write a short formula (drive, filter, delay, reverb, compressor, gate, limiter, IR cab, crossover, envelope, oscillator).

30-second path

1. Click **Presets** and double-click a factory row (amp, delay, reverb, vocal).
2. Play audio. Raise **Mix** until you hear the effect.
3. Turn knobs **a–f**. Names and ranges come from the formula.
4. Want your own sound? Click **Edit**, change the formula, then **Save**.

This Help is offline. Pick a chapter on the left. Search filters titles and text.

---

## 1. Quickstart

Put NEUROKORE on a track, or on a send return. On Mac it is an Audio Unit and a VST3; on Windows and Linux it is VST3 or the Standalone app.

### Load a preset

- Click **Presets** in the top bar.
- Leave Scope on All or Factory.
- Double-click a row, or select it and press **Load**.
- The chip next to Presets shows the name. Use the **<** and **>** buttons on either side to step through the library.

### Hear it

- Mix at 0% is dry (untouched input).
- Raise Mix. On an insert, start around 40–70%. On a send, push Mix high and use the host send level.
- **L / BOTH / R** chooses which input channel the formula hears. BOTH is the default.

### Change the sound

- Knobs **a–f** can be automated from the host.
- If a knob does nothing, that letter is not used in the current formula.

### Edit the formula

- **Edit** opens the formula editor. Suggestions stay closed until you press **Ctrl+Space** (Cmd+Space on Mac).
- **Save** applies the formula. A red line under the editor explains errors.
- **Copy** puts the formula on the clipboard.

### Save your own

- Presets → **Save As…**
- Give it a Name, Author, and Category.

---

## 2. Main UI map

### Top bar

- **NK + NEUROKORE + by Neuroklast + version**: product mark. Click it to open neuroklast.net.
- **Presets**: factory and your own sounds. The last category stays selected when you reopen.
- **Current chip**: loaded preset name, or Untitled. **<** / **>** step through factory, then user presets.
- **Functions**: look up formula words and insert them. Folders on the left: **Core** (math), **Drive** (tube, diode, clip), **Crush** (fold, bitcrush), **Blocks** (ott, widen, vocoder, octaver, ms).
- **Stages**: the blocks in the current formula, and which knobs they use. Select an IR block to open that cab slot.
- **Settings**: animation (**Full** / **Reduced** / **Off**), **Studio / Live** processing, UI scale (100 / 125 / 150), formula text size, and standalone audio device. Remembered.
- **LIVE / STUDIO**: Live uses min-phase oversampling so the delay is not noticeable when you play. Studio uses linear-phase oversampling for mix work. Same control lives in Settings.
- **Bypass**: forces Mix to 0 (dry) and locks the Mix slider. Turn it off and the previous Mix comes back.
- **License**: import a signed `.lic` file. After activation, License shows who this copy is licensed to. **Replace license** loads a new file.
- **Help**: this guide, one chapter at a time. Text is large so you can read it from the chair.

### Settings

Oversampling, Polisher, and Mix sit **between the Circuit/Terminal and the IN/OUT scopes**, not in the top bar.

- **L / BOTH / R**: which input channel feeds the formula. Same width as the knobs, on the Circuit / Terminal row.
- **Oversampling**: 1× / 2× / 4× / 8×. Default **4×**. Drop to 2× or 1× if the computer is struggling.
- **Polisher**: None / Hard Clip / Limiter after the formula. Use Limiter if peaks slam.
- **Settings → Tempo**: **Host** follows the DAW BPM. **User** lets you type a BPM. Note-length knobs and delay sync use that value. The footer shows `BPM 120 HOST` or `BPM 128 USER`.
- **Settings → Circuit cables**: **Dots** (white/gray traces, beads only while audio is present) or **Wave** (same traces, gray post-block waveform).
- **Guitar / BOTH**: a silent stereo side is copied from the live side so L/R splits (Stereo Guitar Wall) work from a mono DI. Amp presets have a **Width** knob (`widen`) defaulting to 0.
- **Text − / +**: size of the formula text (also in **Settings**).
- **Settings → scale**: 100 / 125 / 150. You can still resize the window; the aspect ratio stays locked.
- **Settings → animation**: Full (boot, glitch, light CRT glow), Reduced (overlays snap), Off (still chrome).

### Knobs

- Six knobs in one column: a, b, c, d, e, f from top to bottom.
- Click the name above a knob to rename it (Circuit and Terminal). That also updates `param a = …` in the script. Edit the name in the script and the knob label follows. The formula still uses `a`–`f`.
- Right-click a knob for MIDI Learn.

### Formula area

- **L / Both / R**: input channel, same width as the knobs, on the Circuit / Terminal row.
- **Circuit / Terminal**: two views of the same construct.
  - **Circuit** = assemble. The surface is a board: faint rose crosses, 16 px snap. Blocks are chips. Drag moves X and Y only — it does not change the signal order. Double-click or right-click a chip to open the node overlay (every jack field, knob bind, Remove). Click the arrow to fold/unfold. Right-click empty paper: **Auto-arrange** (fit chips to the current zoom, no rewire) or **Add** with category folders (Tone, Drive, Dynamics, Space, Routing, Stereo, Pitch, Modulation, Measure). Factory presets open already auto-arranged for the current zoom. Routing includes Sidechain (host extra input). Hover a knob to see its traces. Bound knobs print the live value in red on the chip. Ctrl+wheel zooms. Audio cables are orthogonal PCB traces (rounded corners, parallel lanes when tracks share a run); beads only while loudness is up.
  - **Terminal** = hack. The same chain as text. Live view shows coloured knobs and current values (`a[3.20]`). IR slots have an inline button on that line. **Edit** opens the editor; **Save** applies it. Positions are `# @x,y` comments.
- Each block shows the jacks it needs: audio in/out, a mix jack per bus on OUT, A–F for bound knobs, SC on comp/gate/vocoder, MOD on an LFO. Double-click a card to edit. Click a knob name to rename it. Ctrl+Z / Ctrl+Y undo and redo.
- Each `ir1` / `ir2` line has an inline button on that line. Click it to drop, load, change, or clear that impulse. Amp factory presets come with a cabinet already loaded; you can still swap or clear it. Empty slot is dry.
- **Edit**, then **Save** to apply.
- **Optimize**: tidy the math without changing the idea. It will refuse a rewrite that sounds worse.
- **Insert / Quick template**: drop in a common block.

### Bottom

- **Tools row** (between Circuit/Terminal and the scopes): Oversampling, Polisher, Mix. Mix drag may flash only inside the slider.
- Audio device lives in **Settings** (Standalone only). In a DAW the host owns the sample rate.
- **Footer**: NKOS LIVE / STUDIO / BYPASS / SAFE, CPU, latency, sample rate, 32f, buffer, BPM HOST or USER, OS.
- **IN / OUT**: wave, stereo field, and loudness stay open. Click **<<** only if you want the wave alone.
- **Meter**: the tall level meter on the right, plus a limiter cue.

---

## 3. Knobs a–f

There are six knobs: **a, b, c, d, e, f**.

Declare them in the formula:

```text
param a = Drive [0.5, 6.0]
param b = Tone [800, 9000]
param c = Time [1/1, 1/16]
stage1: y = softclip(x, a)
filter1: type = lowpass; cutoff = b; resonance = 0.3
delay1: time = c
```

### How they work

- The host sees each knob as 0–1.
- `param a = Name [min, max]` turns that 0–1 into the range you wrote.
- `param c = Time [1/1, 1/16]` is musical note lengths. The knob snaps to 1/1, 1/2, 1/4, 1/8, 1/16 and the dotted/triplet steps in between. In the formula `c` is milliseconds at the host tempo, so `delay1: time = c` works.
- Use the letter in the formula (`softclip(x, a)`). If you never mention `a`, the knob does nothing.

### Watch out

- A lone `e` is knob **e**, not the number 2.718. Write `exp(1)` for that.
- Renaming the chip under a knob does not rename it in the formula.

---

## 4. Formula editor

1. Click **Edit**. The live view becomes an editor.
2. Write stages, filters, delay, reverb, compressors, envelopes, oscillators.
3. Ctrl/Cmd+Space opens suggestions. After `type =` on a filter you only get filter types.
4. Click **Save**. NEUROKORE checks the formula and starts using it.
5. If Save fails, read the red line under the editor.

### Other buttons

- **Copy**: clipboard only, does not apply.
- **Optimize**: safer, simpler math. Watch the overlay for status.
- **Insert / Quick template**: delay, reverb, clip plus a darkening filter, and similar blocks.

### Live view

- Colours match the knob rings.
- Values in `[brackets]` update while you play.
- You can scroll even when you are not editing.

**Undo:** Ctrl/Cmd+Z and Shift+Ctrl/Cmd+Z walk formula history.

---

## 5. Presets

### Factory

- Built into the plugin. Author is **NEUROKLAST**.
- The folder you last picked stays selected when you reopen Presets.

### The Presets window is an explorer

- Left: folders (All, Distortion, Club, Delay, …) with a count.
- Top: search, plus All / Factory / User.
- Middle: the list. Bottom: name, tags, and what the sound does.
- The number at the top right is how many rows match.

### Load

- Double-click a row, or select + **Load**.
- Or stay in the editor: **<** / **>** beside the preset name steps through factory, then user presets. Wraps at both ends.
- **New Blank** starts a simple softclip formula.

### Save your own

1. Open **Presets**.
2. **Save As…**
3. Name, Author (your name or alias), Category (Guitar, Delay, Vocal, Club, …).
   Optional tags (comma-separated) make the sound easier to find later.

The list shows **Name / Category / Tags / Source / Author / Rating**.
It opens sorted by **Name**. Click a header to change the sort.
Search looks at the name, tags, description, and the formula itself.
Type `delay`, `kick`, `techno`, `cyberpunk`, or `tape` to find matching sounds.

There is no separate builder panel. Write formulas in the editor (templates + Edit).

Share packs by exporting a `.zip` (visible user presets) or by sending a folder of `.nrk` files. Import accepts one `.nrk`, several files, a folder, or a `.zip`. Packs land under your user library in `Packs / pack name` and never overwrite factory sounds.

### Useful factory sounds

The factory library is 500+ studio jobs across 22 folders. Search or tags are the way in. These are mix tools, not a full mastering suite.

- **Side Delay / Side Hall**: delay or reverb on the sides only. The center stays dry.
- **Mono Below**: lows become mono; the top stays wide.
- **MS Imager / MS Mix Desk**: width, cleaner sides, a bit of mid glue.
- **Vocal Send**: dry vocal plus slap and a small room.
- **NY Drum Bus / Parallel Tape**: dry plus a smashed or tape-like path. Blend is the mix.
- **Plate Send / Width Delay / Slap Double**: dry in the middle, wet on a side path.
- **Haas Width / Loudness Curve / Missing Bass / Speech Band**: ear tricks (width, cut-through, implied bass). Not a hearing-lab suite.
- **Mono to Stereo**: allpass + Haas stereoizer. Mid stays the original so the mix still collapses clean.
- **Trailer Impact / Score Hall / Dialogue Seat / Far Plane / Boom Tail / Wide Canvas / Tension Bed**: score, FX, and dialogue processing. Not a trailer sample pack.
- **Stereo Guitar Wall**: two DI takes, two amps (Mesa left / 5150 right), noise gate, cabinet IR preloaded. Needs stereo in and **BOTH**. Swap the cab on the `ir1` button if you want.
- **Glitch Laboratory**: digital smash without ping-pong or an LFO on the filter — light on the computer.
- **Neon Clip / Chrome Fold / Data Mosher / Cyberpunk Drive**: digital dirt.
- **Kick Rumble / Warehouse Rumble**: split kick. Main = click+mids (HPF, dip at 320 Hz). Scream = bright hit only (follows the envelope). Body = dark floor (Floor knob, ~60 Hz) into a 15 ms resonator + sub. Insert on the kick, Mix 100.
- **Hardcore Clip / Gabber Drive**: same split, more mid bark, 320 Hz scooped so it stays crisp. Acid Hash / Tekno Comb / Industrial Gate / Hoover Dirt: more club tools.
- **Cyberpunk Drive**: guitar-shaped digital dirt (crush + fold + short metal comb). Use this instead of the old quiet Glitch Laboratory.
- **Glitch Laboratory**: louder smash + short ping-pong + Level makeup. Still a glitch toy, not an amp.

---

## 5b. Circuit vs Terminal

- **Circuit** is how you assemble the parts. Snap to 16 px. Drag is placement only. Edit a chip in its overlay.
- **Terminal** is how you hack the construct. Same chain, as text. Live `[value]` next to every knob letter.
- The footer strip shows CPU, latency, sample rate, 32-bit float, buffer, BPM (HOST or USER), and oversampling.
- Settings → Tempo: follow the host BPM or type your own. Note-length knobs and delay sync use that tempo.
- Circuit: Ctrl + mouse wheel zooms the board. Cables show the live waveform after each block. Bound knobs print their live value in red on the chip.

## 6. Formula language

Everyday blocks:

| Block | What it does |
|-------|----------------|
| `param a = Name [min, max]` | Declares knob a and its range |
| `stageN: y = …` | Per-sample math (`x` in, `y` out) |
| `filterN: type = lowpass; cutoff = …` | SVF lowpass / highpass / bandpass |
| `eqN: type = peak; freq = …; q = …; gain = …` | Peak, notch, lowcut, highcut, shelves |
| `octaverN: sub = …; up = …; mix = …; tone = …` | Analog −1 divider / +1 rectifier |
| `vocoderN: bands = 8; mix = …; q = …` | Vocoder. Insert on a synth/pad (carrier). Pin the **voice** on Sidechain. Empty pin self-vocodes. |
| `compN: threshold = …; ratio = …` | Compressor; optional `knee`, `makeup`, `hpf`, `source = sidechain` (no pin = no duck) |
| `gateN: threshold = …; hyst = …` | Noise gate (hysteresis + hold) |
| `limitN: ceiling = …; release = …` | In-chain limiter (not the Polisher) |
| `xoverN: f1 = …; f2 = …` | Crossover into `low` / `mid` / `high`; mix with `out: low = 1; high = 1` |
| `ottN: depth = …; time = …` | 3-band upward + downward compressor (OTT). `in`, `low`, `mid`, `high` optional |
| `widenN: width = …; delay = …; bass = …` | Mono to stereo. Mid stays put, side is allpass + Haas. Bass stays mono. |
| `irN: mix = …` | Cab / room slot. Button on that line: drop WAV/AIFF, Load, waveform, Clear. Several slots. Empty = dry |
| `envN: type = peak; source = sidechain` | Envelope; `source = sidechain` follows the extra input |
| `oscN: type = sine; freq = …` | Slow oscillator / LFO |
| `delayN: time = …; feedback = …; mix = …` | Delay |
| `reverbN: size = …; mix = …` | Room / hall |
| `sc` / `sc_l` / `sc_r` | External Sidechain input in the formula |

### Good habits

- After **hardclip**, fold, or bitcrush, add a **lowpass** around 6–12 kHz so it does not get brittle.
- `softclip` and `tube` are usually smoother than hardclip for amp-like grit.
- Mix **0%** is fully dry. Bypass does the same and remembers your Mix.

---

## 7. Tips

- **Functions catalog**: open **Functions**. Pick a folder, then a word, then **Insert**. Core is `sin` / `lerp` / `map`. Drive is `tube` / `diode` / `softclip`. Blocks is `ott1` / `widen1` / `vocoder1`.
- **Send effect**: load a delay or reverb, set Mix high, ride the host send.
- **Amp-like**: Drive + softclip + lowpass. Leave Polisher on None so hits still breathe.
- **Two guitar DIs**: `Stereo Guitar Wall`, L/BOTH/R on BOTH, hard-pan the two takes into this insert.
- **Hardware comps**: `1176 FET`, `LA-2A Opto`, `SSL Bus Comp` — attack cannot go below 1 ms (engine floor).
- **Note knobs** `[1/1, 1/16]`: delay time is milliseconds. On an oscillator (`freq` or `sync`) it is one cycle per note — `Sidechain Pump` Rate is 1/4, not a 500 Hz scream.
- **Rooms**: `EMT 140 Plate` is bright and short; `Lexicon 480 Hall` is long and even; `AMS RMX Nonlin` is the 80s snare burst.
- **Harsh digital**: raise oversampling to 4×/8×. Add a lowpass after clip or fold.
- **A/B**: Bypass is dry without losing your Mix value.
- **MIDI**: right-click a knob → MIDI Learn.
- **Latency**: status shows how late the plugin is, not the delay-effect time. Flip **LIVE** when you play; stay on **STUDIO** when you mix.
- **Standalone sample rate**: **Settings → Audio device**. In a DAW, follow the host.

---

## 8. Troubleshooting

| Issue | What to try |
|-------|-------------|
| No sound | Mix at 0? Bypass on? Red error under the editor? Input on the empty channel? Try BOTH and raise Mix. |
| Crackles / clicks | Feedback too high. Lower Feedback. Add a lowpass after hardclip. A short click when changing oversampling is the fade-in; it must not keep going. |
| Plugin hitches / **SAFE** in the status bar | Formula briefly overran the audio buffer. Wet pauses, then retries on its own. Lower oversampling if SAFE keeps returning. **BYPASS** always stays dry. |
| Host says the plugin is unstable | Save the project and reload the plugin. Avoid rewriting a huge delay formula while recording. |
| Knob does nothing | That letter is not in the formula. Open **Stages** to see which knobs are live. |
| Preset will not load | Your saved file may be damaged. Save As again. Factory sounds are always available. |
| High CPU | Lower oversampling. Use fewer stages and shorter delays. |

---

## 9. License

Without a license NEUROKORE is a 20-minute demo. After that Mix is forced to 0 (dry). Your formula stays loaded.

1. You receive a `.lic` file for your email.
2. Click **License** in the top bar.
3. Choose the file. Status shows `LIC` and the email.
4. After activation, **License** opens a window: Licensed to, your email, and the issue date if the file has one.
5. **Replace license** in that window loads a different file.

The file is stored under your user AppData (`NEUROKLAST / NeuroKore`). If import fails, the file was edited or is not a NEUROKORE license.

---

## 10. Glossary

| Term | Meaning |
|------|---------|
| **Formula** | The text that describes the sound |
| **Stage** | One line of per-sample math `y = …` |
| **Mix** | Blend of dry input and the processed sound |
| **Polisher** | Optional Hard Clip or Limiter after the formula |
| **Factory preset** | A built-in sound from NEUROKLAST |
| **User preset** | A sound you saved, with Name / Author / Category |
| **Quick template** | A ready snippet inserted into the current formula |
| **Latency (status)** | How late the plugin is, not the audible delay-effect time |
| **LIVE** | Processing on. **BYPASS** = Mix forced dry |
| **Mid / Side** | Center of the stereo image vs left-minus-right width |
| **IR slot** | A cab or room impulse. The file is not written into the formula. Click the `ir` line. |
| **Limit vs Polisher** | `limit1` is a block in the chain. Polisher Limiter is the last safety clip. |

---

## 11. Tutorial: gate then limit

1. Edit:

```text
param a = Thresh [-50, -12]
param b = Ceiling [-3, 0]
gate1: threshold = a; hyst = 6; hold = 0.04; range = -80
limit1: ceiling = b; release = 0.08
```

2. Save. Gate closes the hiss between notes. Limit is the chain ceiling, not the Polisher.

---

## 12. Tutorial: IR cab

1. Edit:

```text
param a = Drive [0.8, 6.0]
param b = Mix [0.4, 1.0]
stage1: y = tube(x, a)
ir1: mix = b; gain = 0
```

2. Save. Click the inline **ir1** button on that line.
3. Amp factory presets (Mesa, 5150, JCM, AC30, Tube Screamer, Fuzz Face, Metal Gate, Stereo Guitar Wall) already load a matching cabinet. Drop a WAV or AIFF to replace it, or click **Load**. **Clear** empties the slot (dry).
4. Need two cabs? Add `ir2: mix = 1` and click its own button.

The formula never contains a file path.

---

## 13. Support

NEUROKORE © NEUROKLAST. All rights reserved.

For support, send the host and version, the operating system, the plugin version, the preset or formula, and the steps to reproduce.
