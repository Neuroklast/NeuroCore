import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  CRT_Z,
  OVERLAY_Z,
  overlayBodyOverflow,
  overlayIsSplit,
  overlayIsWide,
  overlayShowsHostClose,
  overlaySizesToContent,
} from "./overlayChrome";
import { OVERLAY_ASSEMBLE, OVERLAY_BACKDROP_ALPHA, OVERLAY_DISASSEMBLE, OVERLAY_PANEL_ALPHA } from "./overlayMotion";

const here = path.dirname(fileURLToPath(import.meta.url));
const css = readFileSync(path.resolve(here, "../theme/tailwind.css"), "utf8");

describe("overlay chrome", () => {
  it("pins the folder rail on explorers — the list scrolls, the rail does not", () => {
    expect(overlayIsSplit("presets")).toBe(true);
    expect(overlayIsSplit("functions")).toBe(true);
    expect(overlayIsSplit("stages")).toBe(true);
    expect(overlayIsSplit("help")).toBe(true);
    expect(overlayBodyOverflow("presets")).toBe("hidden");
    expect(overlayBodyOverflow("settings")).toBe("auto");
    expect(overlayIsWide("presets")).toBe(true);
    expect(overlayIsWide("validate")).toBe(false);
  });

  it("sizes inspect to content and keeps explorers on the wide stage", () => {
    expect(overlaySizesToContent("inspect")).toBe(true);
    expect(overlaySizesToContent("settings")).toBe(true);
    expect(overlaySizesToContent("functions")).toBe(false);
    expect(css).toMatch(/\.nk-overlay-host[\s\S]*?align-items:\s*center/);
    expect(css).toMatch(/\.nk-overlay \{[\s\S]*?height:\s*auto/);
    expect(css).not.toMatch(/\.nk-overlay \{\s*max-height:\s*100%;\s*height:\s*100%;/);
  });

  it("keeps CRT under the overlay so Functions stays sharp", () => {
    expect(OVERLAY_Z).toBeGreaterThan(CRT_Z);
    expect(css).toMatch(/\.nk-overlay-host \{[\s\S]*?z-index:\s*40/);
    expect(css).toMatch(/\.nk-crt \{[\s\S]*?z-index:\s*30/);
  });

  it("uses a translucent board dimmer and panel, clip times match JS", () => {
    expect(OVERLAY_BACKDROP_ALPHA).toBeCloseTo(0.45);
    expect(OVERLAY_PANEL_ALPHA).toBeCloseTo(0.82);
    expect(css).toContain(`rgba(0, 0, 0, ${OVERLAY_BACKDROP_ALPHA})`);
    expect(css).toContain(`nk-overlay-assemble ${OVERLAY_ASSEMBLE.ms / 1000}s`);
    expect(css).toContain(`nk-overlay-disassemble ${OVERLAY_DISASSEMBLE.ms / 1000}s`);
  });

  it("uses the title X only — no second Close", () => {
    expect(overlayShowsHostClose("presets")).toBe(false);
    expect(overlayShowsHostClose("stages")).toBe(false);
    expect(overlayShowsHostClose("settings")).toBe(false);
    expect(overlayShowsHostClose("inspect")).toBe(false);
    expect(overlayShowsHostClose("functions")).toBe(false);
  });

  it("draws the closer as two ticks, not a clipped X chip", () => {
    const src = readFileSync(path.resolve(here, "Overlays.tsx"), "utf8");
    expect(src).toContain("nk-overlay-x");
    expect(src).not.toMatch(/nk-clip px-3[\s\S]*>X</);
    expect(css).toContain(".nk-overlay-x");
    expect(css).toMatch(/\.nk-overlay-x::before/);
  });
});
