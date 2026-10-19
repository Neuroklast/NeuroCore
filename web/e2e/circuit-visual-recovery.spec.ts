import { test, expect, type Locator, type Page } from '@playwright/test';

async function loadDrone(page: Page) {
  await page.goto('/');
  await page.locator("html[data-nk-ready='1']").waitFor();
  await page.getByRole('button', { name: 'Circuit', exact: true }).click();
  await page.locator('.nk-preset-now').click();
  await page.getByPlaceholder('Search presets, tags, effects...').fill('Drone Low Bed');
  await page.locator('.nk-preset-row', { hasText: 'Drone Low Bed' }).dblclick();
  await expect(page.locator('.nk-preset-browser')).toHaveCount(0);
  await expect(page.locator("[data-node-id='octaver1']")).toBeVisible();
  await expect(page.locator("[aria-label='Layout']")).toHaveCount(0, { timeout: 45000 });
}

async function assertChrome(chip: Locator) {
  const result = await chip.evaluate(el => {
    const selectors = ['.nk-chip-headband', '.nk-chip-subheader', '.nk-node-face', '.nk-chip-parameters', '.nk-chip-foot'];
    const box = (n: Element) => { const r = n.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; };
    const zones = selectors.map(selector => { const n = el.querySelector(selector); return { selector, rect: n ? box(n) : null }; });
    const clipped = [...el.querySelectorAll<HTMLElement>('.nk-chip-title, .nk-chip-typecode, .nk-chip-parameter')]
      .filter(n => n.scrollWidth > n.clientWidth + 1).map(n => n.textContent);
    const hits = [...el.querySelectorAll('.nk-ms, .nk-chip-expand')].map(box);
    const pins = [...el.querySelectorAll('.nk-port')].map(box);
    const frame = el.querySelector('.nk-chip-frame polygon');
    return { zones, clipped, hits, pins, frame: !!frame, size: { w: (el as HTMLElement).offsetWidth, h: (el as HTMLElement).offsetHeight } };
  });
  expect(result.clipped, 'full names and typecodes').toEqual([]);
  expect(result.frame, 'cut-corner IC frame').toBe(true);
  for (const zone of result.zones) expect(zone.rect, zone.selector).not.toBeNull();
  const overlaps = (a: NonNullable<typeof result.zones[0]['rect']>, b: typeof a) => a.x < b.x+b.w-.5 && a.x+a.w > b.x+.5 && a.y < b.y+b.h-.5 && a.y+a.h > b.y+.5;
  for (let i=0; i<result.zones.length; i++) for(let j=i+1; j<result.zones.length; j++) {
    expect(overlaps(result.zones[i]!.rect!, result.zones[j]!.rect!), `${result.zones[i]!.selector} vs ${result.zones[j]!.selector}`).toBe(false);
  }
  for (let i=0; i<result.hits.length; i++) for(let j=i+1; j<result.hits.length; j++) expect(overlaps(result.hits[i]!, result.hits[j]!)).toBe(false);
  for (let i=0; i<result.pins.length; i++) for(let j=i+1; j<result.pins.length; j++) expect(overlaps(result.pins[i]!, result.pins[j]!)).toBe(false);
  await expect(chip.locator('.nk-chip-pads')).toBeVisible();
  return result.size;
}

test('Drone chips retain identity and exclusive chrome at both pixel densities', async ({ page }, info) => {
  test.setTimeout(180000);
  await loadDrone(page);
  await page.screenshot({ path: `test-results/recovery-${info.project.name}.png`, fullPage: true });
  for (const [id, name] of [['octaver1','OCTAVER'], ['filter1','FILTER'], ['reverb1','REVERB']]) {
    const chip = page.locator(`[data-node-id='${id}']`);
    await expect(chip.locator('.nk-chip-title')).toHaveText(name!);
    const before = await assertChrome(chip);
    await chip.locator('.nk-chip-title').click();
    expect(await assertChrome(chip), 'selection cannot resize a chip').toEqual(before);
  }
  await page.mouse.move(1250, 850);
  await page.screenshot({ path: `test-results/recovery-final-${info.project.name}.png`, fullPage: true });
});
