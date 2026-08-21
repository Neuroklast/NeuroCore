import { describe, expect, it } from "vitest";
import { CHIP_PAD_X, CHIP_PAD_Y } from "../assemble/chipMetrics";
import { barcodeBits, CHIP_CLIP, CHIP_CUT, DETAIL_HIT, RESIZE_GRIP, SHELL_BEZEL, chipExpandOffset, chipExpandOutsideBody, frameCorners, framePoints, greebleCode, satLampOn, segmentFill, shellBezelCss } from "./chromeSpec";

describe("cyberpunk chrome spec", () => {
  it("cuts two opposite corners at 45 degrees, never rounds", () => {
    expect(CHIP_CLIP.startsWith("polygon(")).toBe(true);
    expect(CHIP_CLIP).toContain(`${CHIP_CUT}px 0`);
    expect(CHIP_CLIP).toContain(`0 ${CHIP_CUT}px`);
    expect(CHIP_CLIP.includes("border-radius") || CHIP_CLIP.includes("round")).toBe(false);
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

  it("draws the cut frame in board space and maps live values to bars", () => {
    expect(framePoints(200, 80)).toBe("10,0 200,0 200,70 190,80 0,80 0,10");
    expect(frameCorners(200, 80)).toHaveLength(4);
    expect(frameCorners(200, 80).every((d) => d.startsWith("M "))).toBe(true);
    expect(satLampOn(-0.3)).toBe(true);
    expect(satLampOn(-1)).toBe(false);
    expect(segmentFill("0.850")).toBe(9);
    expect(segmentFill("not a number")).toBe(0);
  });

  it("draws a thin screen bezel and a bottom-right resize grip", () => {
    expect(SHELL_BEZEL).toBeGreaterThanOrEqual(5);
    expect(SHELL_BEZEL).toBeLessThanOrEqual(10);
    expect(RESIZE_GRIP).toBeGreaterThanOrEqual(20);
    const css = shellBezelCss();
    expect(css).toContain("inset");
    expect(css).toContain("--nk-accent-rgb");
    expect(css).toMatch(/6px|7px|8px|5px/);
  });
});
