# Lessons (rules only)

Session diary: `docs/archive/LESSONS_SESSION_LOG.md`. Add a rule here only if it is **new**.

## DSP

- Peak safety is the engine True-Peak brickwall. Residual mute is `OutputSanitizer` only. Do not re-split those two into Polisher vs sanitizer modes.
- Silent-side seed for a mono DI is `InputRouter` (plugin input, BOTH). `SignalChain` must not copy L↔R — chip taps and a hard pan stay honest.
- Node taps are slots bound in `loadScript`. Never construct `juce::String` to find a tap on the audio thread. Overflow must not land on slot 0 (`__in__`). Wave/peak/rms come from the 64-sample viz tap, not a second OS-rate scan.

## Process

- One logical change. A screenshot bug is not a license to also rewrite CPU, Settings, and the pack script.
- Never run `scripts/generate_factory_presets.mjs` against the shipping catalog. It rebuilds from the generator and drops curated Vocals/legacy. Append to `resources/factory_presets.json` and embed. The generator must refuse to write if it would drop existing names.
- Do not tick DEVELOPMENT_STATUS against a screenshot that still fails.
- `umsetzen` means code. The tester PDF is a source, not a deliverable.
- **Library first.** Before writing canvas geometry (grid, snap, edge path, drag line, layout), open the React Flow docs and the `@xyflow/react` exports already in the file. Use what it ships. Invent only the constraint RF cannot express. If you cannot name the RF API you rejected and the contract it failed, you do not get a new helper.
- A plugin build that skips `npm run build` ships yesterday’s UI. `NeuroKoreWeb` is a hard dependency of `NeuroKore` / VST3 / Standalone. Missing `npm` is a configure error. Do not add an optional web target again.
- The build is CMake (`NeuroKore_All`). Do not restore Projucer `Builds/`, `JuceLibraryCode/`, or `NeuroCore.jucer`. CMake generates BinaryData; native knob PNGs and melatonin_inspector are not part of the editor.
- Testers get a single `.vst3` and `.exe`. The editor must live inside those files (`NEUROKORE_WEB_DIST` zip resource), not in a sibling `web/` folder. Copying `web/dist` next to the artefact is dev-only.
- Windows `.rc` string names in quotes keep the quote characters. FindResource then misses. Use an integer ID (`41001 RCDATA`), not `"NEUROKORE_WEB_DIST"`.

## Circuit

