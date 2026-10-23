import type {Workspace} from "./workspace";
export const HANDOFF_MS=220;
type Reading={available:boolean;receivedAt:number;inRms:number;outRms:number};
/** A short view cue, never a DSP route or a replacement audio measurement. */
export function handoffPlan(from:Workspace,to:Workspace,motion:string,reduced:boolean,reading:Reading,now:number):{direction:"in"|"out";opacity:number;duration:number}|null {
  const direction=from==="face"&&to==="assemble"?"in":from==="assemble"&&to==="face"?"out":null;
  if(!direction||motion!=="full"||reduced||!reading.available||now<reading.receivedAt||now-reading.receivedAt>750)return null;
  const rms=direction==="in"?reading.inRms:reading.outRms;
  if(!Number.isFinite(rms)||rms<=.001)return null;
  return {direction,opacity:Math.min(.7,.2+Math.sqrt(rms)*.4),duration:HANDOFF_MS};
}
