import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { fitCanvas } from "./canvasFit";

const here = path.dirname(fileURLToPath(import.meta.url));

describe("scope canvas fit", () => {
  it("sizes the backing store to the CSS box times dpr, not a stretched 720×128", () => {
    const el = {
      clientWidth: 900,
      clientHeight: 110,
      width: 720,
      height: 128,
    } as HTMLCanvasElement;
    const r = fitCanvas(el, 2);
    expect(r.w).toBe(900);
    expect(r.h).toBe(110);
    expect(r.scale).toBe(2);
    expect(el.width).toBe(1800);
    expect(el.height).toBe(220);
  });

  it("FunctionPlot fits the CSS box instead of stretching a 520×220 bitmap", () => {
    const src = readFileSync(path.resolve(here, "../functions/FunctionPlot.tsx"), "utf8");
    expect(src).toContain("fitCanvas");
    expect(src).not.toMatch(/width=\{520\}/);
  });
});

it('includes host scaling in the backing store while preserving logical geometry', () => {
  const el = { clientWidth: 900, clientHeight: 110, width: 900, height: 110,
    getBoundingClientRect: () => ({ width: 1350, height: 165 }) } as HTMLCanvasElement;
  const fit=fitCanvas(el,2);
  expect(fit.w).toBe(900);
  expect(fit.scale).toBe(3);
  expect(el.width).toBe(2700);
  expect(el.height).toBe(330);
});
