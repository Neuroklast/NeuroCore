import { installUnitHost } from "./unit-host";
import { test, expect, type Locator } from "@playwright/test";
import { assertNoOverlap, type Box } from "./layout";

async function box(el: Locator): Promise<Box> {
  return el.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

test.describe("circuit source inspector", () => {
  test.beforeEach(async ({ page }) => {
    await installUnitHost(page);
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
  });

  test("Source panel is 360–420 px and does not overlap knobs", async ({ page }) => {
    await page.locator(".nk-source-toggle").click();
    const panel = page.locator(".nk-source-inspector");
    await panel.waitFor();
    const p = await box(panel);
    expect(p.w).toBeGreaterThanOrEqual(360);
    expect(p.w).toBeLessThanOrEqual(420);
    const knobs = page.locator(".nk-bind-host .flex").last();
    if (await knobs.count()) {
      const k = await box(knobs);
      if (k.w > 8 && k.h > 8) {
        assertNoOverlap(p, k, "source inspector vs knobs");
      }
    }
    await expect(panel.getByText("Edit Source")).toBeVisible();
  });
});
