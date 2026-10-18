import path from "node:path";
import { fileURLToPath } from "node:url";
import { test, expect, type Page } from "@playwright/test";

const shotDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../docs/audits/signal-lab-baseline");

async function loadPreset(page: Page, name: string) {
  await page.locator(".nk-preset-now").click();
  await page.locator(".nk-preset-browser").waitFor();
  await page.getByPlaceholder("Search presets, tags, effects...").fill(name);
  const row = page.locator(".nk-preset-row", { hasText: name });
  await expect(row).toBeVisible();
  await row.dblclick();
  await page.locator(".nk-preset-browser").waitFor({ state: "hidden" }).catch(() => undefined);
}

/** Hydrate sets layoutBusy; Compact/Arrange must finish before a baseline shot. */
async function waitLayoutIdle(page: Page) {
  const overlay = page.locator("[aria-label='Layout']");
  await page.locator(".nk-board").waitFor();
  await overlay.waitFor({ state: "attached", timeout: 4000 }).catch(() => undefined);
  await expect(overlay).toHaveCount(0, { timeout: 45000 });
}

test.describe("signal lab baseline screenshots", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
  });

  test("captures Circuit and Terminal for Drone Low Bed and Pad Cloud Hall", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='filter1']").waitFor();
    await waitLayoutIdle(page);
    await page.screenshot({ path: path.join(shotDir, "circuit-drone-low-bed.png"), fullPage: true });

    await page.locator("[data-node-id='filter1']").click();
    await waitLayoutIdle(page);
    await page.screenshot({ path: path.join(shotDir, "circuit-drone-low-bed-filter.png"), fullPage: true });

    await page.getByRole("button", { name: "Terminal" }).click();
    await page.locator(".nk-term").waitFor();
    await expect(page.locator(".nk-term")).toContainText("octaver1", { timeout: 20000 });
    await page.screenshot({ path: path.join(shotDir, "terminal-drone-low-bed.png"), fullPage: true });

    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Pad Cloud Hall");
    await page.locator("[data-node-id='reverb1']").waitFor();
    await waitLayoutIdle(page);
    await page.screenshot({ path: path.join(shotDir, "circuit-pad-cloud-hall.png"), fullPage: true });

    await page.getByRole("button", { name: "Terminal" }).click();
    await page.locator(".nk-term").waitFor();
    await expect(page.locator(".nk-term")).toContainText("reverb1", { timeout: 20000 });
    await page.screenshot({ path: path.join(shotDir, "terminal-pad-cloud-hall.png"), fullPage: true });
  });
});
