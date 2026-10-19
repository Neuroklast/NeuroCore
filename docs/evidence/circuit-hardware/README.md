# Circuit hardware finish

2026-10-19, 0.7.1-beta. Browser screenshots of the actual production bundle at 1280×860. Telemetry is the existing browser demonstration host; these are not native DAW captures.

- [Board at 1×](board-1x.png), [Board at 2×](board-2x.png)
- [Octaver at 1×](octaver-1x.png), [Octaver at 2×](octaver-2x.png)
- Prior merged design: [selected board](../circuit-recovery/after-selected-1x.png).

The shared housing has upper/left lit facets and a lower/right shadow, recessed M/S/Inspect controls, 11 px technical identification and metal contacts. The dot field occupies only the central signal region. Full effect names retain their own row. Muting subdues the face rather than fading its name. IN/OUT start at 160×160 instead of 160×256; DSP height remains 256 and width follows captions, identification and control demand.

South ports are numeric bindable parameters; TYPE/CHANNEL stay in Inspect. Local port geometry is shared by DOM and routing, including clamped IO pin hit areas. Compact uses actual widths per row, reserves terminal room when a useful first pair fits, and aligns OUT to its incoming rail where possible. Explicit arrangement rules and manual-position persistence remain in place.

Validation:

- Production build, including TypeScript: passed.
- 89 targeted Vitest contracts across 11 files: passed. New compact-IO, enum-rail and per-row-width contracts failed on the previous implementation before the changes.
- Focused Playwright suite: 14 passed, 2 existing density duplicates skipped. Covers Drone Low Bed, Pad Cloud Hall, Bass Fractionator, all four themes, 1×/2× density, resize, zoom, selection, modulation drag compatibility, context-menu camera lock and Source geometry.
- Final housing checks: 2 passed on the final bundle (1×/2×). They also inspect computed texture/material backgrounds and muted selected-chip opacity/geometry at both densities.

Native DAW/WebView2 visual validation is pending; no native binary was requested or rebuilt. No DSP or product-version changes. This is a visual implementation for review, not a claim of pixel identity with an unavailable historical binary.
