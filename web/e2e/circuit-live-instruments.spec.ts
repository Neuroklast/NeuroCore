import { test, expect } from "@playwright/test";

const script =
  "stepseq1: steps = 4; rate = 1; gate = 1; mix = 1; values = 0.5, -0.5, 0.25, 0";

test.describe("step sequencer editor", () => {
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      const listeners = new Map<string, ((p: unknown) => void)[]>();
      const emit = (id: string, p: unknown) => listeners.get(id)?.forEach((f) => f(p));
      window.addEventListener("circuit-contract-ast", (e) =>
        emit("ast", (e as CustomEvent).detail),
      );
      window.addEventListener("unit-contract-host", (e) =>
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
            const call = payload as { name: string; resultId: number };
            queueMicrotask(() => {
              if (call.name === "UI_READY") {
                emit("host", { sr: 48000, buf: 256, theme: "signal", motion: "off", frameRate: 30 });
              }
              emit("__juce__complete", { promiseId: call.resultId, result: { ok: true } });
            });
          },
        },
      };
    });
    await page.goto("/");
    await page.locator("html[data-nk-ready='1']").waitFor();
    await page.getByRole("button", { name: "Circuit", exact: true }).click();
    await page.evaluate((source) => {
      window.dispatchEvent(new CustomEvent("circuit-contract-ast", {
        detail: {
          origin: "host",
          script: source,
          astJson: JSON.stringify({
            version: 1,
            leadingComments: [],
            params: [],
            nodes: [{
              id: "stepseq1",
              type: "stepseq",
              busName: "",
              args: {
                steps: "64",
                rate: "1",
                gate: "1",
                mix: "1",
                values: "0.5,-0.5,0.25,0",
              },
              trailingComment: "",
            }],
            edges: [],
          }),
          diagnostics: [],
        },
      }));
    }, script);
    await page.locator("[data-node-id='stepseq1']").waitFor();
  });


  test("LED matrix stays in the face and uses the theme accent", async ({page}) => {
    const chip = page.locator("[data-node-id='stepseq1']");
    const leds = chip.locator(".nk-step-leds");
    await expect(leds).toBeVisible();
    await page.evaluate(() => window.dispatchEvent(new CustomEvent("unit-contract-host", {
      detail: { theme:"gold", clips:[{id:"stepseq1", peak:0, step:1}] }
    })));
    const active = leds.locator("[data-active='on']");
    await expect(active).toHaveCount(1);
    const geometry = await leds.evaluate(el => {
      const b=el.getBoundingClientRect(), parent=el.parentElement!.getBoundingClientRect();
      const style=getComputedStyle(el);
      return {columns:style.gridTemplateColumns.split(" ").length,
        inside:b.x>=parent.x && b.y>=parent.y && b.right<=parent.right+1 && b.bottom<=parent.bottom+1};
    });
    expect(geometry).toEqual({columns:8,inside:true});
    const color = await active.evaluate(el => ({
      led:getComputedStyle(el).backgroundColor,
      accent:getComputedStyle(document.documentElement).getPropertyValue("--nk-accent").trim()
    }));
    expect(color.led).toBe("rgb(252, 238, 10)");
  });
});
