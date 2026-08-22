import { describe, expect, it } from "vitest";
import { peakToDb } from "../bridge/telemetry";
import {
  bindRailVisible,
  chipFootLine,
  clipWarnMark,
  closedChipChrome,
  overloadLabel,
  overlayHidesBindRail,
  portBracket,
} from "./chipFace";

describe("closed chip chrome", () => {
  it("does not paint TRG codes, barcodes, grips, or fake jacks", () => {
    for (const id of ["filter", "stage", "eq", "env", "osc"]) {
      const c = closedChipChrome(id, `${id}1`);
      expect(c.greeble, id).toBe(false);
      expect(c.barcode, id).toBe(false);
      expect(c.grip, id).toBe(false);
      expect(c.fakeJacks, id).toBe(false);
    }
  });

  it("picks the lamp from the job", () => {
    expect(closedChipChrome("stage", "stage1").lamp).toBe("clip");
    expect(closedChipChrome("osc", "osc1").lamp).toBe("lfo");
    expect(closedChipChrome("env", "env1").lamp).toBe("env");
  });

  it("wraps port ids in hardware brackets from the jack name", () => {
    expect(portBracket("mid")).toBe("[ MID ]");
    expect(portBracket("side")).toBe("[ SIDE ]");
    expect(portBracket("left")).toBe("[ L ]");
    expect(portBracket("right")).toBe("[ R ]");
    expect(portBracket("in")).toBe("[ IN ]");
    expect(portBracket("out")).toBe("[ OUT ]");
    expect(portBracket("sc")).toBe("[ SC ]");
    expect(portBracket("mid")).not.toBe("mid");
  });

  it("prints node id and live dB on the foot, never a UUID", () => {
    expect(chipFootLine("filter1", 0)).toBe("filter1  —");
    expect(chipFootLine("filter1", 10 ** (-12 / 20))).toBe("filter1  -12dB");
    expect(chipFootLine("stage1", 1.2)).toContain("stage1");
    expect(chipFootLine("stage1", 1.2)).not.toMatch(/[0-9a-f]{8}-[0-9a-f]{4}/i);
    expect(overloadLabel(0.9)).toBeNull();
    expect(overloadLabel(1.2)).toBe(`${peakToDb(1.2) >= 0 ? "+" : ""}${peakToDb(1.2).toFixed(1)}dB`);
  });

  it("paints clip as an outlined hazard triangle with a bang, not a filled header pip", () => {
    const mark = clipWarnMark();
    expect(mark.fill).toBe("none");
    expect(mark.viewBox).toBe("0 0 24 24");
    expect(mark.triangle.trim().split(/\s+/)).toHaveLength(3);
    expect(mark.stem.startsWith("M12 ")).toBe(true);
    expect(mark.dot.r).toBeGreaterThan(0);
    expect(overloadLabel(0.99)).toBeNull();
    expect(overloadLabel(1)).not.toBeNull();
  });

  it("hides the south bind rail while the overlay is open", () => {
    expect(overlayHidesBindRail(false)).toBe(false);
    expect(overlayHidesBindRail(true)).toBe(true);
    expect(bindRailVisible(false, ["cutoff"])).toBe(true);
    expect(bindRailVisible(true, ["cutoff"])).toBe(false);
    expect(bindRailVisible(false, [])).toBe(false);
  });
});
