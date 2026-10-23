import {useEffect} from "react";
import {HANDOFF_MS} from "./workspaceHandoff";
export type HandoffVisual={id:number;direction:"in"|"out";opacity:number;x:number;y:number;w:number;h:number};

/** One disposable SVG streak. The Unit renderer still unmounts immediately. */
export function WorkspaceHandoff({visual,onFinish}:{visual:HandoffVisual;onFinish:()=>void}) {
  useEffect(()=>{
    const timer=window.setTimeout(onFinish,HANDOFF_MS);
    const media=window.matchMedia("(prefers-reduced-motion: reduce)");
    const cancel=()=>{if(media.matches)onFinish();};
    media.addEventListener("change",cancel);
    return ()=>{window.clearTimeout(timer);media.removeEventListener("change",cancel);};
  },[onFinish]);
  const center={x:visual.w*.5,y:visual.h*.5};
  const start=visual.direction==="in"?center:{x:visual.x,y:visual.y};
  const end=visual.direction==="in"?{x:visual.x,y:visual.y}:center;
  return <svg key={visual.id} className="nk-workspace-handoff" viewBox={`0 0 ${visual.w} ${visual.h}`} aria-hidden="true" data-handoff={visual.direction} style={{opacity:visual.opacity}}>
    <path d={`M${start.x} ${start.y} Q${(start.x+end.x)*.5} ${start.y} ${end.x} ${end.y}`} pathLength="1" />
  </svg>;
}