- Circuit is headless (`boardStore` + DOM chips + one cable canvas). Do not wire AssembleView back to React Flow. Layout adapters use local `flowTypes`, not `@xyflow/react`. Camera pan/zoom writes the CSS matrix and canvas `camRef` in the pointer event; `setCamera` is pointerup / fit, never every move.
- ELK (layered RIGHT) places chips in a worker for Arrange. Compact wraps a serial chain to the window (never a snake) and packs named send/bus rails as their own rows. Row gap is WRAP_AIR. A* writes the stored SVG `d`. The cable canvas paints that string.
- HVH is the blocked-channel fallback only. A* is the live router. Corners are 45° chamfers of one cell, not long diagonals.
- Pins on the 16 px grid. No `dropMicroJogs`, no 2 px jack inset.
- IN top-left, OUT bottom-right. Wrap the **chain**, never fold OUT to x = margin.
- Knob letters are labels on the chip. They are not nets.
- Drag = free line. PCB only after drop.
- Auto-arrange runs on first paint / new preset, never after a canvas AST echo. User xy stays until Arrange.
- Do not mark the board as seen until arrange has landed. Strict Mode re-runs the paint effect on the same ref.
- Fit snap floors. Rounding up makes the 1280×860 canvas bigger than the host window and clips the chrome.
- Chip height = ChipSpec.minBodyPx (header + every param socket + south jack band). Expand hides the socket list only; leftover space stays empty.
- MS chips do not have a serial `out`/`in`. Mid/Side jacks without `channel=` in the document are scenery. Circuit must write `channel=`. An untagged chip between encode/decode sits on both rails (DSP: L=mid, R=side). An edge whose handle is not in `visualJacksFor` is not drawn — React Flow returns null, not a tube.
- Split/join is one fork: 1-in/2-out or 2-in/1-out. Family is MS (`mid`/`side`) or LR (`left`/`right`). `encode`=`split`, `decode`=`join`. Never patch MS rails to L/R rails.
- A Bus chip is `bus <name>:` and following audio sits on that rail. Join Signal (`inA`/`inB`/`out`, mix default 0.5) is the mixer. Emit that as `joinN: mix = …`, never also as `out: main=…; dirt=…` for the same mix.
- Send is a tap, not a DSP source. Cables are IN → bus → send → chain. Never send back into the bus header. OUT mix-ins are `main` plus each named bus, never a leftover `in`. Compact packs each rail as its own row; do not DFS-snake a send graph. Plasma on send/bus follows IN — those chips have no tap.
- User chip xy is owned by the board until Arrange or a new graph. A host echo is not a second owner.
- Closed DSP plate is identity + one lamp. No TRG/barcode/grip/fake jacks. Expand overlay hides the south bind rail.
- Compact wrap: `finishHalo` makes the cell around a chip solid. CHIP_AIR_Y (one cell) is not a cable. Row gap is WRAP_AIR = pad + rail + pad. Wrap edges ride `wrapRailRoute` on that rail, never A* around the hull.
- Circuit selection is `data-selected="on"` on `.nk-board-chip`. Delete/Backspace is plugin-owned only while that is set. Do not look for React Flow `.selected`.
- Stereo bus is one stored route, painted as two packet lanes ±4 px on the normal. Do not A* twice. The host tap is `{ rmsL, rmsR, peakL, peakR }`. RMS fills and drives the packets; peak flashes them; peak > 1.0 is the glitch. Split chips have no combined `out`. Mid/Side is not L/R: Mid keeps the main stroke, Side breaks 45° with a short dash. Port labels are `[ MID ]`. Microtext is `node.id` / typecode / peak dB — never a fake UUID.
- Side jacks use fork pitch only when `(n-1)*FORK` fits **under the title and above the foot**. On a short IO tile that is one-cell pitch around the body midline. Do not park IN `out` on the hazard band or `sc` on the footer.
- Chip fill is black. The foot (`node.id` + dB) sits on `--nk-south-gap`, in the pad field, never on the south bind sockets. The peak lamp leads the title (`CHIP_PAD_X` gap), it is not a trailing pip under the chevron.
- Circuit paint uses chip/grid constants (`TITLE_H`, `LABEL_COL`, `BOARD_BLOCK`). No magic px, no graph-paper lattice, no cell-X copper, no idle packet crawl without a tap. Packets are ink cores with cyan/accent glow. Hazard stripes live on a title-band `::before` (clip-path only there). Do not `overflow:hidden` the headband — that eats the peak lamp and its glow. The expand plate owns `DETAIL_HIT` at the top-right; `headbandEndPad` keeps the lamp out of that slot. Clip warn is an outlined Δ + bang at the east jack, never a filled pip in the header. The pad array is `chipPadInset`, not a fill texture.
- Circuit is not a text document: `user-select: none` on `.nk-board`, except input/textarea. Knob bind is chrome jack → south `data-bind-key`, not a graph cable.
- Chevron inspects. Knob-drag over a DSP chip fills that chip with `bindPadCells` (inside the AABB). Ghost crosshair sits on the cursor (hot = 0,0); the letter is a label above it. The pad under the cursor glows.
- IO tiles do not use the DSP label-column gutter. That gutter is 44 px each side and clips "OUT" to "O._".
- Side jacks are local centres (`sidePortLocals`). Global = node origin + local. Paint (RF Handle `top` = centre + translate -50%) and A* read the same vector. Never derive a port from the DOM. Equal pitch, inside the AABB.
- `fitView` waits until the pane has a box (Circuit is `hidden` on Unit, not unmounted) and the laid-out node ids are on the board. React Flow `minZoom` and `fitView` minZoom are the same constant. Do not invent a second zoom floor to hide a missed fit.

