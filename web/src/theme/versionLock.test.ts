import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { nk } from "./tokens";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");

describe("product version is one string", () => {
  it("About, UI_READY, Inno, and pack script share nk.version", () => {
    expect(nk.version).toBe("0.7.1-beta");
    const numeric = nk.version.replace(/-beta.*$/, "");

    const app = readFileSync(join(root, "web/src/app/App.tsx"), "utf8");
    expect(app).toContain("build: nk.version");
    expect(app).not.toMatch(/build:\s*["']0\.\d/);

    const iss = readFileSync(join(root, "installer/NeuroKore.iss"), "utf8");
    expect(iss).toContain(`#define MyAppVersion "${nk.version}"`);
    expect(iss).toContain(`#define MyAppNumeric "${numeric}"`);

    const pack = readFileSync(join(root, "scripts/package_windows.ps1"), "utf8");
    expect(pack).toContain("NEUROKORE_VERSION_LABEL");
    expect(pack).toContain("NEUROKORE_VERSION_NUMERIC");
    expect(pack).not.toMatch(/\$version = "0\.\d+\.\d+-beta"/);
    expect(pack).toContain('Filter "NEUROKORE.vst3"');
    expect(pack).not.toContain('Filter "$stem.vst3"');

    const cmake = readFileSync(join(root, "CMakeLists.txt"), "utf8");
    expect(cmake).toMatch(/NeuroKore_VST3 PROPERTIES OUTPUT_NAME "NEUROKORE"/);
    expect(cmake).not.toMatch(/NeuroKore_VST3 PROPERTIES OUTPUT_NAME "\$\{NEUROKORE_FILE_STEM\}"/);

    const modinfo = readFileSync(join(root, "scripts/write_vst3_moduleinfo.mjs"), "utf8");
    expect(modinfo).toContain('name.toLowerCase() === "neurokore.vst3"');
    expect(modinfo).not.toContain("name.toLowerCase().includes (version.toLowerCase())");
  });

  it("alpha pack and local build banner take NEUROKORE_VERSION_LABEL", () => {
    const cmake = readFileSync(join(root, "CMakeLists.txt"), "utf8");
    const label = cmake.match(/set\(NEUROKORE_VERSION_LABEL "([^"]+)"\)/)?.[1];
    expect(label).toBe(nk.version);

    const alpha = readFileSync(join(root, "scripts/package_alpha.py"), "utf8");
    expect(alpha).toContain("NEUROKORE_VERSION_LABEL");
    expect(alpha).not.toMatch(/0\.\d+\.\d+-beta/);

    const buildLocal = readFileSync(join(root, "scripts/build_local.sh"), "utf8");
    expect(buildLocal).toContain("NEUROKORE_VERSION_LABEL");
    expect(buildLocal).not.toMatch(/0\.\d+\.\d+-beta/);

    const modinfo = readFileSync(join(root, "scripts/write_vst3_moduleinfo.mjs"), "utf8");
    expect(modinfo).not.toMatch(/let version = "0\.\d+\.\d+-beta"/);

    const install = readFileSync(join(root, "docs/ALPHA_INSTALL.txt"), "utf8");
    expect(install).toContain(`NEUROKORE ${nk.version}`);

    const notes = readFileSync(join(root, "docs/ALPHA_RELEASE_NOTES.md"), "utf8");
    expect(notes).toContain(`NEUROKORE ${nk.version}`);
  });

  it("Windows release syncs the living manual and packages every time", () => {
    const norm = (text: string) => text.replace(/\r\n/g, "\n");
    const manual = norm(readFileSync(join(root, "docs/manual/NEUROKORE.md"), "utf8"));
    const help = norm(readFileSync(join(root, "resources/UserManual_en.txt"), "utf8"));
    expect(manual).toContain(nk.version);
    expect(help).toBe(manual);

    const ci = readFileSync(join(root, ".github/workflows/web-layout.yml"), "utf8");
    expect(ci).toContain("npm run build");
    expect(ci).not.toMatch(/playwright/i);
    expect(ci).not.toContain("test:layout");

    const release = readFileSync(join(root, "build_release.bat"), "utf8");
    expect(release).toContain("scripts\\sync_living_docs.mjs");
    expect(release).toContain("scripts\\package_windows.ps1");
    expect(release).not.toMatch(/set "VERSION_LABEL=0\.\d/);

    const pack = readFileSync(join(root, "scripts/package_windows.ps1"), "utf8");
    expect(pack).toContain("NEUROKORE_VERSION_LABEL");
    expect(pack).toContain("sync_living_docs.mjs");
    expect(pack).not.toMatch(/\$version = "0\.\d+\.\d+-beta"/);
  });
});
