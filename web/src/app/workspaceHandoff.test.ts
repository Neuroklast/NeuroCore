import {expect,it} from "vitest";
import {handoffPlan} from "./workspaceHandoff";
const telemetry={available:true,receivedAt:1000,inRms:.2,outRms:.3};
it("uses fresh measured input/output only for Unit/Circuit transitions",()=>{
 expect(handoffPlan("face","assemble","full",false,telemetry,1100)?.direction).toBe("in");
 expect(handoffPlan("assemble","face","full",false,telemetry,1100)?.direction).toBe("out");
 expect(handoffPlan("face","hack","full",false,telemetry,1100)).toBeNull();
 expect(handoffPlan("face","face","full",false,telemetry,1100)).toBeNull();
});
it("does not animate stale, silent, unavailable or reduced/off motion",()=>{
 for(const motion of ["off","reduced"])expect(handoffPlan("face","assemble",motion,false,telemetry,1100)).toBeNull();
 expect(handoffPlan("face","assemble","full",true,telemetry,1100)).toBeNull();
 expect(handoffPlan("face","assemble","full",false,telemetry,2000)).toBeNull();
 expect(handoffPlan("face","assemble","full",false,{...telemetry,inRms:0},1100)).toBeNull();
 expect(handoffPlan("face","assemble","full",false,{...telemetry,available:false},1100)).toBeNull();
});
