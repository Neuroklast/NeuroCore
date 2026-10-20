import { test, expect } from "@playwright/test";
import { writeFileSync } from "node:fs";
for (const width of [960, 1440])
  test(`Circuit navigation owns a stable control band at ${width}px`, async ({
    page,
  }, info) => {
    test.setTimeout(120000);
    await page.setViewportSize({ width, height: 860 });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    const nav = page.getByRole("navigation", { name: "Circuit navigation" });
    await expect(nav).toBeVisible();
    const geometry = async () =>
      nav.locator("button,input").evaluateAll((els) =>
        els.map((el) => {
          const r = el.getBoundingClientRect();
          return {
            label: el.getAttribute("aria-label") ?? el.textContent,
            x: r.x,
            y: r.y,
            w: r.width,
            h: r.height,
          };
        }),
      );
    const before = await geometry();
    await nav.getByPlaceholder("Find chip…").fill("filter");
    expect(await geometry()).toEqual(before);
    const b = await page.locator(".nk-board").boundingBox();
    for (const r of before) {
      expect(r.h).toBeGreaterThanOrEqual(26);
      expect(r.x).toBeGreaterThanOrEqual(b!.x);
      expect(r.x + r.w).toBeLessThanOrEqual(b!.x + b!.width + 1);
    }
    writeFileSync(
      info.outputPath("navigation-geometry.json"),
      JSON.stringify(before, null, 2),
    );
    await page.screenshot({
      path: info.outputPath(`navigation-${width}.png`),
      fullPage: true,
    });
  });

