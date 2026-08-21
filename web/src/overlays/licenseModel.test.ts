import { describe, expect, it } from "vitest";
import { licenseBuyerVisible, licenseStatusLine } from "./licenseModel";

describe("license overlay copy", () => {
  it("states licensed vs remaining demo time", () => {
    expect(licenseStatusLine(true, 0)).toBe("LICENSED");
    expect(licenseStatusLine(false, 125)).toBe("DEMO 2:05 until dry mute");
    expect(licenseStatusLine(false, 0)).toBe("DEMO 0:00 until dry mute");
  });

  it("shows the buyer email only when present", () => {
    expect(licenseBuyerVisible("  a@b.co ")).toBe("a@b.co");
    expect(licenseBuyerVisible("")).toBe("");
  });
});
