import {readFileSync} from "node:fs";
import {expect,it} from "vitest";
it("keeps step/trigger/modulation telemetry outside the chip shell",()=>{
 const body=readFileSync(new URL("./BoardChip.tsx",import.meta.url),"utf8");
 expect(body).not.toContain("s.clipsStep");
 expect(body).not.toContain("s.clipsTrigger");
 expect(body).not.toContain("s.mods[node.args");
 expect(body).toContain("ChipFunctionFace");
});
