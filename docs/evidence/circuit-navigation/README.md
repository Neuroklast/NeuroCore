# Circuit navigation and signal evidence

Base: merged PR #313, `dd39850`. Version remains 0.7.1-beta.

Large patches use a headless view hierarchy over the unmodified board graph. Overview cards are bounded to six; leaf workspaces to three chips. Boundary buttons show actual cross-scope connections. Region names and cameras are editor-session metadata for up to 16 patch topologies, not persisted preset groups. Arbitrary user-authored groups and cross-restart view persistence are not included.

## Contracts and validation

- New model contracts initially failed because the navigation/signal models did not exist; both now pass. The initial browser geometry contract failed because Circuit had no navigation band. Browser checks subsequently caught undersized controls at 960px and a full-graph camera reset after hiding/showing Circuit; these were corrected before final evidence.
- `npm run build`, `tsc --noEmit`: pass. Only standard bundle-size warnings.
- Focused Vitest: **63 pass** across navigation, signal, cable paint, camera, popup, pointer, Source, serial composition and factory signal-flow contracts. A final formatting check reran the navigation/signal/paint subset: **32 pass**.
- Broad Circuit Vitest: **439 pass, 2 fail**, the already documented baseline failures in `chipSpec` (compressor expects five captions, actual catalogue exposes eleven) and `boardView.regression` (existing knob-array subscription). The new code does not modify either implementation.
- Browser regression set: **17 pass, 1 skip** across Circuit navigation, serial composition (960/1440), popup camera lock and Source geometry at 1×/2×. The skip is the existing duplicate-density popup case.
- Final large-patch fixture: **6 pass** (20/100/200 effect chips plus IN/OUT at both pixel densities), including bounded region counts, zero mounted chips in overview, card non-overlap, complete visible leaf bounds, effective caption size ≥8px, camera retention through Unit/Circuit switching, names retained on return, and search reaching the final chip. The final fixture uses the synthetic OUT terminal's real `in` jack; an earlier fixture used `main`, left that terminal disconnected and correctly fell back to general layout.
- Final signal-pixel checks: **2 pass**, using the production native-event bridge with controlled source tap snapshots. Sustained energy brightens the actual cable canvas; stale data and silence return it to its exact static baseline. Values are in `signal-pixels.json`. These are synthetic bridge inputs, not a native audio/DAW measurement.
- Native GraphModel/AstJson and DAW/WebView2: **not run**; no configured JUCE/native build in this workspace. No standalone or plugin binary rebuilt.

## Files

- `large-patches.json`: both densities, component counts, overview counts, depth, real chip rectangles and camera values. Serial graphs with 20, 100 and 200 effects remain the same graph and use the merged directed-rail layout.
- `overview-200-1x.png`: 960×860, 200-effect overview.
- `overview-200-2x.png`: 1440×860 at 2× density, same overview.
- `detail-100-1x.png` / `detail-100-2x.png`: three real components within the 100-effect graph; hardware finish preserved.
- `signal-active-1x.png`: controlled, non-clipping source energy, source contacts and transient tails on the actual renderer.
- `signal-pixels.json`: static, held, stale and released canvas intensity, measured through `getImageData`; no golden screenshot comparison.

Screenshots were inspected after capture. Camera and graph coordinates remain model-owned; DOM rectangles are used only by the browser assertions. Existing user-dragged geometry is never rearranged to make a region fit: a very spread-out region may need panning at the readable zoom floor.
