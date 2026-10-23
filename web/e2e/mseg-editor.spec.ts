import { test, expect } from "@playwright/test";

const script = "mseg1: points = 0:0:0,0.25:1:0,1:0:0; duration = 1";

test.describe("MSEG curve editor", () => {
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
              id: "mseg1",
              type: "mseg",
              busName: "",
              args: {points: "0:0:0,0.25:1:0,1:0:0", duration:"1"},
              trailingComment: "",
            }],
            edges: [],
          }),
          diagnostics: [],
        },
      }));
    }, script);
    await page.locator("[data-node-id='mseg1']").waitFor();
  });

  test("fits Inspect and creates, moves and deletes interior points", async ({page}) => {
    const chip=page.locator("[data-node-id='mseg1']");
    await chip.locator(".nk-chip-expand").click();
    const editor=page.locator("[data-mseg-editor]");
    await expect(editor).toBeVisible();
    await editor.scrollIntoViewIfNeeded();
    const overlay=page.locator(".nk-overlay").last();
    const ob=await overlay.boundingBox(), eb=await editor.boundingBox();
    expect(eb!.x).toBeGreaterThanOrEqual(ob!.x);
    expect(eb!.x+eb!.width).toBeLessThanOrEqual(ob!.x+ob!.width);
    expect(eb!.y+eb!.height).toBeLessThanOrEqual(ob!.y+ob!.height);
    await expect(editor.locator("[data-mseg-point]")).toHaveCount(3);
    await editor.getByRole("button",{name:"Add point",exact:true}).click();
    await expect(editor.locator("[data-mseg-point]")).toHaveCount(4);
    const point=editor.locator("[data-mseg-point='1']");
    const pb=await point.boundingBox();
    await page.mouse.move(pb!.x+pb!.width/2,pb!.y+pb!.height/2);await page.mouse.down();
    await page.mouse.move(pb!.x+pb!.width/2+10,pb!.y+pb!.height/2+8,{steps:4});await page.mouse.up();
    await point.focus();await page.keyboard.press("ArrowDown");
    await page.keyboard.press("Delete");
    await expect(editor.locator("[data-mseg-point]")).toHaveCount(3);
    await editor.getByLabel("Segment curve").fill("2");
    await editor.getByLabel("Segment curve").blur();
    await expect(editor.locator("svg")).toBeVisible();
  });
});
