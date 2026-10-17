import { test, expect, type Locator } from "@playwright/test";
import {
  assertNoOverlap,
  assertStableHitbox,
  type Box,
} from "./layout";

declare global {
  interface Window {
    __nkSetPreset?: (name: string, dirty?: boolean) => void;
  }
}

async function box(el: Locator): Promise<Box> {
  return el.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

test.describe("unit hud + preset cluster", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
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
    const wrapOf = (root: Locator, label: string) =>
      root.evaluate((el, tag) => {
        for (const n of el.querySelectorAll(".nk-face-row")) {
          if (n.scrollWidth > n.clientWidth + 1) {
            throw new Error(`${tag} wraps/overflows`);
          }
        }
      }, label);
    await wrapOf(teleL, "engine row");
    await wrapOf(teleR, "signal row");
    const orbitLeg = page.locator(".nk-face-diag-l .nk-face-orbit-leg");
    expect(await orbitLeg.count()).toBe(1);
    expect(await orbitLeg.locator("span").count()).toBe(3);
    const orbitClip = await orbitLeg.evaluate((el) => {
      const host = el.closest(".nk-face-mod");
      if (! host) {
        throw new Error("orbit legend has no card");
      }
      const a = el.getBoundingClientRect();
      const b = host.getBoundingClientRect();
      return a.bottom > b.bottom + 1 || a.right > b.right + 1;
    });
    expect(orbitClip).toBe(false);
    expect(await page.locator(".nk-face-diag-l .nk-face-mod").count()).toBe(3);
    expect(await page.locator(".nk-face-diag-r .nk-face-mod").count()).toBe(3);
  });

  test("preset cluster does not overlap LIVE", async ({ page }) => {
    const cluster = page.locator(".nk-preset-cluster");
    const mode = page.getByRole("group", { name: "Mode" });
    await cluster.waitFor();
    await mode.waitFor();
    assertNoOverlap(await box(cluster), await box(mode), "preset vs LIVE");
    await page.locator(".nk-preset-now").click();
    await expect(page.locator(".nk-preset-browser")).toBeVisible();
  });

  test("preset arrows stay put when the name changes", async ({ page }) => {
    const cluster = page.locator(".nk-preset-cluster");
    await cluster.waitFor();
    const prev = cluster.locator(":scope > button").first();
    const now = cluster.locator(".nk-preset-now");
    const next = cluster.locator(":scope > button").last();
    const refPrev = await box(prev);
    const refNow = await box(now);
    const refNext = await box(next);
    const names = [
      "A",
      "BASS",
      "TENSION PULSE",
      "EXTREMELY LONG INDUSTRIAL MASTERING CHAIN",
      "Ø∆/ß",
      "",
    ];
    for (const name of names) {
      const tip = name.trim() || "untitled";
      await page.evaluate((n) => window.__nkSetPreset?.(n, false), name);
      await expect(now).toHaveAttribute("data-tip", tip);
      const hit = await cluster.evaluate((el) => {
        const btns = [...el.querySelectorAll(":scope > button")];
        return btns.map((b) => {
          const r = b.getBoundingClientRect();
          return { x: r.x, y: r.y, w: r.width, h: r.height };
        });
      });
      expect(hit.length).toBe(3);
      assertStableHitbox(refPrev, hit[0]!, "prev");
      assertStableHitbox(refNow, hit[1]!, "name");
      assertStableHitbox(refNext, hit[2]!, "next");
    }
    await page.evaluate(() => window.__nkSetPreset?.("TENSION PULSE", true));
    await expect(now).toHaveAttribute("data-tip", "TENSION PULSE*");
    assertStableHitbox(refPrev, await box(prev), "prev dirty");
    assertStableHitbox(refNow, await box(now), "name dirty");
    assertStableHitbox(refNext, await box(next), "next dirty");
  });
});
