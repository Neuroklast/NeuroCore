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
            }, {id:"meter1",type:"meter",busName:"",args:{mode:"peak"},trailingComment:""}],
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
    await expect(leds.locator("[data-step-tick]")).toHaveCount(64);
    const color = await active.evaluate(el => ({
      led:getComputedStyle(el).backgroundColor,
      accent:getComputedStyle(document.documentElement).getPropertyValue("--nk-accent").trim()
    }));
    expect(color.led).toBe("rgb(252, 238, 10)");
  });
  test("Inspect has bounded geometry and Out is a real inspection target", async ({page}) => {
    await page.locator("[data-node-id='stepseq1'] .nk-chip-expand").click();
    const panel=page.locator(".nk-overlay");
    const pb=await panel.boundingBox();
    expect(pb!.width).toBeLessThanOrEqual(660);
    for (const slider of await panel.getByRole("slider").all()) {
      const b=await slider.boundingBox();
      expect(b!.width).toBeLessThanOrEqual(72);
      expect(b!.x).toBeGreaterThanOrEqual(pb!.x);
      expect(b!.x+b!.width).toBeLessThanOrEqual(pb!.x+pb!.width);
    }
    await page.getByRole("button",{name:"Close",exact:true}).click();
    await page.locator("[data-node-id='OUT'] .nk-chip-expand").click();
    await expect(panel).toContainText("Plugin output");
    await expect(panel).not.toContainText("No node selected");
    const close=await panel.getByRole("button",{name:"Close",exact:true}).boundingBox();
    await page.evaluate(() => window.dispatchEvent(new CustomEvent("unit-contract-host", {
      detail: { clips:[{id:"OUT",peak:0.89,rms:0.3}] }
    })));
    expect(await panel.getByRole("button",{name:"Close",exact:true}).boundingBox()).toEqual(close);
  });

  test("meter screen follows its own tap without changing chip geometry", async ({page}, info) => {
    const screen=page.locator("[data-node-id='meter1'] [data-meter-screen]");
    await expect(screen).toBeVisible();
    const before=await screen.boundingBox();
    await page.evaluate(() => window.dispatchEvent(new CustomEvent("unit-contract-host", {
      detail: { clips:[{id:"meter1",peak:0.5,rms:0.25},{id:"OUT",peak:1,rms:1}] }
    })));
    await expect(screen).toContainText("-6.0");
    await expect(screen).toContainText("-12.0");
    expect(await screen.boundingBox()).toEqual(before);
    await page.locator("[data-node-id='meter1'] .nk-chip-expand").click();
    await page.screenshot({path:info.outputPath("inspect-meter.png")});
    await page.getByRole("button",{name:"Close",exact:true}).click();
    await page.screenshot({path:info.outputPath("board-led-meter.png")});
  });

});
