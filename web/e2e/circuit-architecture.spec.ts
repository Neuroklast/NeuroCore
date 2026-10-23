import {test,expect} from "@playwright/test";
import {installUnitHost} from "./unit-host";
const keys=["threshold","ratio","attack","release","knee","makeup","hpf","mix","lookahead","ceiling"];
const ast={version:1,params:[],leadingComments:[],nodes:[
 {id:"random1",type:"random",args:{},busName:"mod",trailingComment:""},
 {id:"comp1",type:"comp",args:Object.fromEntries(keys.map(key=>[key,"random1"])),busName:"main",trailingComment:""}
],edges:[{from:"IN",to:"comp1",kind:"audio",fromJack:"out",toJack:"in"},{from:"comp1",to:"OUT",kind:"audio",fromJack:"out",toJack:"in"}]};
test("compact connection banks keep geometry across all themes",async({page})=>{
  await installUnitHost(page);
  await page.goto("/");await page.locator("html[data-nk-ready='1']").waitFor();
  await page.evaluate(payload=>window.dispatchEvent(new CustomEvent("unit-contract-ast",{detail:payload})),
   {origin:"host",script:"comp1: threshold = random1\n",astJson:JSON.stringify(ast),diagnostics:[]});
  await page.getByRole("button",{name:"Circuit",exact:true}).click();
  const chip=page.locator('[data-node-id="comp1"]');await expect(chip).toBeVisible();
  await expect(page.locator('[aria-label="Layout"]')).toHaveCount(0,{timeout:30000});
  for(const theme of ["signal","gold","azure","digicide"]) {
  await page.evaluate(theme=>window.dispatchEvent(new CustomEvent("unit-contract-host",{detail:{theme}})),theme);
  const before=await chip.boundingBox();
  await chip.getByRole("button",{name:"Connections"}).click();
  const dialog=page.getByRole("dialog",{name:"Connections for comp1"});await expect(dialog).toBeVisible();
  await dialog.getByRole("searchbox").fill("makeup");
  await expect(dialog.getByRole("button",{name:/makeup/i})).toBeVisible();
  await dialog.getByRole("button",{name:"Close connections"}).click();
  expect(await chip.boundingBox()).toEqual(before);
  expect(await chip.locator('.nk-port-north').count()).toBeGreaterThan(0);
  const scale=await page.locator('.nk-board-world').evaluate(el=>new DOMMatrix(getComputedStyle(el).transform).a);
  expect(before!.width/scale).toBeLessThanOrEqual(448.01);
  await page.screenshot({path:`/tmp/neurocore-circuit-${theme}.png`});
  }
});
