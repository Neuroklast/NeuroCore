import { describe, expect, it, vi } from "vitest";
import {
  completeOnlineActivate,
  licenseBuyerVisible,
  licenseDocumentFromActivateResponse,
  licenseStatusLine,
} from "./licenseModel";

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

describe("online activate installs only a verified license document", () => {
  const doc = "NEUROKORE LICENSE\nemail=a@b.co\nsig=1";

  it("rejects HTTP 200 without a license document and does not import", async () => {
    const importLicense = vi.fn(async () => true);
    const out = await completeOnlineActivate(200, '{"ok":true}', importLicense);
    expect(out.ok).toBe(false);
    expect(out.message).toMatch(/INSTALL \.LIC/i);
    expect(out.message).not.toMatch(/stored/i);
    expect(importLicense).not.toHaveBeenCalled();
    expect(licenseDocumentFromActivateResponse(200, '{"ok":true}').ok).toBe(false);
  });

  it("imports a JSON license field and reports installed only after native success", async () => {
    const importLicense = vi.fn(async (text: string) => text.includes("NEUROKORE LICENSE"));
    const out = await completeOnlineActivate(200, JSON.stringify({ license: doc }), importLicense);
    expect(importLicense).toHaveBeenCalledWith(doc);
    expect(out).toEqual({ ok: true, message: "License installed." });
  });

  it("does not claim success when native import rejects the document", async () => {
    const out = await completeOnlineActivate(200, JSON.stringify({ license: doc }), async () => false);
    expect(out.ok).toBe(false);
    expect(out.message).toMatch(/rejected/i);
    expect(out.message).not.toMatch(/stored/i);
  });

  it("imports a raw license document", async () => {
    const importLicense = vi.fn(async () => true);
    const out = await completeOnlineActivate(200, doc, importLicense);
    expect(importLicense).toHaveBeenCalledWith(doc);
    expect(out.ok).toBe(true);
  });

  it("keeps HTTP failures on the install-.lic path", async () => {
    const importLicense = vi.fn(async () => true);
    const out = await completeOnlineActivate(500, doc, importLicense);
    expect(importLicense).not.toHaveBeenCalled();
    expect(out.message).toMatch(/Online activate failed/i);
  });
});
