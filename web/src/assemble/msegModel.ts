export type MsegPoint = {time:number;value:number;curve:number};
export const DEFAULT_MSEG = '0:0:0,0.25:1:0,1:0:0';
const clamp=(n:number,lo:number,hi:number)=>Math.max(lo,Math.min(hi,n));
export function readMsegPoints(raw=DEFAULT_MSEG):MsegPoint[]{
 const fields=raw.split(',').map(p=>p.trim().split(':').map(Number));
 const valid=fields.length>=2 && fields.length<=64 && fields.every((p,i)=>
  (p.length===2||p.length===3) && p.every(Number.isFinite) && p[0]!>=0 && p[0]!<=1 && Math.abs(p[1]!)<=1
  && Math.abs(p[2]??0)<=8 && (i===0 || p[0]!>fields[i-1]![0]!))
  && fields[0]![0]===0 && fields[fields.length-1]![0]===1;
 if(!valid)return readMsegPoints(DEFAULT_MSEG);
 return fields.map(p=>({time:p[0]!,value:p[1]!,curve:p[2]??0}));
}
export function formatMsegPoints(points:MsegPoint[]):string{
 return points.map(p=>[p.time,p.value,p.curve].map(n=>Number(n.toFixed(8))).join(':')).join(',');
}
export function msegAt(points:MsegPoint[],time:number):number{
 const i=Math.max(0,points.findIndex(p=>p.time>=time)-1);
 if(time>=1)return points[points.length-1]!.value;
 const a=points[i]!,b=points[i+1]!;
 return a.value+(b.value-a.value)*Math.pow(clamp((time-a.time)/(b.time-a.time),0,1),Math.pow(2,a.curve));
}
export function moveMsegPoint(points:MsegPoint[],index:number,time:number,value:number,curve=points[index]!.curve):MsegPoint[]{
 return points.map((p,i)=>i!==index?p:{
  time:i===0?0:i===points.length-1?1:clamp(time,points[i-1]!.time+1.e-6,points[i+1]!.time-1.e-6),
  value:clamp(value,-1,1),curve:clamp(curve,-8,8)});
}
export function insertMsegPoint(points:MsegPoint[]):MsegPoint[]{
 if(points.length>=64)return points;
 let index=0;
 for(let i=1;i<points.length-1;i++)if(points[i+1]!.time-points[i]!.time>points[index+1]!.time-points[index]!.time)index=i;
 const time=(points[index]!.time+points[index+1]!.time)/2;
 return [...points.slice(0,index+1),{time,value:msegAt(points,time),curve:0},...points.slice(index+1)];
}
export function deleteMsegPoint(points:MsegPoint[],index:number):MsegPoint[]{
 return index<=0 || index>=points.length-1?points:points.filter((_,i)=>i!==index);
}
export function msegPath(points:MsegPoint[]):string{
 return Array.from({length:129},(_,i)=>`${i?'L':'M'}${(i/128*100).toFixed(3)} ${((1-msegAt(points,i/128))*20).toFixed(3)}`).join(' ');
}

/**
 * Inspect graph geometry is expressed in CSS pixel-sized SVG units. Unlike the
 * compact chip glyph, the editor must never scale its node circles differently
 * on the X and Y axes when the inspector is resized.
 */
export type MsegEditorSize = { width: number; height: number };
const EDITOR_PAD_X = 16;
const EDITOR_PAD_Y = 14;

export function msegEditorPosition(time: number, value: number, size: MsegEditorSize): { x: number; y: number } {
  const w = Math.max(EDITOR_PAD_X * 2 + 1, size.width);
  const h = Math.max(EDITOR_PAD_Y * 2 + 1, size.height);
  return {
    x: EDITOR_PAD_X + clamp(time, 0, 1) * (w - EDITOR_PAD_X * 2),
    y: EDITOR_PAD_Y + (1 - clamp(value, -1, 1)) * 0.5 * (h - EDITOR_PAD_Y * 2),
  };
}

/** Inverse plot projection. Caller converts from viewport to local SVG pixels. */
export function msegEditorValueAt(x: number, y: number, size: MsegEditorSize): { time: number; value: number } {
  const w = Math.max(EDITOR_PAD_X * 2 + 1, size.width);
  const h = Math.max(EDITOR_PAD_Y * 2 + 1, size.height);
  return {
    time: clamp((x - EDITOR_PAD_X) / (w - EDITOR_PAD_X * 2), 0, 1),
    value: clamp(1 - 2 * (y - EDITOR_PAD_Y) / (h - EDITOR_PAD_Y * 2), -1, 1),
  };
}

/** Handles CSS / WebView zoom because client pixels are normalized via the rendered rectangle. */
export function msegEditorPointer(
  clientX: number,
  clientY: number,
  rect: { left: number; top: number; width: number; height: number },
  size: MsegEditorSize,
): { time: number; value: number } {
  const x = rect.width > 0 ? (clientX - rect.left) / rect.width * size.width : 0;
  const y = rect.height > 0 ? (clientY - rect.top) / rect.height * size.height : 0;
  return msegEditorValueAt(x, y, size);
}

/** Same interpolation as native MSEG, projected into a non-distorted responsive editor. */
export function msegEditorPath(points: MsegPoint[], size: MsegEditorSize): string {
  const samples = Math.min(512, Math.max(128, Math.ceil(size.width / 2)));
  return Array.from({ length: samples + 1 }, (_, i) => {
    const time = i / samples;
    const p = msegEditorPosition(time, msegAt(points, time), size);
    return `${i ? "L" : "M"}${p.x.toFixed(2)} ${p.y.toFixed(2)}`;
  }).join(" ");
}
