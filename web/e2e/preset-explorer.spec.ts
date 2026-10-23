import { test, expect, type Locator } from "@playwright/test";
import { assertNoOverlap, assertStableHitbox, type Box } from "./layout";

async function box(el: Locator): Promise<Box> {
  return el.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

test.describe("preset explorer panes", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.locator(".nk-preset-now").waitFor();
    await page.locator(".nk-preset-now").click();
    await page.locator(".nk-preset-browser").waitFor();
  });

  test("search, list and footer do not move when the description changes", async ({ page }) => {
    const chrome = page.locator(".nk-preset-chrome");
    const list = page.locator(".nk-preset-list");
    const folders = page.locator(".nk-preset-folders");
    const detail = page.locator(".nk-preset-detail");
    const foot = page.locator(".nk-preset-foot");
    const rows = page.locator(".nk-preset-row");
    await expect(rows.first()).toBeVisible();
    const chromeBox = await box(chrome);
    const listBox = await box(list);
    const footBox = await box(foot);
    assertNoOverlap(await box(folders), listBox, "folders vs list");
    assertNoOverlap(listBox, await box(detail), "list vs detail");
    const h = (await rows.first().boundingBox())?.height ?? 0;
    expect(h).toBe(44);
    const n = await rows.count();
    if (n > 1) {
      await rows.nth(1).click();
    }
    assertStableHitbox(chromeBox, await box(chrome), "search");
    assertStableHitbox(listBox, await box(list), "list");
    assertStableHitbox(footBox, await box(foot), "footer");
  });

  test("a small category fits without list scroll", async ({ page }) => {
    const folders = page.locator("[data-folder]");
    const count = await folders.count();
    for (let i = 0; i < count; i += 1) {
      await folders.nth(i).click();
      const n = await page.locator(".nk-preset-row").count();
      if (n >= 1 && n <= 9) {
        const list = page.locator(".nk-preset-list");
        const listBox = await box(list);
        expect(n * 44).toBeLessThanOrEqual(listBox.h + 2);
        const overflow = await list.evaluate((el) => el.scrollHeight - el.clientHeight);
        expect(overflow).toBeLessThanOrEqual(2);
        return;
      }
    }
    expect(await page.locator(".nk-preset-row").count()).toBeGreaterThan(0);
  });

  test("arrows move the list selection and Enter on other buttons does not load", async ({ page }) => {
    const list = page.locator(".nk-preset-list");
    const first = page.locator(".nk-preset-row").first();
    const second = page.locator(".nk-preset-row").nth(1);
    await expect(first).toBeVisible();
    await first.click();
    await expect(first).toHaveAttribute("data-selected", "true");
    await list.focus();
    await page.keyboard.press("ArrowDown");
    await expect(second).toHaveAttribute("data-selected", "true");
    const selected = await second.getAttribute("data-name");
    await page.locator("[data-type]").first().focus();
    await page.keyboard.press("Enter");
    await expect(page.locator(".nk-preset-browser")).toBeVisible();
    await expect(page.locator(`.nk-preset-row[data-name="${selected}"]`)).toHaveAttribute("data-selected", "true");
  });

  test("active filters stay visible, removable, and Clear all restores the list", async ({ page }) => {
    const style = page.locator("[data-style]").first();
    await expect(style).toBeVisible();
    const styleName = (await style.getAttribute("data-style")) ?? "";
    await style.click();
    await expect(page.locator("[data-active-filters]")).toBeVisible();
    await expect(page.locator(`[data-chip-facet="style"]`)).toContainText(styleName);
    await page.locator("[data-folder='Guitar']").click();
    await expect(page.locator(`[data-style="${styleName}"]`)).toBeVisible();
    await page.locator("[data-clear-filters]").click();
    await expect(page.locator("[data-active-filters]")).toHaveCount(0);
  });
});
