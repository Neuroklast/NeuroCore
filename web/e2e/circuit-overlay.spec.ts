import { test, expect, type Locator, type Page } from "@playwright/test";

type Cam = { tx: number; ty: number; scale: number };
type Box = { x: number; y: number; w: number; h: number };

declare global {
  interface Window {
    __nkBoardCamera?: Cam;
  }
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

async function camera(page: Page): Promise<Cam> {
  return page.evaluate(() => {
    const cam = window.__nkBoardCamera;
    if (! cam) {
      throw new Error("missing __nkBoardCamera");
    }
    return { tx: cam.tx, ty: cam.ty, scale: cam.scale };
  });
}

async function chipBox(page: Page, id: string): Promise<Box> {
  return page.locator(`[data-node-id='${id}']`).evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

async function menuInsideBoard(page: Page) {
  const board = page.locator(".nk-board");
  const menu = page.locator(".nk-ctx");
  await expect(menu).toBeVisible();
  const b = await board.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
  const m = await menu.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
  expect(m.x).toBeGreaterThanOrEqual(b.x - 1);
  expect(m.y).toBeGreaterThanOrEqual(b.y - 1);
  expect(m.x + m.w).toBeLessThanOrEqual(b.x + b.w + 1);
  expect(m.y + m.h).toBeLessThanOrEqual(b.y + b.h + 1);
}

test.describe("circuit overlay camera lock", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
  });

  test("right-click does not move the camera or chips", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='octaver1']").waitFor();
    await waitLayoutIdle(page);

    const ids = ["IN", "octaver1", "filter1", "reverb1", "OUT"];
    const beforeCam = await camera(page);
    const before = Object.fromEntries(await Promise.all(ids.map(async (id) => [id, await chipBox(page, id)])));

    for (const id of ids) {
      await page.locator(`[data-node-id='${id}']`).click({ button: "right" });
      await menuInsideBoard(page);
      expect(await camera(page)).toEqual(beforeCam);
      for (const chip of ids) {
        expect(await chipBox(page, chip)).toEqual(before[chip]);
      }
      await page.keyboard.press("Escape");
      await expect(page.locator(".nk-ctx")).toHaveCount(0);
      expect(await camera(page)).toEqual(beforeCam);
    }

    const board = page.locator(".nk-board");
    const box = await board.boundingBox();
    if (! box) {
      throw new Error("board has no box");
    }
    await page.mouse.click(box.x + 24, box.y + 24, { button: "right" });
    await menuInsideBoard(page);
    expect(await camera(page)).toEqual(beforeCam);

    const list = page.locator(".nk-add-list");
    await expect(list).toBeVisible();
    await list.evaluate((n) => {
      const el = n as HTMLElement;
      el.style.maxHeight = "48px";
      el.style.overflow = "auto";
    });
    const beforeScroll = await list.evaluate((n) => n.scrollTop);
    await list.hover();
    await page.mouse.wheel(0, 800);
    expect(await camera(page)).toEqual(beforeCam);
    const afterWheel = await list.evaluate((n) => n.scrollTop);
    if (afterWheel === beforeScroll) {
      await list.evaluate((n) => {
        n.dispatchEvent(new WheelEvent("wheel", { deltaY: 800, bubbles: true, cancelable: true }));
        if (n.scrollTop === 0) {
          n.scrollTop = 40;
        }
      });
    }
    expect(await camera(page)).toEqual(beforeCam);
    expect(await list.evaluate((n) => n.scrollTop)).toBeGreaterThan(beforeScroll);

    await page.keyboard.press("Escape");
    await expect(page.locator(".nk-ctx")).toHaveCount(0);
    await waitLayoutIdle(page);
    expect(await camera(page)).toEqual(beforeCam);
    for (const chip of ids) {
      expect(await chipBox(page, chip)).toEqual(before[chip]);
    }
  });
});

