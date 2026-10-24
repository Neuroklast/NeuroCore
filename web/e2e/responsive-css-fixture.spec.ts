import { readFileSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { test, expect } from "@playwright/test";

const here = path.dirname(fileURLToPath(import.meta.url));
const cssDir = path.resolve(here, "../dist/assets");
const cssName = readdirSync(cssDir).find((name) => name.endsWith(".css"));
if (!cssName) throw new Error("Run npm run build before the CSS fixture test.");
const productionCss = readFileSync(path.join(cssDir, cssName), "utf8");

/* Minimal DOM/CSS rendering, deliberately without the expensive live BallGl
 * visualizer. Confirms production CSS geometry rather than source text alone.
 * A full native JUCE screenshot audit remains a separate release gate. */
const tools = `
<button class="nk-brand mr-2 flex shrink-0 items-center gap-3" style="width:245px;height:44px" aria-label="Neurocore">NEUROCORE</button>
<div class="nk-preset-cluster"><button class="nk-clip">‹</button><button class="nk-preset-now">Mesa High Gain</button><button class="nk-clip">›</button></div>
<div class="nk-header-right">
  <div class="nk-header-tools"><button class="nk-clip">Functions</button><button class="nk-clip">Stages</button><button class="nk-clip">Settings</button><button class="nk-clip">Help</button></div>
  <div class="nk-header-primary"><div class="nk-mode-pair"><button>LIVE</button><button>STUDIO</button></div><button class="nk-clip">DRY</button></div>
</div>`;
const read = async (page: import("@playwright/test").Page, selectors: string[]) =>
  page.evaluate((sels) => sels.map((selector) => {
    const el = document.querySelector(selector) as HTMLElement | null;
    if (!el) throw new Error("Missing: " + selector);
    const r = el.getBoundingClientRect();
    return { selector, x: r.x, y: r.y, w: r.width, h: r.height,
      overflowY: getComputedStyle(el).overflowY,
      scrollH: el.scrollHeight, clientH: el.clientHeight,
      position: getComputedStyle(el).position };
  }), selectors);

test("short wide host keeps a single toolbar and usable canvas", async ({ page }) => {
  const width = 1600, height = 700;
  await page.setViewportSize({ width, height });
  await page.setContent(`<div class="nk-scale-stage" data-layout="wide" data-height="short" style="width:${width}px;height:${height}px">
  <main class="nk-os relative flex h-full w-full flex-col">
  <header class="nk-toolbar flex h-[52px] w-full min-w-0 shrink-0 items-center gap-2 overflow-hidden px-3">${tools}</header>
  <div class="nk-frame min-h-0 flex-1"></div>
  <div class="nk-knob-rail flex h-[160px] shrink-0"></div>
  </main></div>`);
  await page.addStyleTag({ content: productionCss });
  const [header, right, frame, knobs] = await read(page, [".nk-toolbar",".nk-header-right",".nk-frame",".nk-knob-rail"]);
  expect(header.h).toBe(52);
  expect(right.y + right.h).toBeLessThanOrEqual(header.y + header.h + 1);
  expect(frame.h).toBeGreaterThan(400);
  expect(knobs.h).toBe(128);
});

for (const { width, height, mode } of [
  { width: 1100, height: 740, mode: "compact" },
  { width: 900, height: 600, mode: "tight" },
]) {
  test(`compact toolbar is not cropped at ${width}x${height}`, async ({ page }) => {
    await page.setViewportSize({ width, height });
    await page.setContent(`<div class="nk-scale-stage" data-layout="${mode}" data-height="short" style="width:${width}px;height:${height}px">
      <header class="nk-toolbar flex h-[52px] w-full min-w-0 shrink-0 items-center gap-2 overflow-hidden px-3">${tools}</header>
      </div>`);
    await page.addStyleTag({ content: productionCss });
    const [header, brand, preset, right] = await read(page, [".nk-toolbar",".nk-brand",".nk-preset-cluster",".nk-header-right"]);
    expect(header.h).toBeGreaterThanOrEqual(88);
    expect(right.y).toBeGreaterThan(brand.y);
    expect(right.y + right.h).toBeLessThanOrEqual(header.y + header.h + 1);
    expect(preset.x + preset.w).toBeLessThanOrEqual(width + 1);
    expect(right.x + right.w).toBeLessThanOrEqual(width + 1);
  });
}

test("tight Unit visualizers scroll individually instead of clipping charts", async ({ page }) => {
  await page.setViewportSize({ width: 900, height: 600 });
  const panels = Array.from({length:3}, (_, i) => `<div class="nk-face-mod"><div class="nk-face-mod-head">SCOPE ${i}</div><svg class="nk-face-mod-svg" viewBox="0 0 100 50"><path d="M0 10 L100 40"/></svg></div>`).join("");
  await page.setContent(`<div class="nk-scale-stage" data-layout="tight" data-height="short">
    <section class="nk-face relative" style="width:900px;height:230px;">
      <div class="nk-face-hud"><aside class="nk-face-tele nk-face-tele-l">INPUT LEVEL</aside>
      <aside class="nk-face-tele nk-face-tele-r">OUTPUT LEVEL</aside>
      <aside class="nk-face-diag nk-face-diag-l">${panels}</aside>
      <aside class="nk-face-diag nk-face-diag-r">${panels}</aside></div>
    </section></div>`);
  await page.addStyleTag({ content: productionCss });
  for(const selector of [".nk-face-diag-l",".nk-face-diag-r"]) {
    const [box] = await read(page,[selector]);
    expect(box.overflowY).toBe("auto");
    expect(box.scrollH).toBeGreaterThan(box.clientH);
    const moved=await page.locator(selector).evaluate((node) => {
      node.scrollTop=100; return node.scrollTop;
    });
    expect(moved).toBeGreaterThan(0);
  }
});

test("compact footer pins license information while remaining scrollable", async ({ page }) => {
  await page.setViewportSize({ width:900,height:600 });
  const labels=["STUDIO","CPU 034%","LAT 12.6ms/512smp","SR 48000","BUF 1024","BPM 160.0","Host tempo","OS 4x","LICENSED"];
  await page.setContent(`<div class="nk-scale-stage" data-layout="tight">
    <footer class="nk-foot h-8 shrink-0 items-center border-t px-3 font-mono text-[12px]" style="width:900px;">
    ${labels.map(v=>`<span>${v}</span>`).join("")}</footer></div>`);
  await page.addStyleTag({ content: productionCss });
  const [footer,license] = await read(page, [".nk-foot",".nk-foot > span:last-child"]);
  expect(footer.w).toBe(900);
  expect(license.position).toBe("sticky");
  expect(license.x+license.w).toBeLessThanOrEqual(901);
});