- Unit sample/time traces use the spectrogram camera: mag 0 is the 3D floor (−1), mag 1 is +peak. Do not run a second signed `waveProject` — that punches −1 through the floor. Paint the live buffer. A trigger lock (or holding the last row) freezes the wave and looks laggy; a first-crossing hunt flickers.

## Chrome / knobs

- WebView2 eats OS key events before JUCE sees them. Non-text keys must be forwarded from JS (`shouldForwardToHost` → `hostKey`) and posted to the DAW HWND (`PostMessage` on Windows; `CGEventPost` on Mac VST3/AU). Do not send `juce::KeyPress` to the top-level component as the Windows path. Text fields, plugin chords, and overlays stay in the plugin.
- Cubase VST3 scan of a **new path** calls `createView` and often `attached()` with a hidden HWND, then `removed()`, all on one stack. Cached plugins skip instantiate — that is why 0.5.1 “just worked” later. Chromium is a backend, not IPlugView: never realize it on the host callback. Post to the message thread; `shouldRealizeChromium` is false if detach already ran. `prepareToPlay` or a 250 ms timer is not the model.
- Monaco is a Terminal chunk. App boot must not import `monacoEnv` or `HackView`. `@monaco-editor/react` defaults to jsDelivr; `pageAboutToLoad` blocks that, so Terminal sits on Loading forever unless `loader.config({ monaco })` runs from the bundled editor.
- JUCE destroys the `IPlugView` / `AudioProcessorEditor` on close. Keep the `WebBrowserComponent` on the processor. Editor close is `removeChild` / native unparent, never `delete` the browser. WebView2 is bound to the peer HWND at create time — park that HWND, do not let the editor peer die while it is still the parent.
- VST3 `removed()` calls `removeFromDesktop()` → `DestroyWindow(plugin HWND)` **before** `~Editor`. `removeFromDesktop` does not notify children first. Never make the WebView HWND a `WS_CHILD` of IPlugView; sibling of the plugin HWND (host parent) or a processor-owned window. `~WebPluginEditor` is too late to `SetParent` away from a child.
- Standalone has no IPlugView. `editor.getPeer()` is the app window (`GetParent` is null). That HWND is the park parent. Parenting to the hidden owner HWND hides the WebView for the whole session. The VST3 “never child of the editor HWND” rule does not apply when the editor peer **is** the top-level window.
- Host bypass is `processBlockBypassed`. JUCE’s default does not delay. If `getLatencySamples() > 0`, bypassed dry must use the same PDC delay as mix 0 or the track jumps. OS, True-Peak GR, soft clip, and dither do not run; the limiter delay line still advances.
- Two VST3 instances in one host process must not share a WebView2 user-data folder. One profile for all instances serializes Chromium and stutters both UIs. That folder is also `localStorage`: never store machine-wide Settings there. Theme/motion/fps live in `UiSettings` (`%AppData%/NEUROKLAST/NeuroKore/ui.settings`) and ride the host snapshot. Mix, OS, knobs, and the formula stay host state per insert. `emitEventIfBrowserIsVisible` is a no-op when `Component::isVisible` is false — do not hide the parked WebView if shared prefs must reach it. Another process writes the same file: reload on change, do not keep a stale singleton.
- A preset load that starts ELK/A* must not apply that result after a newer hydrate. Stamp `layoutEpoch` on replace; drop stale `applyLayout`. Host echo of the same chip ids keeps live xy. Stored PCB that does not meet the current jacks is empty space — paint a stub, do not keep the old polyline.
- Cubase track change / ASIO Guard calls `setNonRealtime` and VST3 `setProcessing`. Half-band OS, sanitation, and delay rings must `reset()` on that flag change (same as `PluginProcessor::reset`, including the 256-sample fade-in). Do not leave filter memory from the previous realtime/lookahead path.
- ENV is a follower. LFO lamp/chase is only for `osc`. Do not feed env cables a fake 1 Hz `freq`.
- ENV is a bus tap (audio `in`, mod `out`). LFO has no audio in. `connectAudio` must not treat env as osc. Draw IN/prev → env.in; env is never a through node.
- Osc writes a node tap; env must too. Circuit glow is that tap (`host.mods`), not a fake 1 Hz chase. "follow" with no level is a dead cable.
- Do not `processBlock` the same buffer N times when a stage writes `y = env`. That is env-follows-itself and settles at 0.5. Copy the dry stimulus each callback, like the host does.
- Preset arrows walk the selected explorer folder, then the next folder. They do not walk JSON order.
- Octaver `up = 0` must not still run the +1 rectifier. Use LookupTables for sin/tanh on that path.
- Audio thread never looks up `variables[juce::String]`. Bind knob/env/osc slots at load. `hardclip`/`fold` are not ADAA — do not force the sample loop.
- Knob readout is `round2` (+ unit / `%`×100). Never round SignalChain / audio-thread samples to match the label.
- Bind from an inactive knob activates it with `chipSpec.ranges` / `enums`. Enum binds are N detents + N ticks, not a continuous arc.
- IR WAV is host state, never the formula. An overlay with Load is not a feature if nothing opens it. Circuit chip, Inspect, Stages, and Terminal must share one Impulse entry.

