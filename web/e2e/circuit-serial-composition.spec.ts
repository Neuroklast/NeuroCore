import { test, expect } from '@playwright/test';
import { writeFileSync } from 'node:fs';

for (const width of [960,1440]) {
  test(`Drone Low Bed has a compact PCB composition at ${width}px`, async ({page},info) => {
    await page.setViewportSize({width,height:860});
    await page.goto('/');
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole('button',{name:'Circuit',exact:true}).click();
    await page.locator('.nk-preset-now').click();
    await page.getByPlaceholder('Search presets, tags, effects...').fill('Drone Low Bed');
    await page.locator('.nk-preset-row',{hasText:'Drone Low Bed'}).dblclick();
    await expect(page.locator('.nk-preset-browser')).toHaveCount(0);
    await expect(page.locator("[aria-label='Layout']")).toHaveCount(0,{timeout:45000});
    const ids=['IN','octaver1','filter1','reverb1','OUT'];
    const dump=await page.evaluate(ids=>ids.map(id=>{
      const el=document.querySelector<HTMLElement>(`[data-node-id='${id}']`)!;
      const rect=(n:Element)=>{const r=n.getBoundingClientRect();return {x:r.x,y:r.y,w:r.width,h:r.height};};
      return {id,box:rect(el),logicalW:el.offsetWidth,
        west:el.querySelector('.nk-port-west')?rect(el.querySelector('.nk-port-west')!):null,
        east:el.querySelector('.nk-port-east')?rect(el.querySelector('.nk-port-east')!):null,
        params:[...el.querySelectorAll<HTMLElement>('.nk-chip-parameters .nk-chip-parameter')].map(n=>({key:n.dataset.bindKey,font:parseFloat(getComputedStyle(n).fontSize),box:rect(n)})),
        pins:[...el.querySelectorAll<HTMLElement>('.nk-port-south')].map(n=>({id:n.dataset.portId,box:rect(n)})),
        faceOpacity:parseFloat(getComputedStyle(el.querySelector('.nk-node-face')!).opacity),
        dotRadius:parseFloat(el.querySelector('.nk-chip-pads')
          ? getComputedStyle(el.querySelector('.nk-chip-pads')!).backgroundImage.match(/\)\s+([\d.]+)px/)?.[1] ?? '0' : '0'),
      };
    }),ids);
    expect(dump.at(-1)!.box.y).toBeGreaterThan(dump[0]!.box.y+dump[0]!.box.h);
    for (let i=0;i<dump.length;i++)for(let j=i+1;j<dump.length;j++) {
      const a=dump[i]!.box,b=dump[j]!.box;
      expect(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y).toBe(true);
    }
    const dsp=dump.slice(1,4), widths=dsp.map(n=>n.logicalW);
    expect(Math.max(...widths)/Math.min(...widths)).toBeLessThanOrEqual(1.25);
    for (const node of dsp) {
      expect(node.faceOpacity).toBeGreaterThanOrEqual(.55);
      expect(node.dotRadius * 2 * node.box.w / node.logicalW, `${node.id} visible dot diameter`).toBeGreaterThanOrEqual(1);
      for (const param of node.params) {
        expect(param.font).toBeGreaterThanOrEqual(15);
        expect(param.font * node.box.w / node.logicalW, `${node.id} visible font`).toBeGreaterThanOrEqual(8);
        const pin=node.pins.find(p=>p.id?.includes(`::${param.key}::`))!;
        expect(pin,param.key).toBeTruthy();
        expect(Math.abs(param.box.x+param.box.w/2-pin.box.x-pin.box.w/2),param.key).toBeLessThan(1);
      }
    }
    const geometryPath=info.outputPath(`geometry-${width}.json`);
    writeFileSync(geometryPath,JSON.stringify(dump,null,2));
    await info.attach('geometry',{path:geometryPath,contentType:'application/json'});
    await page.mouse.move(width-5,855);
    await page.screenshot({path:info.outputPath(`serial-${width}.png`),fullPage:true});
  });
}
