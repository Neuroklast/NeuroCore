import { test, expect } from "@playwright/test";
import { writeFileSync } from "node:fs";
for (const width of [960, 1440])
  test(`Circuit navigation owns a stable control band at ${width}px`, async ({
    page,
  }, info) => {
    test.setTimeout(120000);
    await page.setViewportSize({ width, height: 860 });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    const nav = page.getByRole("navigation", { name: "Circuit navigation" });
    await expect(nav).toBeVisible();
    const geometry = async () =>
      nav.locator("button,input").evaluateAll((els) =>
        els.map((el) => {
          const r = el.getBoundingClientRect();
          return {
            label: el.getAttribute("aria-label") ?? el.textContent,
            x: r.x,
            y: r.y,
            w: r.width,
            h: r.height,
          };
        }),
      );
    const before = await geometry();
    await nav.getByPlaceholder("Find chip…").fill("filter");
    expect(await geometry()).toEqual(before);
    const b = await page.locator(".nk-board").boundingBox();
    for (const r of before) {
      expect(r.h).toBeGreaterThanOrEqual(26);
      expect(r.x).toBeGreaterThanOrEqual(b!.x);
      expect(r.x + r.w).toBeLessThanOrEqual(b!.x + b!.width + 1);
    }
    writeFileSync(
      info.outputPath("navigation-geometry.json"),
      JSON.stringify(before, null, 2),
    );
    await page.screenshot({
      path: info.outputPath(`navigation-${width}.png`),
      fullPage: true,
    });
  });