## DSP / CPU

- Footer CPU is 0–100. Host-callback ratio > 1 is not a percent of the machine.
- Cubase ASIO Guard can deliver host `n` > `prepareToPlay` `samplesPerBlock`. Never `setSize` / `new` / `juce::String` copy on the audio thread; slice at the prepared host ceiling before OS.
- After a trip, do not decay the wet EMA to 0 (fake recovery). Do not leave 1.73 on the meter during SAFE-hold (`noteHoldDisplay`).
- LFO Hz is not audio rate. Filter coeffs: stride + smoother, not `setCutoff` every sample at 8×.
- `evaluate()` (lock) is not an inner-loop call. Use `evaluateLive`.
- CPU-hold / lock-miss: last wet or silence, never raw input.
- Live formula IR is `ExprTape`. JIT (asmjit) only after the tape exists, only for Load/Add/Sub/Mul/Neg, only Windows x64. Call/ADAA `invoke` crashed factory load — keep the interpreter.
- SIMD for tape is `exprTapeEvalSimd`, not the AST `evalSimd` tree. ADAA is sequential — `softclip`/`tube`/`diode` stay on the sample loop. `hasLiveTape()` must not force scalar for `y = x * a`.
- JIT Call/ADAA is `call` into `exprTapeDiv` / `exprTapeCallAdaa` and friends. asmjit Invoke into C++ shapers crashed factory load. Failed emit leaves `fn` null.
- `display: none` does not stop rAF or React. Unmount Unit analyzers. One viz clock; last subscriber cancels the frame.
- Do not `xorps` against a 4-byte const pool entry. SSE reads 16 bytes. Negate with `mulss -1`.
- Do not `JitRuntime::release` / destroy delay rings when `processBlock`'s local `shared_ptr` dies. Retire the old chain on the next `loadScript` (message thread).
- JUCE half-band FIR already strides `k += 2`. Do not dual-own OS next to sanitation AA.
- No global `/fp:fast`. It breaks `isfinite`, sanitation NaN-hold, and ADAA.
- `alignedRing` padding must not change wrap length. A 10 ms delay impulse stays near 480 samples @ 48 kHz.
- macOS VST3/AU: tape only (`NK_HAS_EXPR_JIT=0`). No asmjit, no WebView2 defines, web zip in `Contents/Resources`.

## Factory / sound

- Clip chains end with an LPF. Predelay is a wet-bus delay at mix 1, not a slap in series.
- Phaser is an allpass cascade (`phaser` or `filter type = allpass`), not HP+LP. Env is 0–1 unless `unit = db`.
