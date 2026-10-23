import {memo} from "react";
import {useHostStore} from "../store/hostStore";
import {NodeFace} from "./nodeFace/NodeFace";
import {faceKindForType} from "./nodeFace/faceKind";
import {stepCountFromArgs} from "./stepSeqModel";
import {useFaceKnobs} from "./useFaceKnobs";
import type {BoardNode} from "./boardModel";

/** Only the function well subscribes to step/trigger/control telemetry. */
export const ChipFunctionFace=memo(function ChipFunctionFace({node}:{node:BoardNode}) {
  const knobs=useFaceKnobs(node.args);
  const bpm=useHostStore(s=>s.bpm);
  const sr=useHostStore(s=>s.sr);
  const step=useHostStore(s=>node.type==="stepseq"?(s.clipsStep[node.id]??-1):-1);
  const trigger=useHostStore(s=>node.type==="envtrigger"?(s.clipsTrigger[node.id]??0):0);
  const controlA=useHostStore(s=>node.type==="modmix"?s.mods[node.args.ina??node.args.inA??""]:undefined);
  const controlB=useHostStore(s=>node.type==="modmix"?s.mods[node.args.inb??node.args.inB??""]:undefined);
  const controlOut=useHostStore(s=>node.type==="modmix"?s.mods[node.id]:undefined);
  return <NodeFace kind={faceKindForType(node.type)} type={node.type} args={node.args} id={node.id}
    host={{knobs,bpm,sr,step,trigger,controlA,controlB,controlOut,stepCount:node.type==="stepseq"?stepCountFromArgs(node.args):undefined}} />;
});
