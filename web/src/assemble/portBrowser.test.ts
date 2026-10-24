import {expect,it} from "vitest";
import {connectionTargets,portTargetValue} from "./PortBrowser";
it("lists real compressor modulation targets and filters by full name",()=>{
 expect(connectionTargets("comp",{},"makeup")).toContain("makeup");
 expect(connectionTargets("comp",{},"detector")).toEqual([]);
 expect(connectionTargets("comp",{},"makeup")).not.toContain("attack");
});
it("formats modulation targets for musicians without leaking normalized fractions",()=>{
 expect(portTargetValue("0.734283", "%")).toBe("73.43%");
 expect(portTargetValue("1200", "Hz")).toBe("1200.00 Hz");
 expect(portTargetValue("a")).toBe("Macro A");
 expect(portTargetValue("a * 2400")).toBe("Expression: a * 2400");
 expect(portTargetValue("")).toBe("—");
});
it("offers distinct ModMix inputs but no invented MIDI event source",()=>{
 expect(connectionTargets("modmix",{})).toContain("inA");
 expect(connectionTargets("modmix",{})).toContain("inB");
 expect(connectionTargets("modmix",{})).not.toContain("midi");
});
