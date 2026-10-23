import {useMemo} from "react";
import {useShallow} from "zustand/react/shallow";
import {useHostStore} from "../store/hostStore";
import {lettersInExpr} from "./bindLinks";
import type {FaceKnob} from "./nodeFace/faceArgs";

/** Select scalars, not the host knob array: unrelated macros cannot repaint a chip. */
export function useFaceKnobs(args:Record<string,string>):FaceKnob[] {
  const letters=useMemo(()=>new Set(Object.values(args).flatMap(lettersInExpr)),[args]);
  const fields=useHostStore(useShallow(state=>state.knobs.filter(k=>letters.has(k.id))
    .flatMap(k=>[k.id,k.value,k.min,k.max,k.isNote??false])));
  return useMemo(()=>{
    const knobs:FaceKnob[]=[];
    for(let i=0;i<fields.length;i+=5)knobs.push({id:String(fields[i]),value:Number(fields[i+1]),min:Number(fields[i+2]),max:Number(fields[i+3]),isNote:Boolean(fields[i+4])});
    return knobs;
  },[fields]);
}
