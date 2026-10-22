import { test, expect } from "@playwright/test";

const script = "modmix1: ina = random1; inb = random2; levela = 1; levelb = 1";

test.describe("control mixer display", () => {
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
            nodes: [{id:"random1",type:"random",busName:"",args:{min:"0",max:"1"},trailingComment:""},{id:"join1",type:"join",busName:"",args:{mix:"0.5"},trailingComment:""},{
              id: "modmix1",
              type: "modmix",
              busName: "",
              args: {ina: "random1", inb: "random2", levela: "1", levelb: "1"},
              trailingComment: "",
            }],
            edges: [],
          }),
          diagnostics: [],
        },
      }));
    }, script);
    await page.locator("[data-node-id='modmix1']").waitFor();
  });

  test("shows measured A/B/output inside the chip without moving its pins", async ({page}) => {
    const chip=page.locator("[data-node-id='modmix1']");
    const face=chip.locator("[data-glyph='control-mix']");
    await expect(face).toBeVisible();
    const before=await chip.boundingBox();
    await page.evaluate(()=>window.dispatchEvent(new CustomEvent("unit-contract-host",{detail:{mods:[{id:"random1",value:0.4},{id:"random2",value:-0.2},{id:"modmix1",value:0.2}]}})));
    await expect(face.locator("[data-mix-output]")).toHaveAttribute("data-mix-output","0.2");
    const after=await chip.boundingBox(), box=await face.boundingBox();
    expect(after).toEqual(before);
    expect(box!.x).toBeGreaterThanOrEqual(after!.x);
    expect(box!.y).toBeGreaterThanOrEqual(after!.y);
    expect(box!.x+box!.width).toBeLessThanOrEqual(after!.x+after!.width);
    expect(box!.y+box!.height).toBeLessThanOrEqual(after!.y+after!.height);
    await chip.locator(".nk-chip-expand").click();
    await expect(page.locator(".nk-overlay").last()).toBeVisible();
  });
  test("real cable drag distinguishes control A/B from audio Join A/B",async({page})=>{
    for(const [node,sourceId,accepted] of [
      ["modmix1","random1::out::src",true],["modmix1","IN::out::src",false],
      ["join1","IN::out::src",true],["join1","random1::out::src",false],
    ] as const){
      for(const input of ["inA","inB"]){
        const target=page.locator(`[data-port-id="${node}::${input}::dst"]`);
        const dest=await target.boundingBox(),source=await page.locator(`[data-port-id="${sourceId}"]`).boundingBox();
        expect(dest).not.toBeNull();expect(source).not.toBeNull();
        await page.mouse.move(source!.x+source!.width/2,source!.y+source!.height/2);await page.mouse.down();
        await page.mouse.move(dest!.x+dest!.width/2,dest!.y+dest!.height/2,{steps:6});
        if(accepted)await expect(target).toHaveClass(/nk-port-hot/);else await expect(target).not.toHaveClass(/nk-port-hot/);
        await page.mouse.move(30,620);await page.mouse.up();
      }
    }
  });

});
