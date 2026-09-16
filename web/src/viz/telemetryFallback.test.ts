import { expect, it } from "vitest";
import { createTelemetryViews } from "../bridge/telemetry";
import { fillTelemetryFallback } from "./ScopeDeck";
it("clears failed native telemetry instead of synthesizing audio", () => {
 const v=createTelemetryViews(); v.inPeak=0.7; v.scopeOut.fill(0.4);
 fillTelemetryFallback(v,10,true);
 expect(v.inPeak+v.outPeak+v.inRms+v.outRms).toBe(0);
 expect(v.scopeIn.every(x=>x===0)&&v.scopeOut.every(x=>x===0)).toBe(true);
 expect(v.gonioX.every(x=>x===0)&&v.gonioY.every(x=>x===0)).toBe(true);
});
it("keeps synthetic audio available only in the browser preview",()=>{
 const v=createTelemetryViews(); fillTelemetryFallback(v,10,false);
 expect(v.outPeak).toBeGreaterThan(0);
});
