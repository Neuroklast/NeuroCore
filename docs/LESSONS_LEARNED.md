# Lessons (rules only)

Session diary: `docs/archive/LESSONS_SESSION_LOG.md`. Add a rule here only if it is **new**.

## DSP

- Peak safety is the engine True-Peak brickwall. Residual mute is `OutputSanitizer` only. Do not re-split those two into Polisher vs sanitizer modes.

## Process

- One logical change. A screenshot bug is not a license to also rewrite CPU, Settings, and the pack script.
- Never run `scripts/generate_factory_presets.mjs` against the shipping catalog. It rebuilds from the generator and drops curated Vocals/legacy. Append to `resources/factory_presets.json` and embed. The generator must refuse to write if it would drop existing names.
- Factory inserts: Level/Output default near unity (≥ ~0.85). Guitar/distortion cab LPF ≥ ~6.5 kHz unless the filter *is* the sound (RAT, muff, acid). Interstage `round()` is a DAC step — recovery LPF after, same as bitcrush. Amp stacks stay SIMD (`tube`/`round`/`bitcrush`, no `y_prev`) unless regen is the patch. “A bit of bit” is `round(y*512)/512` and `bitcrush(y, 13)`, not Grid 32 / 8-bit. That is a crush pedal.
- Do not tick DEVELOPMENT_STATUS against a screenshot that still fails.
- `umsetzen` means code. The tester PDF is a source, not a deliverable.
- **Library first.** Before writing canvas geometry (grid, snap, edge path, drag line, layout), open the React Flow docs and the `@xyflow/react` exports already in the file. Use what it ships. Invent only the constraint RF cannot express. If you cannot name the RF API you rejected and the contract it failed, you do not get a new helper.
- A plugin build that skips `npm run build` ships yesterday’s UI. `NeuroKoreWeb` is a hard dependency of `NeuroKore` / VST3 / Standalone. Missing `npm` is a configure error. Do not add an optional web target again.
- The build is CMake (`NeuroKore_All`). Do not restore Projucer `Builds/`, `JuceLibraryCode/`, or `NeuroCore.jucer`. CMake generates BinaryData; native knob PNGs and melatonin_inspector are not part of the editor.
- Testers get a single `.vst3` and `.exe`. The editor must live inside those files (`NEUROKORE_WEB_DIST` zip resource), not in a sibling `web/` folder. Copying `web/dist` next to the artefact is dev-only.
- Windows `.rc` string names in quotes keep the quote characters. FindResource then misses. Use an integer ID (`41001 RCDATA`), not `"NEUROKORE_WEB_DIST"`.

## Circuit

- React Flow owns the canvas chrome: `Background` (Dots / Lines / Cross, `gap`, stack two), `snapToGrid` + `snapGrid`, `Handle` + `Position`, `nodesDraggable`, `onConnect`, `BaseEdge`, `getStraightPath` for audio drag/temp, `getSmoothStepPath` for knob-bind preview. We own only PCB constraints RF does not have **after drop**: east/west stubs, obstacle boxes, parallel rail pitch, equal air to chips. A second drag model, `BoardGrid` SVG, a second router next to `tubePath`, and a drag-preview that PCB-routes while the product rule says “drag = free line” are the failure mode.
- ELK (layered RIGHT) places chips in a worker for Arrange. Compact wraps into 1–3 rows (PCB, not a meter-long sausage) with 32 px air so every tube is at least one grid long. A* writes the stored SVG `d`. React Flow only paints `data.route` (`StaticGridEdge`).
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
- User chip xy is owned by the board until Arrange or a new graph. A host echo is not a second owner.

## Chrome / knobs

- WebView2 eats OS key events before JUCE sees them. Non-text keys must be forwarded from JS (`shouldForwardToHost` → `hostKey`) and posted to the DAW HWND (`PostMessage` on Windows; `CGEventPost` on Mac VST3/AU). Do not send `juce::KeyPress` to the top-level component as the Windows path. Text fields, plugin chords, and overlays stay in the plugin.
- JUCE destroys the `IPlugView` / `AudioProcessorEditor` on close. Keep the `WebBrowserComponent` on the processor. Editor close is `removeChild` / native unparent, never `delete` the browser. WebView2 is bound to the peer HWND at create time — park that HWND, do not let the editor peer die while it is still the parent.
- VST3 `removed()` calls `removeFromDesktop()` → `DestroyWindow(plugin HWND)` **before** `~Editor`. `removeFromDesktop` does not notify children first. Never make the WebView HWND a `WS_CHILD` of IPlugView; sibling of the plugin HWND (host parent) or a processor-owned window. `~WebPluginEditor` is too late to `SetParent` away from a child.
- Standalone has no IPlugView. `editor.getPeer()` is the app window (`GetParent` is null). That HWND is the park parent. Parenting to the hidden owner HWND hides the WebView for the whole session. The VST3 “never child of the editor HWND” rule does not apply when the editor peer **is** the top-level window.
- Host bypass is `processBlockBypassed`. JUCE’s default does not delay. If `getLatencySamples() > 0`, bypassed dry must use the same PDC delay as mix 0 or the track jumps.
- Two VST3 instances in one host process must not share a WebView2 user-data folder. One profile for all instances serializes Chromium and stutters both UIs.
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
- Phaser: HP stays below LP. Env output is 0–1.