for (const count of [20, 100, 200])
  test(`large patch ${count} exposes bounded regions and readable real chips`, async ({
    page,
  }, info) => {
    test.setTimeout(180000);
    await page.setViewportSize({
      width: info.project.name.endsWith("2x") ? 1440 : 960,
      height: 860,
    });
    await page.addInitScript(() => {
      const listeners = new Map<string, ((p: unknown) => void)[]>();
      const emit = (id: string, p: unknown) =>
        listeners.get(id)?.forEach((f) => f(p));
      window.addEventListener("circuit-contract-ast", (e) =>
        emit("ast", (e as CustomEvent).detail),
      );
      window.addEventListener("circuit-contract-host", (e) =>
        emit("host", (e as CustomEvent).detail),
      );
      window.__JUCE__ = {
        backend: {
          addEventListener(id, fn) {
            const list = listeners.get(id) ?? [];
            list.push(fn);
            listeners.set(id, list);
          },
          emitEvent(id, payload) {
            if (id !== "__juce__invoke") return;
            const c = payload as { name: string; resultId: number };
            queueMicrotask(() => {
              if (c.name === "UI_READY")
                emit("host", {
                  sr: 48000,
                  buf: 256,
                  theme: "signal",
                  motion: "full",
                  frameRate: 30,
                });
              emit("__juce__complete", {
                promiseId: c.resultId,
                result: { ok: true },
              });
            });
          },
        },
      };
    });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    await page.evaluate((count) => {
      const nodes = Array.from({ length: count }, (_, i) => ({
        id: `filter${i + 1}`,
        type: "filter",
        busName: "",
        args: { type: "lowpass", cutoff: "1200", resonance: ".7" },
        trailingComment: "",
      }));
      const ids = ["IN", ...nodes.map((n) => n.id), "OUT"];
      const edges = ids.slice(1).map((id, i) => ({
        from: ids[i],
        to: id,
        kind: "audio",
        fromJack: "out",
        toJack: "in",
      }));
      window.dispatchEvent(
        new CustomEvent("circuit-contract-ast", {
          detail: {
            origin: "host",
            script: nodes.map((n) => `${n.id}: filter(cutoff=1200)`).join("\n"),
            astJson: JSON.stringify({
              version: 1,
              leadingComments: [],
              params: [],
              nodes,
              edges,
            }),
            diagnostics: [],
          },
        }),
      );
    }, count);
    await expect(
      page.getByRole("region", { name: "Board regions" }),
    ).toBeVisible();
    await expect(page.locator("[aria-label='Layout']")).toHaveCount(0, {
      timeout: 45000,
    });
    const overview = page.locator(".nk-board-regions");
    const cards = await overview.locator("article").count();
    expect(cards).toBeLessThanOrEqual(6);
    expect(await page.locator(".nk-board-world [data-node-id]").count()).toBe(
      0,
    );
    const allCards = await overview.locator("article").evaluateAll((els) =>
      els.map((el) => {
        const r = el.getBoundingClientRect();
        return { x: r.x, y: r.y, w: r.width, h: r.height };
      }),
    );
    for (let i = 0; i < allCards.length; i++)
      for (let j = i + 1; j < allCards.length; j++) {
        const a = allCards[i]!,
          b = allCards[j]!;
        expect(
          a.x + a.w <= b.x + 1 ||
            b.x + b.w <= a.x + 1 ||
            a.y + a.h <= b.y + 1 ||
            b.y + b.h <= a.y + 1,
        ).toBe(true);
      }
    await overview
      .getByPlaceholder("Name region")
      .first()
      .fill("Input texture");
    await page.screenshot({
      path: info.outputPath(`overview-${count}.png`),
      fullPage: true,
    });
    let depth = 0;
    while (await page.getByRole("region", { name: "Board regions" }).count()) {
      await page.locator(".nk-region-open").first().click();
      expect(++depth).toBeLessThan(6);
    }
    const chips = page.locator(".nk-board-world [data-node-id]");
    expect(await chips.count()).toBeGreaterThan(0);
    expect(await chips.count()).toBeLessThanOrEqual(3);
    const dom = await chips.evaluateAll((els) =>
      els.map((el) => {
        const r = el.getBoundingClientRect();
        return {
          id: el.getAttribute("data-node-id"),
          x: r.x,
          y: r.y,
          h: r.height,
          w: r.width,
          logicalW: (el as HTMLElement).offsetWidth,
        };
      }),
    );
    for (const chip of dom)
      expect((15 * chip.w) / chip.logicalW).toBeGreaterThanOrEqual(8);
    const boardBox = await page.locator(".nk-board").boundingBox();
    for (const chip of dom) {
      expect(chip.x + chip.w).toBeGreaterThan(boardBox!.x);
      expect(chip.x).toBeGreaterThanOrEqual(boardBox!.x);
      expect(chip.x + chip.w).toBeLessThanOrEqual(
        boardBox!.x + boardBox!.width,
      );
      expect(chip.y + chip.h).toBeGreaterThan(boardBox!.y + 40);
      expect(chip.y).toBeGreaterThanOrEqual(boardBox!.y + 40);
      expect(chip.y + chip.h).toBeLessThanOrEqual(
        boardBox!.y + boardBox!.height - 40,
      );
    }
    const before = await page.evaluate(() => window.__nkBoardCamera);
    await page.getByRole("button", { name: "Unit", exact: true }).click();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    await page.evaluate(
      () =>
        new Promise<void>((resolve) =>
          requestAnimationFrame(() => requestAnimationFrame(() => resolve())),
        ),
    );
    expect(await page.evaluate(() => window.__nkBoardCamera)).toEqual(before);
    const shown = await chips.first().boundingBox();
    expect(shown!.x + shown!.width).toBeGreaterThan(boardBox!.x);
    expect(shown!.y).toBeLessThan(boardBox!.y + boardBox!.height);
    if (count === 20) {
      const signal = await page.evaluate(async () => {
        const canvas =
          document.querySelector<HTMLCanvasElement>(".nk-board-canvas")!;
        const ink = () => {
          const data = canvas
            .getContext("2d")!
            .getImageData(0, 0, canvas.width, canvas.height).data;
          let value = 0;
          for (let i = 0; i < data.length; i += 4)
            value +=
              ((data[i]! + data[i + 1]! + data[i + 2]!) * data[i + 3]!) / 255;
          return value;
        };
        const emit = (level: number) =>
          window.dispatchEvent(
            new CustomEvent("circuit-contract-host", {
              detail: {
                motion: "full",
                sr: 48000,
                buf: 256,
                cpu: 20,
                bpm: 120,
                os: 4,
                clips: ["IN", "filter1", "filter2"].map((id) => ({
                  id,
                  peak: level,
                  rms: level * 0.4,
                  peaks: level ? [0.15, 0.6] : [],
                })),
              },
            }),
          );
        emit(0);
        await new Promise((r) => setTimeout(r, 100));
        const silent = ink();
        const timer = setInterval(() => emit(0.8), 40);
        emit(0.8);
        await new Promise((r) => setTimeout(r, 300));
        const held = ink();
        clearInterval(timer);
        await new Promise((r) => setTimeout(r, 650));
        const stale = ink();
        emit(0);
        await new Promise((r) => setTimeout(r, 100));
        const released = ink();
        return { silent, held, stale, released };
      });
      expect(signal.held).toBeGreaterThan(signal.silent * 1.05);
      expect(signal.stale).toBeLessThan(signal.held);
      expect(Math.abs(signal.released - signal.silent)).toBeLessThan(
        signal.silent * 0.05 + 1,
      );
      writeFileSync(
        info.outputPath("signal-pixels.json"),
        JSON.stringify(signal, null, 2),
      );
    }
    if (count === 20) {
      await page.evaluate(() => {
        window.dispatchEvent(
          new CustomEvent("circuit-contract-host", {
            detail: {
              motion: "full",
              sr: 48000,
              buf: 256,
              cpu: 20,
              bpm: 120,
              os: 4,
              clips: ["IN", "filter1", "filter2"].map((id) => ({
                id,
                peak: 0.8,
                rms: 0.32,
                peaks: [0.15, 0.6],
              })),
            },
          }),
        );
        return new Promise<void>((resolve) =>
          requestAnimationFrame(() => requestAnimationFrame(() => resolve())),
        );
      });
      await page.screenshot({
        path: info.outputPath("signal-active.png"),
        fullPage: true,
      });
    }
    await page.screenshot({
      path: info.outputPath(`detail-${count}.png`),
      fullPage: true,
    });
    await page
      .getByRole("navigation", { name: "Circuit navigation" })
      .getByRole("button", { name: "Board", exact: true })
      .click();
    await expect(
      page.getByRole("region", { name: "Board regions" }),
    ).toBeVisible();
    await expect(page.getByPlaceholder("Name region").first()).toHaveValue(
      "Input texture",
    );
    const search = page.getByPlaceholder("Find chip…");
    await search.fill(`filter${count}`);
    await page
      .getByRole("option")
      .filter({ hasText: `filter${count}` })
      .first()
      .click();
    await expect(page.locator(`[data-node-id='filter${count}']`)).toBeVisible();
    writeFileSync(
      info.outputPath("large-patch.json"),
      JSON.stringify({ count, cards, depth, dom, camera: before }, null, 2),
    );
  });
