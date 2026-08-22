import { describe, expect, it } from "vitest";
import { demoEndDateLabel, footerLicenseLabel } from "./footerLicense";

describe("footerLicenseLabel", () => {
  it("shows LIC when licensed, else remaining days and end date", () => {
    expect(footerLicenseLabel(true, 0)).toBe("LIC");
    expect(footerLicenseLabel(false, 0)).toBe("DEMO ended");
    const now = Date.UTC(2026, 7, 22);
    expect(footerLicenseLabel(false, 14 * 86400, now)).toBe("DEMO 14d · 5 Sep 2026");
    expect(demoEndDateLabel(14 * 86400, now)).toBe("5 Sep 2026");
    expect(footerLicenseLabel(false, 86400, now)).toMatch(/^DEMO 1d · /);
  });
});
