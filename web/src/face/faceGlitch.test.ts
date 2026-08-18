import { describe, expect, it } from "vitest";
import { faceGlitchStyle, flashKeepsAlpha, logoOverlayMask, pickFaceGlitch, scheduleFaceGlitch } from "./faceGlitch";

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
});

