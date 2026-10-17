import { test, expect, type Locator } from "@playwright/test";
import {
  assertContainedWithin,
  assertNoOverlap,
  assertNoUnexpectedWrap,
  assertStableHitbox,
  boxOf,
  type Box,
} from "./layout";

declare global {
  interface Window {
    __nkSetPreset?: (name: string, dirty?: boolean) => void;
  }
}

async function box(el: Locator): Promise<Box> {
  return boxOf(await el.boundingBox() ?? { x: 0, y: 0, width: 0, height: 0 });
}

test.describe("unit hud + preset cluster", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.waitForFunction(() => document.fonts.status === "loaded");
    await page.locator(".nk-face-hud").waitFor();
  });

  test("telemetry does not overlap diagnostic stacks", async ({ page }) => {
    const teleL = page.locator(".nk-face-tele-l");
    const teleR = page.locator(".nk-face-tele-r");
    const diagL = page.locator(".nk-face-diag-l");
    const diagR = page.locator(".nk-face-diag-r");
    const tl = await box(teleL);
    const tr = await box(teleR);
    const dl = await box(diagL);
    const dr = await box(diagR);
    assertNoOverlap(tl, dl, "engine vs core status");
    assertNoOverlap(tr, dr, "signal vs field activity");
    assertNoOverlap(dl, dr, "diag columns");
    for (const row of await teleL.locator(".nk-face-row").all()) {
      assertNoUnexpectedWrap(await row.evaluate((n) => ({ scrollWidth: n.scrollWidth, clientWidth: n.clientWidth })), "engine row");
    }
    for (const row of await teleR.locator(".nk-face-row").all()) {
      assertNoUnexpectedWrap(await row.evaluate((n) => ({ scrollWidth: n.scrollWidth, clientWidth: n.clientWidth })), "signal row");
    }
    assertContainedWithin(await box(page.locator(".nk-face-orbit-leg")), dl, "orbit legend");
    expect(await page.locator(".nk-face-diag-l .nk-face-mod").count()).toBe(3);
    expect(await page.locator(".nk-face-diag-r .nk-face-mod").count()).toBe(3);
  });

  test("preset arrows stay put when the name changes", async ({ page }) => {
    const cluster = page.locator(".nk-preset-cluster");
    await cluster.waitFor();
    const prev = cluster.locator("button").nth(0);
    const now = cluster.locator(".nk-preset-now");
    const next = cluster.locator("button").nth(2);
    const refPrev = await box(prev);
    const refNow = await box(now);
    const refNext = await box(next);
    const names = [
      "A",
      "BASS",
      "TENSION PULSE",
      "EXTREMELY LONG INDUSTRIAL MASTERING CHAIN",
      "X".repeat(200),
      "Ø∆/ß",
      "",
    ];
    for (const name of names) {
      await page.evaluate((n) => window.__nkSetPreset?.(n, false), name);
      await page.waitForTimeout(16);
      assertStableHitbox(refPrev, await box(prev), "prev");
      assertStableHitbox(refNow, await box(now), "name");
      assertStableHitbox(refNext, await box(next), "next");
    }
    await page.evaluate(() => window.__nkSetPreset?.("TENSION PULSE", true));
    await page.waitForTimeout(16);
    assertStableHitbox(refPrev, await box(prev), "prev dirty");
    assertStableHitbox(refNow, await box(now), "name dirty");
    assertStableHitbox(refNext, await box(next), "next dirty");
  });
});
