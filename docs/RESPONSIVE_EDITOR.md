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
