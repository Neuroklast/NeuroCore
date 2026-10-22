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
                steps: "4",
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

  test("stays inside the Inspect panel and edits values", async ({ page }) => {
    const chip = page.locator("[data-node-id='stepseq1']");
    await chip.locator(".nk-chip-expand").click();
    await expect(page.locator(".nk-overlay")).toBeVisible();

    const editor = page.locator("[data-stepseq-editor]");
    await expect(editor).toBeVisible();
    const overlay = page.locator(".nk-overlay").last();
    const ob = await overlay.boundingBox();
    const eb = await editor.boundingBox();
    if (! ob || ! eb) throw new Error("missing editor or overlay box");
    expect(eb.x).toBeGreaterThanOrEqual(ob.x);
    expect(eb.x + eb.width).toBeLessThanOrEqual(ob.x + ob.width + 1);

    const count = Number(await editor.getAttribute("data-step-count"));
    expect(count).toBe(4);
    await expect(editor.locator("[data-step-bar]")).toHaveCount(count);

    const plot = editor.locator("div").filter({ has: page.locator("svg") }).first();
    const pb = await plot.boundingBox();
    if (! pb) throw new Error("missing plot box");
    const before = Number(await editor.locator("[data-step-bar='0']").getAttribute("height"));
    await page.mouse.move(pb.x + pb.width / (2 * count), pb.y + pb.height * 0.85);
    await page.mouse.down();
    await page.mouse.up();
    await expect.poll(async () => Number(await editor.locator("[data-step-bar='0']").getAttribute("height")))
      .not.toBe(before);

    const mute = editor.locator("[data-mute-step='0']");
    await mute.click();
    await expect(mute).toHaveAttribute("aria-pressed", "true");

    await page.evaluate(() => {
      window.dispatchEvent(new CustomEvent("unit-contract-host", {
        detail: { clips: [{ id: "stepseq1", peak: 0, step: 1 }] },
      }));
    });
    await expect(editor.locator("[data-step-live]")).toHaveAttribute("data-step-live", "1");
    await expect(page.locator("[data-step-tick='1']")).toHaveAttribute("data-active", "on");
  });
});
