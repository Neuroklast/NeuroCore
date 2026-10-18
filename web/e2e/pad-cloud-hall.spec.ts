import { installUnitHost } from "./unit-host";
import { test, expect } from "@playwright/test";

test.describe("Pad Cloud Hall workspace", () => {
  test.beforeEach(async ({ page }) => {
    await installUnitHost(page);
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
  });

  test("loads Pad Cloud Hall, Source shows reverb, camera does not jump on select", async ({ page }) => {
    await page.locator(".nk-preset-now").click();
    await page.locator(".nk-preset-browser").waitFor();
    await page.getByPlaceholder("Search presets, tags, effects...").fill("Pad Cloud Hall");
    const row = page.locator(".nk-preset-row", { hasText: "Pad Cloud Hall" });
    await expect(row).toBeVisible();
    await row.dblclick();
    await page.locator(".nk-preset-browser").waitFor({ state: "hidden" }).catch(() => undefined);

    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    const board = page.locator(".nk-board");
    const before = await board.evaluate((el) => el.getBoundingClientRect().x);
    await page.locator("[data-node-id='reverb1']").click();
    await page.locator(".nk-source-toggle").click();
    const panel = page.locator(".nk-source-inspector");
    await panel.waitFor();
    await expect(panel).toContainText("reverb1");
    await expect(panel).toContainText("decay");
    const after = await board.evaluate((el) => el.getBoundingClientRect().x);
    expect(after).toBe(before);
  });
});
