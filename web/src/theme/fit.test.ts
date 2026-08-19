import { describe, expect, it } from "vitest";
import { DESIGN_H, DESIGN_W, fitForWindow, fitOrigin, menuPos, snapUiFitToGrid } from "./fit";

describe("design scale", () => {
  it("keeps a 1280x860 canvas", () => {
    expect(DESIGN_W).toBe(1280);
    expect(DESIGN_H).toBe(860);
  });

  it("fills a matching-aspect window exactly, no letterbox snap", () => {
    expect(fitForWindow(1280, 860)).toBe(1);
    expect(fitForWindow(2560, 1720)).toBe(2);
    expect(fitForWindow(0, 0)).toBe(1);
    const fit = fitForWindow(1920, 1290);
    expect(DESIGN_W * fit).toBeCloseTo(1920, 5);
    expect(DESIGN_H * fit).toBeCloseTo(1290, 5);
    expect(fitOrigin(1920, 1290, fit)).toEqual({ x: 0, y: 0 });
    const snapped = snapUiFitToGrid(1400 / 1280);
    expect(DESIGN_W * snapped).toBeLessThan(1400);
  });

  it("keeps the design aspect after scale", () => {
    const fit = fitForWindow(1600, 900);
    expect((DESIGN_W * fit) / (DESIGN_H * fit)).toBeCloseTo(DESIGN_W / DESIGN_H, 8);
  });

  it("never scales the canvas larger than the window", () => {
    const cases: Array<[number, number]> = [
      [1270, 850],
      [1900, 1000],
      [1100, 739],
      [1440, 900],
      [1600, 900],
      [1280, 860],
    ];
    for (const [w, h] of cases) {
      const fit = fitForWindow(w, h);
      expect(DESIGN_W * fit).toBeLessThanOrEqual(w + 1e-6);
      expect(DESIGN_H * fit).toBeLessThanOrEqual(h + 1e-6);
    }
  });

  it("clamps a context menu inside the editor", () => {
    const el = {
      offsetWidth: 1280,
      offsetHeight: 860,
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 640, height: 430 }),
    };
    const pos = menuPos(620, 420, el, 188, 176);
    expect(pos.left + 188).toBeLessThanOrEqual(1280);
    expect(pos.top + 176).toBeLessThanOrEqual(860);
    expect(pos.left).toBeGreaterThanOrEqual(0);
  });

  it("flips a tall context menu up when it would clip the board", () => {
    const el = {
      offsetWidth: 640,
      offsetHeight: 400,
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 640, height: 400 }),
    };
    const pos = menuPos(40, 380, el, 188, 280);
    expect(pos.top + 280).toBeLessThanOrEqual(400);
    expect(pos.top).toBeGreaterThanOrEqual(0);
  });
});
