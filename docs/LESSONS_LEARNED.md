# Lessons (rules only)

Session diary: `docs/archive/LESSONS_SESSION_LOG.md`. Add a rule here only if it is **new**.

## Process

- One logical change. A screenshot bug is not a license to also rewrite CPU, Settings, and the pack script.
- Never run `scripts/generate_factory_presets.mjs` against the shipping catalog. It rebuilds from the generator and drops curated Vocals/legacy. Append to `resources/factory_presets.json` and embed. The generator must refuse to write if it would drop existing names.
- Do not tick DEVELOPMENT_STATUS against a screenshot that still fails.
- `umsetzen` means code. The tester PDF is a source, not a deliverable.
- **Library first.** Before writing canvas geometry (grid, snap, edge path, drag line, layout), open the React Flow docs and the `@xyflow/react` exports already in the file. Use what it ships. Invent only the constraint RF cannot express. If you cannot name the RF API you rejected and the contract it failed, you do not get a new helper.
- A plugin build that skips `npm run build` ships yesterday’s UI. `NeuroKoreWeb` is a hard dependency of `NeuroKore` / VST3 / Standalone. Missing `npm` is a configure error. Do not add an optional web target again.

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

- Bare Space is Cubase play/pause even while the plugin window is focused. WebView2 eats WM_KEYDOWN; JS must not keep the key (except in a text field) and native must PostMessage it to the host HWND. Do not consume Space in `keyPressed`.
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
- After a trip, do not decay the wet EMA to 0 (fake recovery). Do not leave 1.73 on the meter during SAFE-hold (`noteHoldDisplay`).
- LFO Hz is not audio rate. Filter coeffs: stride + smoother, not `setCutoff` every sample at 8×.
- `evaluate()` (lock) is not an inner-loop call. Use `evaluateLive`.
- CPU-hold / lock-miss: last wet or silence, never raw input.

## Factory / sound

- Clip chains end with an LPF. Predelay is a wet-bus delay at mix 1, not a slap in series.
- Phaser: HP stays below LP. Env output is 0–1.
