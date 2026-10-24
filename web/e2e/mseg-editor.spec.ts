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
  test("maintains circular points and correct positions when Inspect changes width", async ({page}) => {
    await page.locator("[data-node-id='mseg1'] .nk-chip-expand").click();
    const editor=page.locator("[data-mseg-editor]");
    const plot=editor.locator('svg[aria-label="MSEG point editor"]');
    await expect(plot).toHaveAttribute("preserveAspectRatio","xMidYMid meet");
    const matchesBounds=()=>plot.evaluate((svg)=>{
      const viewBox=svg.viewBox.baseVal;
      return Math.abs(viewBox.width-svg.clientWidth)<2 && Math.abs(viewBox.height-svg.clientHeight)<2;
    });
    await expect.poll(matchesBounds).toBe(true);
    const point=editor.locator("[data-mseg-point='1']");
    const marker=point.locator("circle").last();
    const before=await marker.boundingBox();
    expect(before).not.toBeNull();
    expect(Math.abs(before!.width-before!.height)).toBeLessThan(1);
    const chartBefore=await plot.boundingBox();
    expect(chartBefore).not.toBeNull();
    const relativeBefore=(before!.x+before!.width/2-chartBefore!.x)/chartBefore!.width;
    expect(relativeBefore).toBeGreaterThan(0.2);
    expect(relativeBefore).toBeLessThan(0.3);

    await plot.evaluate(el=>{el.style.width="420px";});
    await expect.poll(matchesBounds).toBe(true);
    const after=await marker.boundingBox();
    const chartAfter=await plot.boundingBox();
    expect(after).not.toBeNull();
    expect(chartAfter).not.toBeNull();
    expect(Math.abs(after!.width-after!.height)).toBeLessThan(1);
    const relativeAfter=(after!.x+after!.width/2-chartAfter!.x)/chartAfter!.width;
    expect(relativeBefore).toBeCloseTo(0.25+8/chartBefore!.width,2);
    expect(relativeAfter).toBeCloseTo(0.25+8/chartAfter!.width,2);
  });

});
