import { describe, expect, it } from "vitest";
import { bindRailVisible, closedChipChrome, overlayHidesBindRail } from "./chipFace";

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

  it("hides the south bind rail while the overlay is open", () => {
    expect(overlayHidesBindRail(false)).toBe(false);
    expect(overlayHidesBindRail(true)).toBe(true);
    expect(bindRailVisible(false, ["cutoff"])).toBe(true);
    expect(bindRailVisible(true, ["cutoff"])).toBe(false);
    expect(bindRailVisible(false, [])).toBe(false);
  });
});
