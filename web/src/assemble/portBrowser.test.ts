import {expect,it} from "vitest";
import {connectionTargets} from "./PortBrowser";
it("lists real compressor modulation targets and filters by full name",()=>{
 expect(connectionTargets("comp",{},"makeup")).toContain("makeup");
 expect(connectionTargets("comp",{},"detector")).toEqual([]);
 expect(connectionTargets("comp",{},"makeup")).not.toContain("attack");
});
it("offers distinct ModMix inputs but no invented MIDI event source",()=>{
 expect(connectionTargets("modmix",{})).toContain("inA");
 expect(connectionTargets("modmix",{})).toContain("inB");
 expect(connectionTargets("modmix",{})).not.toContain("midi");
});
