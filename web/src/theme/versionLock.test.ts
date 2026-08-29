import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { nk } from "./tokens";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");

describe("product version is one string", () => {
  it("About, UI_READY, Inno, and pack script share nk.version", () => {
    expect(nk.version).toBe("0.6.4-beta");
    const numeric = nk.version.replace(/-beta.*$/, "");

    const app = readFileSync(join(root, "web/src/app/App.tsx"), "utf8");
    expect(app).toContain("build: nk.version");
    expect(app).not.toMatch(/build:\s*["']0\.\d/);

    const iss = readFileSync(join(root, "installer/NeuroKore.iss"), "utf8");
    expect(iss).toContain(`#define MyAppVersion "${nk.version}"`);
    expect(iss).toContain(`#define MyAppNumeric "${numeric}"`);

    const pack = readFileSync(join(root, "scripts/package_windows.ps1"), "utf8");
    expect(pack).toContain(`$version = "${nk.version}"`);
    expect(pack).toContain(`$numeric = "${numeric}"`);
  });
});
