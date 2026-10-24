import { test, expect } from "@playwright/test";

/**
 * The scope canvas and live Circuit animations are expensive on software-only
 * CI Chromium. Smoke-test layout identity and control reachability without
 * forcing repeated synchronous full-page getBoundingClientRect() calculations.
 */
test.describe("responsive editor", () => {
  for (const { width, height, mode } of [
    { width: 1280, height: 860, mode: "wide" },
    { width: 1100, height: 740, mode: "compact" },
    { width: 900, height: 600, mode: "tight" },
  ]) {
    test(`keeps essential controls reachable at ${width}x${height}`, async ({ page }, info) => {
      test.skip(info.project.name !== "chromium-1x");
      await page.setViewportSize({ width, height });
      await page.goto("/");
      await page.locator("html[data-nk-ready='1']").waitFor();
      const stage = page.locator(".nk-scale-stage");
      await expect(stage).toHaveAttribute("data-layout", mode);
      await expect(stage).toHaveAttribute("data-scaled", "false");
      await expect(page.locator(".nk-os")).toHaveClass(/h-full/);
      await expect(page.locator(".nk-toolbar")).toBeVisible();
      await expect(page.locator(".nk-frame")).toBeVisible();
      await expect(page.locator(".nk-knob-rail")).toBeVisible();
      await expect(page.locator(".nk-preset-cluster")).toBeVisible();
      await expect(page.locator(".nk-header-right")).toBeVisible();
      for (const label of ["Previous preset", "Next preset", "Settings", "Help", "Toggle dry-only monitoring"]) {
        await expect(page.getByRole("button", { name: label, exact: true })).toBeVisible();
      }
      if (mode === "tight") {
        await page.locator(".nk-preset-now").click({ timeout: 10000 });
        await expect(page.getByRole("dialog", { name: "Preset Explorer" })).toBeVisible();
        await expect(page.locator(".nk-preset-folders")).toBeVisible();
        await expect(page.locator(".nk-preset-detail")).toBeVisible();
      }
    });
  }

  test("browser views below native size use explicit fallback", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    await page.setViewportSize({ width: 720, height: 480 });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await expect(page.locator(".nk-scale-stage")).toHaveAttribute("data-scaled", "true");
    await expect(page.locator(".nk-scale-stage")).toHaveAttribute("data-layout", "tight");
  });
});
