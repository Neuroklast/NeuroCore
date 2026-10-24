# Responsive NEUROKORE editor

The web UI uses a real-size flex layout within the JUCE WebView. Its reference visual size
remains 1280×860, but it **does not scale down the entire page** inside native editor
windows. Text, hit targets, panel geometry and pointer movements remain measured in
CSS pixels.

| Window | Layout |
| --- | --- |
| 1280×860, larger wide surfaces | Wide one-row toolbar and 3-pane preset explorer |
| 1100×740 | Compact toolbar reflow, scrollable footer, narrower explorer columns |
| 900×600 | Tight mode, reduced chrome vertical cost, scrollable panels |
| Under 900×600 (browser preview only) | 900×600 virtual safety stage scaled down; native editor cannot resize below minimum |

Breakpoints: `layoutForViewport` in `web/src/theme/fit.ts`. `ScaleShell` uses
ResizeObserver; the stage's `data-layout` attribute selects the CSS rules in
`web/src/theme/tailwind.css`. Existing board camera and pointer utilities use
`clientToPane`, so the Circuit view adapts when its actual pane changes size.

JUCE: `Config::kUiMinWindowWidth/Height = 900×600`, max 1920×1290;
`WebPluginEditor` has **no fixed aspect ratio**; width and height are persisted
and restored separately. Standalone and host formats may have additional
platform-specific size restrictions.

The layout preserves all existing controls; no menu actions are removed at
compact sizes. A narrow footer scrolls horizontally instead of ellipsizing
latency and license information.

Tests: `web/src/theme/fit.test.ts` and `web/e2e/responsive-editor.spec.ts`,
along with the regular Web layout and native `GlobalPreferencesTest` suite.

Interactive browser geometry check (manual on a GPU-capable runner):
`cd web && npm run build && npx playwright install chromium && npx playwright test e2e/responsive-editor.spec.ts --project=chromium-1x`.
CI runs fast responsive breakpoints and CSS/native source contracts instead;
software-rendered Chromium can hang while synchronously forcing full-page layout
with the active Unit visualizer. Do not treat contract checks as a screenshot audit.

## Visual polish and window aspect

Width and height use **independent** layout signals. `data-layout=wide|compact|tight`
controls horizontal reflow, while `data-height=regular|short` controls
vertical density. A wide-but-short host window (e.g. 1600×700) therefore
keeps the one-row toolbar and allocates the saved height to the canvas.

At 900×600, the Unit's left and right diagnostics columns scroll **inside
their own panes** instead of compressing the Spectrum, Dynamics and field
visualizers into clipped boxes. Primary controls and the central core stay
visible. The macro dials have reduced vertical SVG geometry but retain their
editable readout, range endpoints and binding indicators. The compact footer
keeps the license status pinned to the right while the remaining telemetry
can scroll horizontally. Focus-visible controls use the cyan outline.

CI includes `web/e2e/responsive-css-fixture.spec.ts`: a headless Chromium
render of the **compiled production stylesheet** with small representative
DOM fixtures, rather than mounting the real GPU-heavy Unit. This checks
actual geometry/overflow and complements the pure responsive unit contracts.
It does not replace a native WebView2, VST3, AU or DAW screenshot pass.
