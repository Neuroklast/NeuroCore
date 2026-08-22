import { describe, expect, it } from "vitest";
import { demoClipRows } from "./demoClips";

describe("demoClipRows", () => {
  it("writes the same clip rows the host would, louder toward IN, L≠R", () => {
    const a = demoClipRows(["IN", "stage1", "OUT"], 0);
    const b = demoClipRows(["IN", "stage1", "OUT"], 8);
    expect(a).toHaveLength(3);
    expect(a[0]?.id).toBe("IN");
    expect(a[0]!.peak).toBeGreaterThan(0);
    expect(a[0]!.rms).toBeGreaterThan(0);
    expect(a[0]!.rms).toBeLessThan(a[0]!.peak);
    expect(Math.abs(a[0]!.peakL - a[0]!.peakR)).toBeGreaterThan(0.01);
    expect(Math.abs(a[0]!.peak - b[0]!.peak)).toBeGreaterThan(0.01);
  });
});
