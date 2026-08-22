import { describe, expect, it } from "vitest";
import {
  faceGlitchStyle,
  flashKeepsAlpha,
  logoMotion,
  logoMotionStyle,
  logoOverlayMask,
  pickFaceGlitch,
  scheduleFaceGlitch,
  unitMarkCssVars,
  unitMarkMaxHeight,
  unitMarkSrc,
  UNIT_MARK_DEFAULT,
  UNIT_MARK_DIGICIDE,
  UNIT_MARK_DIGICIDE_H,
  UNIT_MARK_H,
} from "./faceGlitch";

describe("unit logo glitch", () => {
  it("cycles slice bars, a full flash, and pixel-noise dissolve — not a 1px jitter", () => {
    expect(pickFaceGlitch(0)).toBe("slice");
    expect(pickFaceGlitch(0.6)).toBe("slice");
    expect(pickFaceGlitch(0.7)).toBe("flash");
    expect(pickFaceGlitch(0.9)).toBe("noise");

    expect(scheduleFaceGlitch("idle", 0)).toBeGreaterThanOrEqual(700);
    expect(scheduleFaceGlitch("flash", 1)).toBeLessThan(140);
    expect(scheduleFaceGlitch("slice", 1)).toBeLessThan(240);
    expect(scheduleFaceGlitch("noise", 0)).toBeGreaterThanOrEqual(180);

    const slice = faceGlitchStyle("slice", 42);
    expect(slice["--nk-g-y1"]).toMatch(/%$/);
    expect(slice["--nk-g-h1"]).toMatch(/%$/);
    expect(slice["--nk-g-x"]).toMatch(/px$/);
    expect(Number.parseFloat(slice["--nk-g-h1"] ?? "0")).toBeGreaterThan(2);
  });

  it("keeps logo alpha — no invert, no cream fill, overlays masked to the PNG", () => {
    expect(flashKeepsAlpha("brightness(2.4) saturate(1.2)")).toBe(true);
    expect(flashKeepsAlpha("invert(1) hue-rotate(180deg)")).toBe(false);
    expect(flashKeepsAlpha("background:#f4f1ea")).toBe(false);
    const mask = logoOverlayMask("./img/neurokore.png");
    expect(mask.maskImage).toContain("neurokore.png");
    expect(mask.maskSize).toBe("contain");
  });

  it("uses the Digicide mask only on the DIGICIDE theme", () => {
    expect(unitMarkSrc("digicide")).toBe(UNIT_MARK_DIGICIDE);
    expect(unitMarkSrc("signal")).toBe(UNIT_MARK_DEFAULT);
    expect(unitMarkSrc("gold")).toBe(UNIT_MARK_DEFAULT);
    expect(unitMarkSrc("azure")).toBe(UNIT_MARK_DEFAULT);
    expect(unitMarkSrc("")).toBe(UNIT_MARK_DEFAULT);
    expect(UNIT_MARK_DIGICIDE).toContain("digicide.png");
    expect(UNIT_MARK_DEFAULT).toContain("neurokore.png");
  });

  it("clips rain/scan to the live mark and grows DIGICIDE by half", () => {
    expect(unitMarkMaxHeight("digicide")).toBe(UNIT_MARK_DIGICIDE_H);
    expect(unitMarkMaxHeight("signal")).toBe(UNIT_MARK_H);
    expect(UNIT_MARK_DIGICIDE_H).toBe(Math.round(UNIT_MARK_H * 1.5));
    const digi = unitMarkCssVars(UNIT_MARK_DIGICIDE, "digicide");
    expect(digi["--nk-face-mark"]).toContain("digicide.png");
    expect(digi["--nk-face-mark"]).not.toContain("neurokore.png");
    expect(digi["--nk-face-mark-h"]).toBe("252px");
    const signal = unitMarkCssVars(UNIT_MARK_DEFAULT, "signal");
    expect(signal["--nk-face-mark"]).toContain("neurokore.png");
    expect(signal["--nk-face-mark-h"]).toBe("168px");
  });
});

describe("unit logo motion from loudness / bass / treble", () => {
  it("maps fixed bands: loudness→glow, treble→chroma px, bass→jitter Hz", () => {
    const quiet = logoMotion(0, 0, 0, { motion: "full", prefersReduced: false });
    expect(quiet.glow).toBe(0);
    expect(quiet.chromaPx).toBe(0);
    expect(quiet.jitterHz).toBe(0);

    const loud = logoMotion(1, 0, 0, { motion: "full", prefersReduced: false });
    const bass = logoMotion(0.2, 1, 0, { motion: "full", prefersReduced: false });
    const treb = logoMotion(0.2, 0, 1, { motion: "full", prefersReduced: false });

    expect(loud.glow).toBeGreaterThan(0.8);
    expect(loud.chromaPx).toBe(0);
    expect(loud.jitterHz).toBe(0);

    expect(bass.jitterHz).toBeGreaterThan(treb.jitterHz);
    expect(bass.jitterHz).toBeGreaterThan(4);
    expect(bass.chromaPx).toBe(0);

    expect(treb.chromaPx).toBeGreaterThan(bass.chromaPx);
    expect(treb.chromaPx).toBeGreaterThan(3);
    expect(treb.jitterHz).toBe(0);

    const mid = logoMotion(0.5, 0.5, 0.5, { motion: "full", prefersReduced: false });
    expect(mid.glow).toBeGreaterThan(quiet.glow);
    expect(mid.glow).toBeLessThan(loud.glow);
    expect(mid.chromaPx).toBeGreaterThan(0);
    expect(mid.jitterHz).toBeGreaterThan(0);

    const css = logoMotionStyle(treb);
    expect(css["--nk-logo-chroma"]).toBe(`${treb.chromaPx}px`);
    expect(css["--nk-logo-jitter-hz"]).toBe(String(treb.jitterHz));
    expect(flashKeepsAlpha(JSON.stringify(css))).toBe(true);
  });

  it("zeros all channels when prefers-reduced-motion or motion is off", () => {
    const bands = { loudness: 1, bass: 1, treble: 1 } as const;
    const off = logoMotion(bands.loudness, bands.bass, bands.treble, {
      motion: "off",
      prefersReduced: false,
    });
    const reducedOs = logoMotion(bands.loudness, bands.bass, bands.treble, {
      motion: "full",
      prefersReduced: true,
    });
    expect(off).toEqual({ glow: 0, chromaPx: 0, jitterHz: 0 });
    expect(reducedOs).toEqual({ glow: 0, chromaPx: 0, jitterHz: 0 });

    const snap = logoMotion(1, 1, 1, { motion: "reduced", prefersReduced: false });
    expect(snap.glow).toBeGreaterThan(0);
    expect(snap.chromaPx).toBe(0);
    expect(snap.jitterHz).toBe(0);
  });
});

