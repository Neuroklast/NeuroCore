# Circuit visual recovery

Base: `40c3e0e19d3a81e8988e432a0074cb97e290bcef`, version 0.7.1-beta. No DSP or version changes.

The first browser contract failed on the unchanged base because OCTAVER was clipped. The recovery restores cut corners, the dotted surface and complete effect names while retaining calculated NodeFaces, routing, camera behavior, telemetry and Source Inspector. Name, technical controls, signal surface, parameter captions and status have separate geometry. Full captions grow the plate on the board grid. Macro-only expression fields retain binding endpoints in the technical row. Unconnected/silent pins remain metal; connected pins light from their source signal. Clipping marks retain their real overload condition.

## Screenshots

All captures use the browser renderer, not a native VST3/DAW. Full-window images share the 1280×860 viewport. These are actual application screenshots, not mockups or recolored reference images.

- [Previous main](before-main.png): clipped names and dominant plots.
- [Recovered, idle](after-idle-1x.png): name, code, dotted surface and parameter/status separation.
- [Recovered, selected](after-selected-1x.png): stronger calculated plot without a geometry change.
- [Recovered at 2× density](after-selected-2x.png).
- [Historical code reference](historical-5b7f856.png), [Octaver crop](historical-octaver.png).
- Themes: [Signal](drone-signal.png), [Gold](drone-gold.png), [Azure](drone-azure.png), [DIGICIDE](drone-digicide.png).
- [Pad Cloud Hall](pad-cloud-hall.png), [Bass Fractionator](bass-fractionator.png).

The historical ref supplied in the brief, `5b7f856e11d`, identifies itself as **0.6.6-beta**. Its original renderer was built separately for this comparison. It is a code reference for the requested earlier identity, not proof of an exact 0.6.7 native build. The supplied 0.6.7 screenshots were not available as image attachments in this workspace.

## Validation

- `npm run build`: passed, including TypeScript. Existing Vite chunk-size advisories remain.
- `npx vitest run src/assemble src/theme`: **468 passed, 4 failed**, 90 files.
- The four failures also reproduce on unchanged `40c3e0e`: boardView's existing knobs subscription, chipSpec's outdated compressor catalog expectation, chromeSpec's existing raw polygon, and CRT bloom's existing alpha. No new failures remain in this scope.
- Main browser run: **10 passed, 2 skipped**. Existing chip-zone and overlay tests explicitly skip their 2× variants; the new recovery contracts independently exercise both densities.
- Additional real-pointer LFO/parameter drag run: **2 passed**, one per density. LFO output magnetizes the cutoff pin, while audio does not. Drags are cancelled before commit; native graph compilation/persistence is a separate host test.
- Browser coverage: Drone Low Bed, Pad Cloud Hall, Bass Fractionator, four themes, 1×/2×, repeated 960×645 → 1600×1075 → 1280×860 resize, multiple wheel zoom levels, selection, real DOM rectangles for chrome/control/parameter/pin collisions, unclipped names/typecodes, source-signal pin state, context-menu camera lock and Source Inspector.
- Headless connection contract reproduced rejected LFO → parameter and accepted audio → parameter before normalizing parameter/control ports as modulation; both checks now pass.

Run from `web/`:

```sh
npm ci
npm run build
npx vitest run src/assemble src/theme
npx playwright test e2e/circuit-visual-recovery.spec.ts e2e/circuit-chip-zones.spec.ts e2e/circuit-overlay.spec.ts e2e/circuit-source.spec.ts --workers=1
```

This environment used Chromium 153 with software rendering through `NK_CHROMIUM`. No native WebView2, DAW, native performance baseline or audio-engine validation is claimed. Existing native tests were not rebuilt for this web-only change. Do not merge as a native release signoff.

## Publication

The implementation is committed locally on `fix/circuit-visual-recovery`. Automatic approval review rejected publication to the public GitHub remote, requiring explicit end-user authorization for that destination and payload. No push or PR was completed.
