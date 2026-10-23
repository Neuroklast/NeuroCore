export function stepCountFromArgs(args: Record<string, string>): number {
  const n = Math.round(Number(args.steps ?? 4));
  return Number.isFinite(n) ? Math.max(1, Math.min(64, n)) : 4;
}

function tokens(raw: string | undefined): string[] {
  return (raw ?? "").split(/[,;]/).map((s) => s.trim()).filter((s) => s.length > 0);
}

export function readStepValues(args: Record<string, string>, count = stepCountFromArgs(args)): number[] {
  const listed = tokens(args.values);
  const out: number[] = [];
  for (let i = 0; i < count; i += 1) {
    const raw = listed.length > 0 ? listed[i] : args[`step${i}`];
    const n = Number(raw ?? 0);
    out.push(Number.isFinite(n) ? Math.max(-1, Math.min(1, n)) : 0);
  }
  return out;
}

export function readStepMutes(args: Record<string, string>, count = stepCountFromArgs(args)): boolean[] {
  const listed = tokens(args.mutes);
  return Array.from({ length: count }, (_, i) => Number(listed[i] ?? 0) !== 0);
}

export function formatStepValues(values: number[]): string {
  return values.map((v) => Math.max(-1, Math.min(1, v)).toFixed(2)).join(",");
}

export function formatStepMutes(mutes: boolean[]): string {
  return mutes.map((m) => (m ? "1" : "0")).join(",");
}

/** Pointer y inside a bar area: top = 1, centre = 0, bottom = -1, 0.05 steps. */
export function stepValueFromPointer(clientY: number, top: number, height: number): number {
  if (! (height > 0)) {
    return 0;
  }
  const raw = 1 - 2 * ((clientY - top) / height);
  return Math.max(-1, Math.min(1, Math.round(raw * 20) / 20));
}

export function stepIndexFromPointer(clientX: number, left: number, width: number, count: number): number {
  if (! (width > 0)) {
    return 0;
  }
  return Math.max(0, Math.min(count - 1, Math.floor(((clientX - left) / width) * count)));
}

export type StepLedState = "unused" | "muted" | "current" | "ready" | "stopped";

/** A host playhead is a measurement. Invalid/stopped values never animate locally. */
export function stepLedMatrix(args:Record<string,string>,liveStep:number) {
  const count=stepCountFromArgs(args);
  const rows=Math.ceil(count/8);
  const values=readStepValues(args,count);
  const mutes=readStepMutes(args,count);
  const current=Number.isInteger(liveStep)&&liveStep>=0&&liveStep<count?liveStep:-1;
  return {columns:8,rows,count,cells:Array.from({length:rows*8},(_,index)=>{
    const used=index<count;
    const state:StepLedState=!used?"unused":mutes[index]?"muted":index===current?"current":current<0?"stopped":"ready";
    return {index,value:values[index]??0,state,active:used&&index===current,muted:used&&mutes[index]};
  })};
}
