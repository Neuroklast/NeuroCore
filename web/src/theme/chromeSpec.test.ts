import { describe, expect, it } from "vitest";
import { barcodeBits, CHIP_CLIP, CHIP_CUT, DETAIL_HIT, chipExpandOffset, chipExpandOutsideBody, framePoints, greebleCode, segmentFill } from "./chromeSpec";

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
  });

  it("draws the cut frame in board space and maps live values to bars", () => {
    expect(framePoints(200, 80)).toBe("10,0 200,0 200,70 190,80 0,80 0,10");
    expect(segmentFill("0.850")).toBe(9);
    expect(segmentFill("not a number")).toBe(0);
  });
});
