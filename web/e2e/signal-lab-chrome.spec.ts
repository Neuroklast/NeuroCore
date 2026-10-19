import path from "node:path";
import { fileURLToPath } from "node:url";
import { test, expect, type Locator, type Page } from "@playwright/test";
import { assertStableHitbox, type Box } from "./layout";

const shotDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../docs/audits/signal-lab-chrome");

async function loadPreset(page: Page, name: string) {
  await page.locator(".nk-preset-now").click();
  await page.locator(".nk-preset-browser").waitFor();
  await page.getByPlaceholder("Search presets, tags, effects...").fill(name);
  const row = page.locator(`.nk-preset-row[data-name="${name}"]`);
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

async function boxOf(el: Locator): Promise<Box> {
  return el.evaluate((n) => {
    const r = n.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
  });
}

async function clickChipHeader(page: Page, chip: Locator) {
  const r = await chip.boundingBox();
  if (! r) {
    throw new Error("chip has no box");
  }
  await page.mouse.click(r.x + Math.min(56, r.width * 0.25), r.y + Math.min(14, r.height * 0.12));
}

test.describe("signal lab shared module chrome", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
  });

  test("keeps filter AABB and jacks still on select, with distinct family faces", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='filter1']").waitFor();
    await waitLayoutIdle(page);

    const board = page.locator(".nk-board");
    const filter = page.locator("[data-node-id='filter1']");
    const reverb = page.locator("[data-node-id='reverb1']");
    await expect(filter).toHaveAttribute("data-face", "filter");
    await expect(reverb).toHaveAttribute("data-face", "reverb");
    await expect(filter).toHaveAttribute("data-depth", "1");
    await expect(filter.locator("[data-glyph='response']")).toHaveAttribute("data-axis", "log-freq");
    await expect(reverb.locator("[data-glyph='decay']")).toHaveAttribute("data-axis", "time");
    await expect(filter.locator("[data-glyph='response']")).toHaveAttribute("aria-label", "calculated filter response");
    await expect(reverb.locator("[data-glyph='decay']")).toHaveAttribute("aria-label", "calculated decay envelope");
    await expect(filter.locator("path[data-curve='response']")).toBeVisible();
    await expect(reverb.locator("path[data-curve='decay']")).toBeVisible();
    const filterD = await filter.locator("path[data-curve='response']").getAttribute("d");
    const reverbD = await reverb.locator("path[data-curve='decay']").getAttribute("d");
    expect(filterD && filterD.length > 8).toBe(true);
    expect(reverbD && reverbD.length > 8).toBe(true);
    expect(filterD).not.toBe(reverbD);
    await expect(reverb.locator("[data-glyph='decay']")).toHaveAttribute("data-decay", "0.95");
    const size = Number(await reverb.locator("[data-glyph='decay']").getAttribute("data-size"));
    expect(size).toBeGreaterThan(0.5);
    expect(size).toBeLessThan(0.7);
    await expect(board).toHaveAttribute("data-zoom", /^(overview|normal|focused)$/);

    const titleSize = await filter.locator(".nk-chip-title").evaluate((n) => getComputedStyle(n).fontSize);
    const footSize = await filter.locator(".nk-chip-foot").evaluate((n) => getComputedStyle(n).fontSize);
    expect(titleSize).toBe("13px");
    expect(footSize).toBe("10px");

    await page.screenshot({ path: path.join(shotDir, "circuit-drone-low-bed.png"), fullPage: true });

    const idle = await boxOf(filter);
    const jack = filter.locator(".nk-port").first();
    const jackIdle = await boxOf(jack);
    await clickChipHeader(page, filter);
    await expect(filter).toHaveAttribute("data-selected", "on");
    await expect(filter).toHaveAttribute("data-depth", "2");
    assertStableHitbox(idle, await boxOf(filter), "filter AABB on select");
    assertStableHitbox(jackIdle, await boxOf(jack), "filter jack on select");

    await page.screenshot({ path: path.join(shotDir, "circuit-drone-low-bed-filter.png"), fullPage: true });

    await loadPreset(page, "Pad Cloud Hall");
    await page.locator("[data-node-id='reverb1']").waitFor();
    await waitLayoutIdle(page);
    const hallReverb = page.locator("[data-node-id='reverb1']");
    await expect(hallReverb).toHaveAttribute("data-face", "reverb");
    await expect(hallReverb.locator("[data-glyph='decay']")).toBeVisible();
    await page.screenshot({ path: path.join(shotDir, "circuit-pad-cloud-hall.png"), fullPage: true });

    await loadPreset(page, "Split Authority");
    await page.locator("[data-node-id='xover1']").waitFor();
    await waitLayoutIdle(page);
    const xover = page.locator("[data-node-id='xover1']");
    await expect(xover).toBeVisible();
    const xoverBox = await xover.evaluate((n) => ({
      w: (n as HTMLElement).offsetWidth,
      h: (n as HTMLElement).offsetHeight,
    }));
    expect(xoverBox.w).toBeGreaterThanOrEqual(200);
    expect(xoverBox.h).toBeGreaterThanOrEqual(200);
    await page.screenshot({ path: path.join(shotDir, "circuit-split-authority.png"), fullPage: true });
  });

  test("delay LFO and dynamics faces paint calculated glyphs", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();

    await loadPreset(page, "Chorus Delay");
    await page.locator("[data-node-id='delay1']").waitFor();
    await waitLayoutIdle(page);
    const delay = page.locator("[data-node-id='delay1']");
    const lfo = page.locator("[data-node-id='osc1']");
    await expect(delay).toHaveAttribute("data-face", "delay");
    await expect(lfo).toHaveAttribute("data-face", "lfo");
    await expect(delay.locator("path[data-curve='taps']")).toBeVisible();
    await expect(lfo.locator("path[data-curve='waveform']")).toBeVisible();
    const delayD = await delay.locator("path[data-curve='taps']").getAttribute("d");
    const lfoD = await lfo.locator("path[data-curve='waveform']").getAttribute("d");
    expect(delayD && delayD.length > 8).toBe(true);
    expect(lfoD && lfoD.length > 8).toBe(true);
    expect(delayD).not.toBe(lfoD);
    await expect(lfo.locator("[data-glyph='waveform']")).toHaveAttribute("data-shape", "sine");
    await page.screenshot({ path: path.join(shotDir, "circuit-chorus-delay.png"), fullPage: true });

    await loadPreset(page, "Vocal Comp");
    await page.locator("[data-node-id='comp1']").waitFor();
    await waitLayoutIdle(page);
    const comp = page.locator("[data-node-id='comp1']");
    await expect(comp).toHaveAttribute("data-face", "dynamics");
    await expect(comp.locator("path[data-curve='curve']")).toBeVisible();
    await expect(comp.locator("[data-glyph='curve']")).toHaveAttribute("data-mode", "comp");
    await page.screenshot({ path: path.join(shotDir, "circuit-vocal-comp.png"), fullPage: true });
  });

  test("env pitch and drive faces paint calculated glyphs", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();

    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='octaver1']").waitFor();
    await waitLayoutIdle(page);
    const octaver = page.locator("[data-node-id='octaver1']");
    await expect(octaver).toHaveAttribute("data-face", "pitch");
    await expect(octaver.locator("path[data-curve='intervals']")).toBeVisible();
    await expect(octaver.locator("[data-glyph='intervals']")).toHaveAttribute("data-mode", "octaver");
    await expect(octaver.locator("[data-glyph='intervals']")).toHaveAttribute("aria-label", "calculated pitch intervals");

    await loadPreset(page, "Vocal Comp");
    await page.locator("[data-node-id='stage1']").waitFor();
    await waitLayoutIdle(page);
    const drive = page.locator("[data-node-id='stage1']");
    await expect(drive).toHaveAttribute("data-face", "drive");
    await expect(drive.locator("path[data-curve='transfer']")).toBeVisible();
    await expect(drive.locator("[data-glyph='transfer']")).toHaveAttribute("data-shaper", "diode");
    await expect(drive.locator("[data-glyph='transfer']")).toHaveAttribute("aria-label", "calculated drive transfer");

    await loadPreset(page, "Warehouse Rumble");
    await page.locator("[data-node-id='env1']").waitFor();
    await waitLayoutIdle(page);
    const env = page.locator("[data-node-id='env1']");
    await expect(env).toHaveAttribute("data-face", "env");
    await expect(env.locator("path[data-curve='envelope']")).toBeVisible();
    await expect(env.locator("[data-glyph='envelope']")).toHaveAttribute("data-source", "in");
    await expect(env.locator("[data-glyph='envelope']")).toHaveAttribute("aria-label", "calculated envelope curve");
    const envD = await env.locator("path[data-curve='envelope']").getAttribute("d");
    expect(envD && envD.length > 8).toBe(true);
    await page.screenshot({ path: path.join(shotDir, "circuit-warehouse-rumble.png"), fullPage: true });
  });

  test("routing modfx cab and io faces paint calculated glyphs", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();

    await loadPreset(page, "Phaser Lab");
    await page.locator("[data-node-id='phaser1']").waitFor();
    await waitLayoutIdle(page);
    const phaser = page.locator("[data-node-id='phaser1']");
    await expect(phaser).toHaveAttribute("data-face", "modfx");
    await expect(phaser.locator("path[data-curve='waveform']")).toBeVisible();
    await expect(phaser.locator("[data-glyph='waveform']")).toHaveAttribute("data-mode", "phaser");
    await expect(phaser.locator("[data-glyph='waveform']")).toHaveAttribute("aria-label", "calculated modfx waveform");
    const phaserD = await phaser.locator("path[data-curve='waveform']").getAttribute("d");
    expect(phaserD && phaserD.length > 8).toBe(true);
    await page.screenshot({ path: path.join(shotDir, "circuit-phaser-lab.png"), fullPage: true });

    await loadPreset(page, "Mesa High Gain");
    await page.locator("[data-node-id='ir1']").waitFor();
    await waitLayoutIdle(page);
    const cab = page.locator("[data-node-id='ir1']");
    await expect(cab).toHaveAttribute("data-face", "cab");
    await expect(cab.locator("path[data-curve='mix']")).toBeVisible();
    await expect(cab.locator("[data-glyph='mix']")).toHaveAttribute("data-mix", "0.45");
    await expect(cab.locator("[data-glyph='mix']")).toHaveAttribute("data-slot", "ir1");
    await expect(cab.locator("[data-glyph='mix']")).toHaveAttribute("aria-label", "calculated cabinet mix");

    await loadPreset(page, "Metal Gate");
    await page.locator("[data-node-id='widen1']").waitFor();
    await waitLayoutIdle(page);
    const width = page.locator("[data-node-id='widen1']");
    await expect(width).toHaveAttribute("data-face", "routing");
    await expect(width.locator("path[data-curve='map']")).toBeVisible();
    await expect(width.locator("[data-glyph='map']")).toHaveAttribute("data-mode", "width");
    await expect(width.locator("[data-glyph='map']")).toHaveAttribute("aria-label", "calculated routing map");
    await page.screenshot({ path: path.join(shotDir, "circuit-metal-gate.png"), fullPage: true });

    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='IN']").waitFor();
    await waitLayoutIdle(page);
    const inn = page.locator("[data-node-id='IN']");
    const out = page.locator("[data-node-id='OUT']");
    await expect(inn).toHaveAttribute("data-face", "io");
    await expect(out).toHaveAttribute("data-face", "io");
    await expect(inn.locator("[data-glyph='jack']")).toHaveAttribute("data-outs", "out,sc");
    await expect(inn.locator("path[data-curve='jack']")).toBeVisible();
    await expect(out.locator("[data-glyph='jack']")).toHaveAttribute("aria-label", "calculated io jacks");
    const innJack = inn.locator(".nk-port").first();
    const jackIdle = await boxOf(innJack);
    const innBox = await boxOf(inn);
    await clickChipHeader(page, inn);
    await expect(inn).toHaveAttribute("data-selected", "on");
    assertStableHitbox(innBox, await boxOf(inn), "IN AABB on select");
    assertStableHitbox(jackIdle, await boxOf(innJack), "IN jack on select");
  });

  test("hides unreadably small typecode at overview zoom except on the selected chip", async ({ page }, info) => {
    test.skip(info.project.name !== "chromium-1x");
    test.setTimeout(180000);
    await page.getByRole("button", { name: "Circuit" }).click();
    await page.locator(".nk-board").waitFor();
    await loadPreset(page, "Drone Low Bed");
    await page.locator("[data-node-id='filter1']").waitFor();
    await waitLayoutIdle(page);

    const board = page.locator(".nk-board");
    const box = await board.boundingBox();
    if (! box) {
      throw new Error("board has no box");
    }
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    for (let i = 0; i < 12; i += 1) {
      await page.mouse.wheel(0, 240);
      if (await board.getAttribute("data-zoom") === "overview") {
        break;
      }
    }
    await expect(board).toHaveAttribute("data-zoom", "overview");

    const filter = page.locator("[data-node-id='filter1']");
    const reverb = page.locator("[data-node-id='reverb1']");
    await expect(filter.locator(".nk-chip-title")).toHaveCSS("visibility", "visible");
    await expect(filter.locator(".nk-chip-typecode")).toHaveCSS("visibility", "hidden");
    await expect(reverb.locator(".nk-chip-foot")).toHaveCSS("visibility", "hidden");
    await expect(filter.locator(".nk-port-cap").first()).toHaveCSS("visibility", "hidden");
    await expect(filter.locator("path[data-curve='response']")).toBeVisible();
    const plot = filter.locator(".nk-face-plot");
    await expect(plot).toHaveCSS("background-image", "none");
    await clickChipHeader(page, filter);
    await expect(filter.locator(".nk-chip-typecode")).toHaveCSS("visibility", "visible");
    await expect(filter.locator(".nk-chip-foot")).toHaveCSS("visibility", "visible");
    await expect(filter.locator(".nk-port-cap").first()).toHaveCSS("visibility", "hidden");
    await expect(reverb.locator(".nk-chip-typecode")).toHaveCSS("visibility", "hidden");
    await expect(reverb.locator(".nk-port-cap").first()).toHaveCSS("visibility", "hidden");
  });
});
