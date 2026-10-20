import { writeFileSync } from "node:fs";
import { test, expect } from "@playwright/test";
for (const count of [6, 20, 100, 200])
  test(`PCB keeps all ${count} effects on one surface`, async ({
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

    await expect(page.locator('[aria-label="Layout"]')).toHaveCount(0, {
      timeout: 90000,
    });
    const chips = page.locator(".nk-board-world [data-node-id]");
    await expect(chips).toHaveCount(count + 2);
    await expect(page.locator(".nk-board-regions")).toHaveCount(0);
    const bounds = await chips.evaluateAll((els) =>
      els.map((el) => {
        const r = el.getBoundingClientRect();
        return { x: r.x, y: r.y, w: r.width, h: r.height };
      }),
    );
    const board = await page.locator(".nk-board").boundingBox();
    const occupiedW=Math.max(...bounds.map(r=>r.x+r.w))-Math.min(...bounds.map(r=>r.x));
    const occupiedH=Math.max(...bounds.map(r=>r.y+r.h))-Math.min(...bounds.map(r=>r.y));
    expect(Math.max(occupiedW/board!.width,occupiedH/(board!.height-36)), 'Fit must use the pane, not a stale pre-layout camera').toBeGreaterThan(.75);
    for (const r of bounds) {
      expect(r.x).toBeGreaterThanOrEqual(board!.x);
      expect(r.y).toBeGreaterThanOrEqual(board!.y + 36);
      expect(r.x + r.w).toBeLessThanOrEqual(board!.x + board!.width + 1);
      expect(r.y + r.h).toBeLessThanOrEqual(board!.y + board!.height + 1);
    }
    for (let i = 0; i < bounds.length; i++)
      for (let j = i + 1; j < bounds.length; j++) {
        const a = bounds[i]!,
          b = bounds[j]!;
        expect(
          a.x + a.w <= b.x + 1 ||
            b.x + b.w <= a.x + 1 ||
            a.y + a.h <= b.y + 1 ||
            b.y + b.h <= a.y + 1,
        ).toBe(true);
      }
    await page.screenshot({
      path: info.outputPath(`pcb-${count}.png`),
      fullPage: true,
    });
    const search = page.getByPlaceholder("Find chip…");
    await search.fill(`filter${count}`);
    await page
      .getByRole("option")
      .filter({ hasText: `filter${count}` })
      .first()
      .click();
    await expect(chips).toHaveCount(count + 2);
    const found = await page
      .locator(`[data-node-id='filter${count}']`)
      .boundingBox();
    expect(found!.width).toBeGreaterThan(120);
    const camera = await page.evaluate(() => window.__nkBoardCamera);
    await page.getByRole("button", { name: "Unit", exact: true }).click();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    expect(await page.evaluate(() => window.__nkBoardCamera)).toEqual(camera);
    await page.getByRole("button", { name: "Fit board", exact: true }).click();
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
      expect(signal.held).toBeGreaterThan(signal.silent);
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

    writeFileSync(
      info.outputPath("pcb-geometry.json"),
      JSON.stringify({ count, bounds, camera }, null, 2),
    );
  });

for (const name of ["Wide Motion", "Bass Regen Growl", "Multiband Punch"])
  test(`factory PCB ${name}`, async ({ page }, info) => {
    test.setTimeout(120000);
    await page.setViewportSize({
      width: info.project.name.endsWith("2x") ? 1440 : 960,
      height: 860,
    });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    await page.locator(".nk-preset-now").click();
    await page.getByPlaceholder("Search presets, tags, effects...").fill(name);
    await page.locator(".nk-preset-row", { hasText: name }).first().dblclick();
    await expect(page.locator(".nk-preset-browser")).toHaveCount(0);
    await expect(page.locator('[aria-label="Layout"]')).toHaveCount(0, {
      timeout: 90000,
    });
    await expect(page.locator(".nk-board-regions")).toHaveCount(0);
    const chips = page.locator(".nk-board-world [data-node-id]");
    expect(await chips.count()).toBeGreaterThan(3);
    const boxes = await chips.evaluateAll((els) =>
      els.map((el) => {
        const r = el.getBoundingClientRect();
        return {
          id: el.getAttribute("data-node-id"),
          x: r.x,
          y: r.y,
          w: r.width,
          h: r.height,
        };
      }),
    );
    const board = (await page.locator(".nk-board").boundingBox())!;
    for (const r of boxes) {
      expect(r.x).toBeGreaterThanOrEqual(board.x);
      expect(r.y).toBeGreaterThanOrEqual(board.y + 36);
      expect(r.x + r.w).toBeLessThanOrEqual(board.x + board.width + 1);
      expect(r.y + r.h).toBeLessThanOrEqual(board.y + board.height + 1);
    }
    for (let i = 0; i < boxes.length; i++)
      for (let j = i + 1; j < boxes.length; j++) {
        const a = boxes[i]!,
          b = boxes[j]!;
        expect(
          a.x + a.w <= b.x + 1 ||
            b.x + b.w <= a.x + 1 ||
            a.y + a.h <= b.y + 1 ||
            b.y + b.h <= a.y + 1,
        ).toBe(true);
      }
    await page.mouse.move(0, 0);
    await page.screenshot({
      path: info.outputPath("factory-pcb.png"),
      fullPage: true,
    });
    writeFileSync(
      info.outputPath("factory-geometry.json"),
      JSON.stringify({ name, boxes }, null, 2),
    );
  });
