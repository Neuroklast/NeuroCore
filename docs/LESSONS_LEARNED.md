# Lessons (rules only)

Session diary: `docs/archive/LESSONS_SESSION_LOG.md`. Add a rule here only if it is **new**.

## Process

- One logical change. A screenshot bug is not a license to also rewrite CPU, Settings, and the pack script.
- Do not tick DEVELOPMENT_STATUS against a screenshot that still fails.
- `umsetzen` means code. The tester PDF is a source, not a deliverable.

## Circuit

- HVH: east stub, one vertical, west stub. A* finds scenic routes.
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

- Knob readout is `round2` (+ unit / `%`×100). Never round SignalChain / audio-thread samples to match the label.
- Bind from an inactive knob activates it with `chipSpec.ranges` / `enums`. Enum binds are N detents + N ticks, not a continuous arc.

## DSP / CPU

- Footer CPU is 0–100. Host-callback ratio > 1 is not a percent of the machine.
- After a trip, do not decay the wet EMA to 0 (fake recovery). Do not leave 1.73 on the meter during SAFE-hold (`noteHoldDisplay`).
- LFO Hz is not audio rate. Filter coeffs: stride + smoother, not `setCutoff` every sample at 8×.
- `evaluate()` (lock) is not an inner-loop call. Use `evaluateLive`.
- CPU-hold / lock-miss: last wet or silence, never raw input.

## Factory / sound

- Clip chains end with an LPF. Predelay is a wet-bus delay at mix 1, not a slap in series.
- Phaser: HP stays below LP. Env output is 0–1.
