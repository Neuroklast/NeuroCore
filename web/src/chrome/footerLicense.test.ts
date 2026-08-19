import { describe, expect, it } from "vitest";
import { footerLicenseLabel } from "./footerLicense";

describe("footer license slot", () => {
  it("shows LIC when licensed, else remaining demo time", () => {
    expect(footerLicenseLabel(true, 0)).toBe("LIC");
    expect(footerLicenseLabel(false, 125)).toBe("DEMO 2:05");
    expect(footerLicenseLabel(false, 0)).toBe("DEMO 0:00");
  });
});
