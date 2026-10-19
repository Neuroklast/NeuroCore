import { test, expect, type Page } from "@playwright/test";

type Box = { x: number; y: number; w: number; h: number };

function overlap(a: Box, b: Box): boolean {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

function contained(inner: Box, outer: Box, slack = 0.5): boolean {
  return inner.x >= outer.x - slack
    && inner.y >= outer.y - slack
    && inner.x + inner.w <= outer.x + outer.w + slack
    && inner.y + inner.h <= outer.y + outer.h + slack;
}

async function loadPreset(page: Page, name: string) {
  await page.locator(".nk-preset-now").click();
  await page.locator(".nk-preset-browser").waitFor();
  await page.getByPlaceholder("Search presets, tags, effects...").fill(name);
  const row = page.locator(".nk-preset-row", { hasText: name });
  await expect(row).toBeVisible();
  await row.dblclick();
  await page.locator(".nk-preset-browser").waitFor({ state: "hidden" }).catch(() => undefined);
}

async function waitLayoutIdle(page: Page) {
  const overlay = page.locator("[aria-label='Layout']");
  await page.locator(".nk-board").waitFor();
  await overlay.waitFor({ state: "attached", timeout: 4000 }).catch(() => undefined);
  await expect(overlay).toHaveCount(0, { timeout: 45000 });
}

async function boxOf(locator: ReturnType<Page["locator"]>): Promise<Box> {
  return locator.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

async function layoutSize(locator: ReturnType<Page["locator"]>): Promise<{ w: number; h: number }> {
  return locator.evaluate((n) => ({ w: (n as HTMLElement).offsetWidth, h: (n as HTMLElement).offsetHeight }));
}

test.describe("circuit chip zones", () => {
  test("octaver mute, face, and port labels do not overlap", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Drone Low Bed");
    const chip = page.locator("[data-node-id='octaver1']");
    await chip.waitFor();
    await waitLayoutIdle(page);

    const ms = chip.locator(".nk-chip-ms");
    const face = chip.locator(".nk-node-face");
    const titleBand = chip.locator(".nk-chip-subheader");
    await expect(ms).toBeVisible();
    await expect(face).toBeVisible();
    const msBox = await boxOf(ms);
    const faceBox = await boxOf(face);
    const bandBox = await boxOf(titleBand);
    const chipSize = await layoutSize(chip);
    const faceSize = await layoutSize(face);
    expect(chipSize.w).toBeGreaterThanOrEqual(200);
    expect(chipSize.h).toBeGreaterThanOrEqual(180);
    expect(faceSize.w).toBeGreaterThanOrEqual(180);
    expect(faceSize.h).toBeGreaterThanOrEqual(100);
    expect(overlap(msBox, faceBox), "mute/solo vs face").toBe(false);
    expect(contained(msBox, bandBox), "mute/solo inside technical band").toBe(true);
    await page.screenshot({ path: "test-results/circuit-ic-drone-low-bed.png", fullPage: true });

    const pins = chip.locator(".nk-port");
    const pinCount = await pins.count();
    expect(pinCount).toBeGreaterThan(0);
    const seen = new Set<string>();
    for (let i = 0; i < pinCount; i += 1) {
      const pin = await boxOf(pins.nth(i));
      const key = `${Math.round(pin.x)}:${Math.round(pin.y)}`;
      expect(seen.has(key), `pin ${i} collides at ${key}`).toBe(false);
      seen.add(key);
    }

    const caps = chip.locator(".nk-port-cap");
    const n = await caps.count();
    expect(n).toBeGreaterThan(0);
    for (let i = 0; i < n; i += 1) {
      const capEl = caps.nth(i);
      const vis = await capEl.evaluate((el) => getComputedStyle(el).visibility);
      if (vis === "hidden") {
        continue;
      }
      const cap = await boxOf(capEl);
      if (cap.w < 2 || cap.h < 2) {
        continue;
      }
      const jack = await capEl.evaluate((el) => (el.parentElement?.getAttribute("data-port-id") ?? el.textContent ?? `${i}`));
      expect(overlap(cap, msBox), `${jack} vs mute/solo`).toBe(false);
      expect(overlap(cap, faceBox), `${jack} vs face`).toBe(false);
    }
  });
});
