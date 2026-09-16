import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { CHIP_PAD_X, CHIP_PAD_Y } from "../assemble/chipMetrics";
import { barcodeBits, CHIP_CLIP, CHIP_CUT, DETAIL_HIT, chipExpandOffset, chipExpandOutsideBody, frameCorners, framePoints, greebleCode, headbandEndPad, satLampOn, segmentFill } from "./chromeSpec";

const here = path.dirname(fileURLToPath(import.meta.url));

describe("cyberpunk chrome spec", () => {
  it("cuts two opposite corners at 45 degrees, never rounds", () => {
    expect(CHIP_CLIP.startsWith("polygon(")).toBe(true);
    expect(CHIP_CLIP).toContain(`${CHIP_CUT}px 0`);
    expect(CHIP_CLIP).toContain(`0 ${CHIP_CUT}px`);
    expect(CHIP_CLIP.includes("border-radius") || CHIP_CLIP.includes("round")).toBe(false);
  });

  it("cuts every chrome corner from one token, never a raw pixel polygon", () => {
    const css = readFileSync(path.resolve(here, "tailwind.css"), "utf8");
    expect(css).toMatch(/--nk-cut-sm:\s*\d+px/);
    expect(css).toMatch(/--nk-cut:\s*\d+px/);
    expect(css).toMatch(/--nk-cut-lg:\s*\d+px/);
    const firstArgs = [...css.matchAll(/clip-path:\s*polygon\(\s*([^,\s]+)/g)].map((m) => m[1]);
    expect(firstArgs.length).toBeGreaterThan(0);
    expect(firstArgs.filter((a) => ! a.startsWith("var("))).toEqual([]);
  });

  it("stamps a stable tech greeble and barcode per node id", () => {
    expect(greebleCode("stage1")).toMatch(/^TRG-[0-9A-F]{4}$/);
    expect(greebleCode("stage1")).toBe(greebleCode("stage1"));
    expect(greebleCode("filter1")).not.toBe(greebleCode("stage1"));
    expect(barcodeBits("DRIVE").length).toBe(16);
    expect(barcodeBits("DRIVE")).toEqual(barcodeBits("DRIVE"));
  });

  it("gives the chip details control a 26px plate outside the clipped body", () => {
    expect(DETAIL_HIT).toBeGreaterThanOrEqual(26);
    expect(chipExpandOutsideBody).toBe(true);
    expect(chipExpandOffset().size).toBeGreaterThanOrEqual(26);
    expect(chipExpandOffset().top).toBe(CHIP_PAD_Y);
    expect(chipExpandOffset().right).toBe(CHIP_PAD_X);
  });

  it("reserves the expand plate at the end of the title band so the lamp is not under the chevron", () => {
    expect(headbandEndPad(false)).toBe(0);
    expect(headbandEndPad(true)).toBe(DETAIL_HIT);
    expect(headbandEndPad(true)).toBe(chipExpandOffset().size);
  });

  it("draws the cut frame in board space and maps live values to bars", () => {
    expect(framePoints(200, 80)).toBe("10,0 200,0 200,70 190,80 0,80 0,10");
    expect(frameCorners(200, 80)).toHaveLength(4);
    expect(frameCorners(200, 80).every((d) => d.startsWith("M "))).toBe(true);
    expect(satLampOn(-0.3)).toBe(true);
    expect(satLampOn(-1)).toBe(false);
    expect(segmentFill("0.850")).toBe(9);
    expect(segmentFill("not a number")).toBe(0);
  });
});
