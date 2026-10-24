import { test, expect, type Page } from "@playwright/test";

async function bounding(page: Page, selector: string) {
  return page.locator(selector).first().evaluate((node) => {
    const r = node.getBoundingClientRect();
    return { left: r.left, right: r.right, top: r.top, bottom: r.bottom, width: r.width, height: r.height };
  });
}

test.describe("responsive editor", () => {
  for (const { width, height, mode } of [
    { width: 1280, height: 860, mode: "wide" },
    { width: 1100, height: 740, mode: "compact" },
    { width: 900, height: 600, mode: "tight" },
  ]) {
    test(`reflows chrome, knobs, panes and modal at ${width}x${height}`, async ({ page }, info) => {
      test.skip(info.project.name !== "chromium-1x");
      await page.setViewportSize({ width, height });
      await page.goto("/");
      await page.locator("html[data-nk-ready='1']").waitFor();
      const stage = page.locator(".nk-scale-stage");
      await expect(stage).toHaveAttribute("data-layout", mode);
      await expect(stage).toHaveAttribute("data-scaled", "false");
      const transform = await stage.evaluate((e) => getComputedStyle(e).transform);
      expect(transform).toBe("none");
      const root = await bounding(page, ".nk-os");
      expect(root.width).toBeCloseTo(width, 0);
      expect(root.height).toBeCloseTo(height, 0);

      const header = await bounding(page, ".nk-toolbar");
      const frame = await bounding(page, ".nk-frame");
      const knobs = await bounding(page, ".nk-knob-rail");
      expect(frame.height).toBeGreaterThan(80);
      expect(frame.bottom).toBeLessThanOrEqual(knobs.top + 1);
      expect(header.bottom).toBeLessThan(frame.top);
      for (const label of ["Previous preset", "Next preset", "Settings", "Help", "Toggle dry-only monitoring"]) {
        const hit = page.getByRole("button", { name: label, exact: true });
        await expect(hit).toBeVisible();
        const b = await hit.boundingBox();
        expect(b).not.toBeNull();
        expect(b!.x).toBeGreaterThanOrEqual(-1);
        expect(b!.x + b!.width).toBeLessThanOrEqual(width + 1);
        expect(b!.y + b!.height).toBeLessThanOrEqual(height + 1);
      }
      await page.locator(".nk-preset-now").click();
      const explorer = page.getByRole("dialog", { name: "Preset Explorer" });
      await expect(explorer).toBeVisible();
      for (const selector of [".nk-preset-folders", ".nk-preset-list", ".nk-preset-detail"]) {
        const b = await bounding(page, selector);
        expect(b.width).toBeGreaterThan(80);
        expect(b.right).toBeLessThanOrEqual(width + 1);
        expect(b.bottom).toBeLessThanOrEqual(height + 1);
      }
      await page.getByRole("button", { name: "Close", exact: true }).click();
      await page.getByRole("button", { name: "Circuit", exact: true }).click();
      await expect(page.locator(".nk-board")).toBeVisible();
    });
  }

  test("tiny browser preview uses explicit safety scaling", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    await page.setViewportSize({ width: 720, height: 480 });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await expect(page.locator(".nk-scale-stage")).toHaveAttribute("data-scaled", "true");
    await expect(page.locator(".nk-scale-stage")).toHaveAttribute("data-layout", "tight");
  });
});
