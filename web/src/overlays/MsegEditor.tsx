import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { setCircuitArgs } from "../assemble/addBlock";
import { deleteMsegPoint, formatMsegPoints, insertMsegPoint, moveMsegPoint, msegEditorPath, msegEditorPointer, msegEditorPosition, readMsegPoints, type MsegPoint, type MsegEditorSize } from "../assemble/msegModel";

type Bounds = {loopstart:number;loopend:number;sustain:number};
export function MsegEditor({nodeId,args}:{nodeId:string;args:Record<string,string>}) {
  const [points,setPoints]=useState(()=>readMsegPoints(args.points));
  const [selected,setSelected]=useState(0);
  const current=useRef(points);
  const bounds=useRef<Bounds>({loopstart:0,loopend:points.length-1,sustain:-1});
  const drag=useRef<{index:number;before:MsegPoint[]}|null>(null);
  const svg=useRef<SVGSVGElement>(null);
  const [plotSize,setPlotSize]=useState<MsegEditorSize>({width:600,height:180});
  useLayoutEffect(()=>{
    const element=svg.current;
    if(!element)return;
    const update=(width:number,height:number)=>{
      if(width<=0||height<=0)return;
      const next={width:Math.round(width),height:Math.round(height)};
      setPlotSize(prev=>prev.width===next.width&&prev.height===next.height?prev:next);
    };
    update(element.clientWidth,element.clientHeight);
    const observer=new ResizeObserver(entries=>{
      const rect=entries[0]?.contentRect;
      if(rect)update(rect.width,rect.height);
    });
    observer.observe(element);
    return ()=>observer.disconnect();
  },[]);
  useEffect(()=>{
    const next=readMsegPoints(args.points);
    current.current=next;setPoints(next);setSelected(i=>Math.min(i,next.length-1));
    bounds.current={loopstart:Number(args.loopstart??0),loopend:Number(args.loopend??next.length-1),sustain:Number(args.sustain??-1)};
  },[nodeId,args.points,args.loopstart,args.loopend,args.sustain]);
  const cancel=()=>{
    if(drag.current){current.current=drag.current.before;setPoints(current.current);drag.current=null;}
  };
  useEffect(()=>{
    const blur=()=>{
      if(drag.current){current.current=drag.current.before;setPoints(current.current);drag.current=null;}
    };
    window.addEventListener("blur",blur);
    return ()=>window.removeEventListener("blur",blur);
  },[]);
  const commit=(next:MsegPoint[],before=current.current)=>{
    const remap=(index:number)=>{
      if(index<0)return -1;
      if(next.length===before.length)return Math.min(index,next.length-1);
      return next.indexOf(before[index]!);
    };
    let start=Math.max(0,remap(bounds.current.loopstart)),end=remap(bounds.current.loopend);
    if(end<=start){start=0;end=next.length-1;}
    bounds.current={loopstart:start,loopend:end,sustain:remap(bounds.current.sustain)};
    current.current=next;setPoints(next);setSelected(i=>Math.min(i,next.length-1));
    setCircuitArgs(nodeId,{points:formatMsegPoints(next),...Object.fromEntries(Object.entries(bounds.current).map(([k,v])=>[k,String(v)]))});
  };
  const remove=(index:number)=>commit(deleteMsegPoint(current.current,index));
  return <section data-mseg-editor="" className="border border-[var(--nk-line)] bg-black/40 p-2 text-xs">
    <div className="flex items-center justify-between gap-2">
      <span>MSEG · {points.length}/64 points</span>
      <button type="button" className="min-h-[28px] px-2" disabled={points.length>=64} onClick={()=>commit(insertMsegPoint(current.current))}>Add point</button>
    </div>
    <svg ref={svg} viewBox={`0 0 ${plotSize.width} ${plotSize.height}`} preserveAspectRatio="xMidYMid meet" width="100%" height="180"
      style={{touchAction:"none"}} aria-label="MSEG point editor"
      onPointerDown={e=>{
        const hit=(e.target as Element).closest("[data-mseg-point]");
        if(!hit)return;
        e.preventDefault();const index=Number(hit.getAttribute("data-mseg-point"));
        setSelected(index);drag.current={index,before:current.current};e.currentTarget.setPointerCapture(e.pointerId);
      }}
      onPointerMove={e=>{
        if(!drag.current)return;
        const {time,value}=msegEditorPointer(e.clientX,e.clientY,e.currentTarget.getBoundingClientRect(),plotSize);
        const next=moveMsegPoint(current.current,drag.current.index,time,value);
        current.current=next;setPoints(next);
      }}
      onPointerUp={e=>{
        if(!drag.current)return;
        const before=drag.current.before;drag.current=null;
        if(e.currentTarget.hasPointerCapture(e.pointerId))e.currentTarget.releasePointerCapture(e.pointerId);
        commit(current.current,before);
      }}
      onPointerCancel={cancel}
      onLostPointerCapture={cancel}
      onKeyDown={e=>{if(e.key==="Escape"){e.preventDefault();cancel();}}}>
      <path d={`M${msegEditorPosition(0,0,plotSize).x} ${msegEditorPosition(0,0,plotSize).y} H${msegEditorPosition(1,0,plotSize).x}`} stroke="var(--nk-line)" fill="none" strokeWidth="1.5"/>
      <path d={msegEditorPath(points,plotSize)} stroke="var(--nk-accent)" fill="none" strokeWidth="2.5" strokeLinejoin="round" strokeLinecap="round"/>
      {points.map((p,i)=><g key={i} data-mseg-point={i} tabIndex={0} role="button" aria-label={`Point ${i+1}`}
        onFocus={()=>setSelected(i)}
        onKeyDown={e=>{
          if(e.key==="Delete"||e.key==="Backspace"){e.preventDefault();remove(i);return;}
          const dx=e.key==="ArrowLeft"?-0.01:e.key==="ArrowRight"?0.01:0;
          const dy=e.key==="ArrowDown"?-0.05:e.key==="ArrowUp"?0.05:0;
          if(dx||dy){e.preventDefault();commit(moveMsegPoint(current.current,i,p.time+dx,p.value+dy));}
        }}>
        <circle cx={msegEditorPosition(p.time,p.value,plotSize).x} cy={msegEditorPosition(p.time,p.value,plotSize).y} r="12" fill="transparent"/>
        <circle cx={msegEditorPosition(p.time,p.value,plotSize).x} cy={msegEditorPosition(p.time,p.value,plotSize).y}
          r={i===selected?5:4} fill="var(--nk-accent)" stroke="var(--nk-well)" strokeWidth="1" pointerEvents="none"/>
      </g>)}
    </svg>
    <div className="flex flex-wrap items-center gap-2">
      <label>Segment curve <input aria-label="Segment curve" type="number" min={-8} max={8} step={0.25}
        className="w-20 min-h-[28px] border border-[var(--nk-line)] bg-[var(--nk-well)] px-2 text-[var(--nk-ink)] font-mono tabular-nums focus:border-[var(--nk-accent)]" value={points[selected]?.curve??0}
        onChange={e=>{
          if(!Number.isFinite(e.currentTarget.valueAsNumber))return;
          const p=current.current[selected]!;
          current.current=moveMsegPoint(current.current,selected,p.time,p.value,e.currentTarget.valueAsNumber);setPoints(current.current);
        }} onBlur={()=>commit(current.current)} /></label>
      <button type="button" className="min-h-[28px] px-2" disabled={selected===0||selected===points.length-1} onClick={()=>remove(selected)}>Delete point</button>
      <span className="text-[var(--nk-ink-soft)]">Drag · arrows move · Delete removes · Esc cancels</span>
    </div>
  </section>;
}
