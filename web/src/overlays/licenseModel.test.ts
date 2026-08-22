import { describe, expect, it } from "vitest";
import { licenseBuyerVisible, licenseStatusLine } from "./licenseModel";

describe("license overlay copy", () => {
  it("states licensed vs remaining demo time", () => {
    expect(licenseStatusLine(true, 0)).toBe("LICENSED");
    const now = Date.UTC(2026, 7, 22);
    expect(licenseStatusLine(false, 14 * 86400, now)).toBe("Demo ends 5 Sep 2026 (14 days left)");
    expect(licenseStatusLine(false, 0, now)).toBe("DEMO ended — Mix stays dry until you install a license");
  });

  it("shows the buyer email only when present", () => {
    expect(licenseBuyerVisible("  a@b.co ")).toBe("a@b.co");
    expect(licenseBuyerVisible("")).toBe("");
  });
});
