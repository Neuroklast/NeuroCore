import { describe, expect, it } from "vitest";
import { ABOUT, aboutLegalIds, aboutUserFields, settingsAboutTarget } from "./aboutModel";

describe("about document", () => {
  it("identifies manufacturer and plugin", () => {
    expect(ABOUT.manufacturer).toBe("NEUROKLAST");
    expect(ABOUT.manufacturerDisplay).toBe("Neuroklast");
    expect(ABOUT.website).toBe("https://neuroklast.net");
    expect(ABOUT.email).toBe("info@neuroklast.net");
    expect(ABOUT.product).toBe("NEUROKORE");
    expect(ABOUT.version).toBe("0.4.11-alpha");
    expect(ABOUT.formats).toEqual(["Standalone", "VST3", "AU"]);
    expect(ABOUT.logos.manufacturer).toBe("./img/neuroklast.png");
    expect(ABOUT.logos.product).toBe("./img/neurokore.png");
  });

  it("carries the shipped legal text", () => {
    expect(ABOUT.copyright.toLowerCase()).toContain("all rights reserved");
    expect(ABOUT.copyright).toContain("2024");
    expect(ABOUT.copyright).toContain("NEUROKLAST");
    expect(aboutLegalIds()).toEqual([
      "license",
      "restrictions",
      "ownership",
      "termination",
      "disclaimer",
      "liability",
      "contact",
    ]);
    const bodies = ABOUT.eula.map((s) => s.body).join(" ");
    expect(bodies).toContain("licensed, not sold");
    expect(bodies).toContain("AS IS");
    expect(ABOUT.thirdParty.some((t) => t.name === "JUCE")).toBe(true);
    expect(ABOUT.thirdParty.some((t) => t.name.includes("VST"))).toBe(true);
  });

  it("is opened from Settings as about, not license", () => {
    expect(settingsAboutTarget()).toBe("about");
  });

  it("shows only user-facing fields, no plugin codes", () => {
    const keys = aboutUserFields().map((r) => r.key);
    expect(keys).toEqual(["Version", "Formats", "Web", "Mail"]);
    expect(keys.join(" ")).not.toMatch(/code|target|category|grain|noise/i);
  });
});
