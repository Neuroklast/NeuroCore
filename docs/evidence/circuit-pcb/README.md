# Circuit: one continuous PCB

The supplied native screenshots rejected the region-card hierarchy and the forced single serial row. This correction always mounts the complete graph. Search focuses a real chip on the same board; Fit board returns to the whole circuit. No region cards, hidden groups, boundary buttons or separate selection workspace remain.

Serial placement compares complete reading-order row compositions, including 64 px horizontal gutters and 128 px inter-row routing channels. The last row ends at the right. Placement produces the serial trace routes directly. General graphs retain ranked rows with area-based width growth. Reroute preserves manual positions. Chip captions are 17 px and the DSP baseline is 352 px, retaining the gunmetal, dotted face and metallic pin design.

The single substrate and fit camera share bounds over chips and routed traces. This matters: visual review caught outer traces escaping the initial candidate's substrate and control band despite passing chip-only geometry checks. A new failing contract reproduced the clipping before the bounds model was corrected. A subsequent DOM dump exposed a load-time camera race: a completed compact board occupied less than 46% of the pane because an early fit invalidated the worker-owned fit. Initial node/resize fitting now yields while layout is busy; layout reads the current pane size. Browser checks require the fitted graph to occupy at least 75% of one available dimension.

## Validation

- Production TypeScript/Vite build passes; the existing large-bundle warning remains.
- Initial reproduction: all four compact-board aspect contracts failed on the old infinite rail; the browser found **0 of 22 chips** because the hierarchy mounted cards instead.
- 56 focused geometry/signal/camera contracts pass. The preceding 101-test layout/paint set also passed, including the factory routing audit.
- Final full Circuit Vitest: **445 pass, 2 existing failures**. See `vitest-summary.json`. The unchanged failures concern the compressor catalogue and BoardChip knob-array subscription; they are not treated as passing.
- After the camera ownership correction, all 23 camera/commit/pointer/overlay tests pass.
- Final Playwright: **25 pass, 1 existing duplicate-density skip**. Runs cover 960/1440 windows at 1×/2×, all 6/20/100/200-effect chips mounted and contained, non-overlapping chip rectangles, same-board search, Unit/Circuit camera retention, fixed toolbar hitboxes, the three reported factory presets, Drone captions/pins, popup camera lock and Source bounds.
- Signal-pixel fixtures show positive energy under fresh source telemetry and return to the silent baseline after stale/released telemetry. All chips remain mounted throughout.
- Model dumps contain every route point, Manhattan length and corner count. `quality.json` also reports proper segment crossings; crossings in branched graphs are measured, not claimed to be eliminated. Foreign-chip route collision checks pass for all four named presets.

## Evidence

- `wide-motion-960.png`, `bass-regen-growl-960.png`, `multiband-punch-960.png`: the reported preset families, complete boards.
- `multiband-punch-1440-2x.png`: full 24-component board at high density.
- `drone-low-bed-1440.png`: two-row serial PCB with larger captions and retained chip material.
- `board-200-960.png`: 202 actual components on one continuous board.
- `browser-geometry.json`: actual DOM rectangles and camera measurements.
- Four preset JSON dumps plus `metrics-960.json` / `metrics-1440.json`: headless placement/routing geometry.

These are screenshots of the production web build, not a generated design mockup or native DAW capture. Factory browser fixtures use the web DSL parser. Large fixtures inject explicit graphs through a simulated native bridge. Native GraphModel/AstJson, rebuilt embedded assets and DAW/WebView2 review were not run: this workspace has no configured native/JUCE build. No plugin binary was rebuilt; DSP, version 0.7.1-beta and VST identifiers are unchanged.

At 100/200 effects, whole-board fit is an overview; small text requires wheel zoom or Find chip. That changes the camera only and never removes the rest of the circuit. This test is a layout/interaction check, not a native frame-rate benchmark.
