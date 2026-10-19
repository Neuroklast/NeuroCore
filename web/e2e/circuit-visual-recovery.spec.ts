import { test, expect, type Locator, type Page } from '@playwright/test';

async function loadDrone(page: Page, name = "Drone Low Bed") {
  await page.goto('/');
  await page.locator("html[data-nk-ready='1']").waitFor();
  await page.getByRole('button', { name: 'Circuit', exact: true }).click();
  await page.locator('.nk-preset-now').click();
  await page.getByPlaceholder('Search presets, tags, effects...').fill(name);
  await page.locator('.nk-preset-row', { hasText: name }).dblclick();
  await expect(page.locator('.nk-preset-browser')).toHaveCount(0);
  await expect(page.locator(".nk-board-chip").first()).toBeVisible();
  await expect(page.locator("[aria-label='Layout']")).toHaveCount(0, { timeout: 45000 });
}

async function assertChrome(chip: Locator) {
  const result = await chip.evaluate(el => {
    const selectors = ['.nk-chip-headband', '.nk-chip-subheader', '.nk-node-face', '.nk-chip-parameters', '.nk-chip-foot'];
    const box = (n: Element) => { const r = n.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; };
    const zones = selectors.map(selector => { const n = el.querySelector(selector); return { selector, rect: n ? box(n) : null }; });
    const clipped = [...el.querySelectorAll<HTMLElement>('.nk-chip-title, .nk-chip-typecode, .nk-chip-parameter')]
      .filter(n => n.scrollWidth > n.clientWidth + 1).map(n => n.textContent);
    const captions = [...el.querySelectorAll('.nk-chip-parameter')].map(box);
    const hits = [...el.querySelectorAll('.nk-ms, .nk-chip-expand')].map(box);
    const pins = [...el.querySelectorAll('.nk-port')].map(box);
    const frame = el.querySelector('.nk-chip-frame polygon');
    return { zones, clipped, captions, hits, pins, frame: !!frame, size: { w: (el as HTMLElement).offsetWidth, h: (el as HTMLElement).offsetHeight } };
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
  for (let i=0; i<result.captions.length; i++) for(let j=i+1; j<result.captions.length; j++) expect(overlaps(result.captions[i]!, result.captions[j]!), 'parameter captions').toBe(false);
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
    await chip.click({ position: { x: 50, y: 20 } });
    expect(await assertChrome(chip), 'selection cannot resize a chip').toEqual(before);
  }
  const octaver = page.locator("[data-node-id='octaver1']");
  await expect(octaver.locator('.nk-port-east')).toHaveAttribute('data-signal', 'on');
  for (const pin of await octaver.locator('.nk-port-south').all()) await expect(pin).toHaveAttribute('data-signal', 'off');
  await page.mouse.move(1250, 850);
  await page.screenshot({ path: `test-results/recovery-final-${info.project.name}.png`, fullPage: true });
});


test('chip geometry survives all themes, zoom and repeated window resizing', async ({ page }, info) => {
  test.setTimeout(180000);
  await loadDrone(page);
  const chip = page.locator("[data-node-id='octaver1']");
  const before = await assertChrome(chip);
  for (const theme of ['Signal', 'Gold', 'Azure', 'DIGICIDE']) {
    await page.getByRole('button', { name: 'Settings', exact: true }).click();
    await page.getByRole('button', { name: theme, exact: true }).click();
    await page.keyboard.press('Escape');
    expect(await assertChrome(chip)).toEqual(before);
    await page.screenshot({ path: info.outputPath(`drone-${theme}.png`), fullPage: true });
  }
  for (const size of [{width:960,height:645},{width:1600,height:1075},{width:1280,height:860}]) {
    await page.setViewportSize(size);
    await page.evaluate(() => new Promise<void>(resolve => requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
    expect(await assertChrome(chip), 'resize cannot alter logical chip geometry').toEqual(before);
  }
  for (const delta of [1000,-1000,300]) {
    await page.mouse.move(40,450);
    await page.mouse.wheel(0,delta);
    await page.evaluate(() => new Promise<void>(resolve => requestAnimationFrame(() => requestAnimationFrame(() => resolve()))));
    expect(await assertChrome(chip), 'zoom changes paint, not logical dimensions').toEqual(before);
  }
});

test('Pad Cloud Hall and a many-port dynamics patch keep independent chip zones', async ({ page }, info) => {
  test.setTimeout(180000);
  for (const preset of ['Pad Cloud Hall','Bass Fractionator']) {
    await loadDrone(page, preset);
    for (const chip of await page.locator('.nk-chip').all()) await assertChrome(chip);
    await page.screenshot({ path: info.outputPath(`${preset.replaceAll(' ', '-').toLowerCase()}.png`), fullPage: true });
  }
});
