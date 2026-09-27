import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

const here = dirname(fileURLToPath(import.meta.url));
const web = join(here, "..", "..");
const html = readFileSync(join(web, "index.html"), "utf8");
const app = readFileSync(join(web, "src/app/App.tsx"), "utf8");
const main = readFileSync(join(web, "src/main.tsx"), "utf8");

describe("boot splash", () => {
  it("shows the mark with a progress bar under it", () => {
    expect(html).toContain('id="nk-splash"');
    expect(html).toMatch(/role="progressbar"/);
    expect(html).toContain("nk-splash-bar");
  });

  it("does not dismiss on the theme paint — only after UI_READY", () => {
    expect(app).not.toMatch(/dataset\.nkReady[\s\S]{0,80}nk-splash/);
    expect(app).toContain("dismissSplash");
    expect(app).toMatch(/UI_READY[\s\S]*dismissSplash|dismissSplash[\s\S]*UI_READY/);
    expect(main).toContain("setSplashProgress");
  });
});
