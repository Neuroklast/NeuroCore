import { installUnitHost } from './unit-host';
import { test, expect } from '@playwright/test';
import { assertContainedWithin, assertNoOverlap, assertStableHitbox, type Box } from './layout';

test('resizing the same Unit keeps HUD anchors, contents and callouts in logical coordinates', async ({ page }, testInfo) => {
  await installUnitHost(page);
  await page.goto('/');
  await page.locator("html[data-nk-ready='1']").waitFor();
  await expect(page.locator(".nk-face-hud")).toHaveCSS("display", "grid");
  await page.evaluate(() => document.fonts.ready);
  let reference: Box[] | undefined;
  for (const [width, height] of [[1280,860],[960,645],[1440,968],[1100,800],[1600,1000],[1280,860]]) {
    await page.setViewportSize({ width, height });
    await page.evaluate(() => new Promise<void>(resolve => requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
    // Wait for the measured diagnostics and their lower-frequency readout paint.
    await page.waitForTimeout(120);
    const geometry = await page.evaluate(() => {
      const face = document.querySelector<HTMLElement>('.nk-face')!;
      const root = face.getBoundingClientRect();
      const scale = root.width / face.clientWidth;
      const box = (el: Element) => {
        const r=el.getBoundingClientRect();
        return { x:(r.x-root.x)/scale, y:(r.y-root.y)/scale, w:r.width/scale, h:r.height/scale };
      };
      return {
        root: { x:0,y:0,w:face.clientWidth,h:face.clientHeight },
        anchors: [...face.querySelectorAll('.nk-face-tele, .nk-face-diag, .nk-face-callout:not([hidden])')].map(box),
        panels: [...face.querySelectorAll('.nk-face-mod')].map(el => ({ box:box(el), children:[...el.children].map(box) })),
        calls: [...face.querySelectorAll('.nk-face-callout:not([hidden])')].map(box),
        reserved: [...face.querySelectorAll('.nk-face-tele, .nk-face-diag')].map(box),
        barsVisible: [...face.querySelectorAll('.nk-face-seg i')].every(el => {
          let p: Element|null=el;
          while(p && p !== face) { if(getComputedStyle(p).opacity==='0') return false; p=p.parentElement; }
          return el.getBoundingClientRect().height>0;
        }),
      };
    });
    expect(geometry.barsVisible).toBe(true);
    for(const panel of geometry.panels) {
      assertContainedWithin(panel.box,geometry.root,'panel within Unit');
      for(const child of panel.children) assertContainedWithin(child,panel.box,'content within its panel');
    }
    for(const call of geometry.calls) {
      assertContainedWithin(call,geometry.root,'callout within Unit');
      for(const reserved of geometry.reserved) assertNoOverlap(call,reserved,'callout vs HUD');
    }
    if(reference) {
      expect(geometry.anchors.length).toBe(reference.length);
      geometry.anchors.forEach((box,i)=>assertStableHitbox(reference![i],box,'logical HUD anchor'));
    } else reference=geometry.anchors;
    await page.screenshot({ path:testInfo.outputPath(`unit-${width}x${height}.png`) });
  }
});

for(const theme of ['signal','gold','azure','digicide']) {
  test(`all Unit panels fit in ${theme}`, async ({ page },testInfo)=>{
    await installUnitHost(page,theme);
    await page.goto('/');
    await page.locator("html[data-nk-ready='1']").waitFor();
    await expect(page.locator(".nk-face-hud")).toHaveCSS("display", "grid");
    await expect(page.locator(".nk-face-mod")).toHaveCount(6);
    await page.evaluate(()=>document.fonts.ready);
    for(const panel of await page.locator('.nk-face-mod').all()) {
      const clipped=await panel.evaluate(el=>{
        const host=el.getBoundingClientRect();
        return [...el.children].some(child=>{
          const r=child.getBoundingClientRect();
          return r.top<host.top-.5 || r.bottom>host.bottom+.5 || r.left<host.left-.5 || r.right>host.right+.5;
        });
      });
      expect(clipped).toBe(false);
    }
    await page.screenshot({path:testInfo.outputPath(`unit-${theme}.png`)});
  });
}

test('scaled knob menus and bind cursor follow the pointer', async ({page})=>{
  await installUnitHost(page);
  await page.setViewportSize({width:960,height:645});
  await page.goto('/');
  await page.locator("html[data-nk-ready='1']").waitFor();
  await expect(page.locator('.nk-face-hud')).toHaveCSS('display','grid');
  await page.getByRole('button',{name:'Circuit',exact:true}).click();
  const bind=page.locator('[data-knob-bind="a"]').first();
  await expect(bind).toBeVisible();
  const r=await bind.boundingBox();
  await page.mouse.move(r!.x+r!.width/2,r!.y+r!.height/2);
  await page.mouse.down();
  await page.mouse.move(450,360);
  const ghost=page.locator('.nk-bind-ghost');
  await expect(ghost).toBeVisible();
  const g=await ghost.boundingBox();
  expect(g!.x+g!.width/2).toBeCloseTo(450,0);
  expect(g!.y+g!.height/2).toBeCloseTo(360,0);
  await page.mouse.up();
});
