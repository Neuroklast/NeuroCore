import {useEffect,useRef,useState} from "react";
import {entryById} from "./blockCatalog";
import {formatKnobDisplay} from "../chrome/noteValue";
import {chipSpec} from "./chipSpec";
import {commitBoardConnect} from "./boardCommit";
import {portId,portIsOutput,type BoardGraph,type BoardPort} from "./boardModel";

export function connectionTargets(type:string,args:Record<string,string>,query="") {
  const spec=chipSpec(type,args);
  const catalog=entryById(spec.id);
  const keys=[...new Set([...spec.modIns,...(catalog?.params.filter(p=>p.modulatable).map(p=>p.key)??Object.keys(spec.ranges))])];
  return keys.filter(key=>`${key} ${spec.paramJackLabels[key]??""}`.toLowerCase().includes(query.toLowerCase()));
}

/** Display human-readable values without changing the underlying DSL. */
export function portTargetValue(raw: string, unit = ""): string {
  const value = raw.trim();
  if (!value) return "—";
  const numeric = Number(value);
  if (Number.isFinite(numeric)) return formatKnobDisplay(numeric, unit);
  return /^[a-f]$/i.test(value) ? `Macro ${value.toUpperCase()}` : "Expression: " + value;
}

/** Search operates outside the world transform and never expands a node. */
export function PortBrowser({nodeId,graph,onClose}:{nodeId:string;graph:BoardGraph;onClose:()=>void}) {
  const [query,setQuery]=useState("");
  const [sourceId,setSourceId]=useState("");
  const search=useRef<HTMLInputElement>(null);
  const node=graph.nodes[nodeId]!;
  const spec=chipSpec(node.type,node.args);
  const seen=new Set<string>();
  const sources=Object.values(graph.ports).filter(port=>{
    const key=`${port.nodeId}:${port.jackId}`;
    if(!portIsOutput(port)||port.kind!=="mod"||port.nodeId===nodeId||port.nodeId==="MIDI"||seen.has(key))return false;
    seen.add(key);return true;
  });
  const selected=sources.find(p=>p.id===sourceId)??sources[0];
  useEffect(()=>{
    const previous=document.activeElement;
    search.current?.focus();
    return ()=>{if(previous instanceof HTMLElement&&previous.isConnected)previous.focus();};
  },[]);
  return <section className="nk-port-browser" role="dialog" aria-label={`Connections for ${nodeId}`} data-board-control=""
    onPointerDown={e=>e.stopPropagation()} onWheel={e=>e.stopPropagation()}
    onKeyDown={e=>{e.stopPropagation();if(e.key==="Escape"){e.preventDefault();onClose();}}}>
    <header><strong>{node.label} connections</strong><button type="button" onClick={onClose} aria-label="Close connections">Close</button></header>
    <label>Modulation source<select value={selected?.id??""} onChange={e=>setSourceId(e.target.value)}>
      {!sources.length?<option value="">No modulation source in this patch</option>:null}
      {sources.map(port=><option key={port.id} value={port.id}>{graph.nodes[port.nodeId]?.label || port.nodeId} · {port.jackId === "out" ? "Output" : port.jackId}</option>)}
    </select></label>
    <input ref={search} type="search" aria-label="Search connection targets" placeholder="Search parameters" value={query} onChange={e=>setQuery(e.target.value)} />
    <p>Choose a parameter to connect. Full values are available in Inspect.</p>
    <div className="nk-port-browser-targets">
      {connectionTargets(node.type,node.args,query).map(key=><button type="button" key={key} disabled={!selected}
        title={`${spec.paramJackLabels[key] || key}: ${node.args[key] ?? spec.defaultArgs[key] ?? "—"}`}
        onClick={()=>{
          if(!selected)return;
          const target:BoardPort=graph.ports[portId(nodeId,key,false)]??{
            id:portId(nodeId,key,false),nodeId,jackId:key,face:"n",index:0,count:1,kind:"mod"};
          void commitBoardConnect(selected,target).then(onClose);
        }}><span>{spec.paramJackLabels[key] || key.replace(/[_-]/g," ")}</span><small>{portTargetValue(node.args[key]??spec.defaultArgs[key]??"",spec.ranges[key]?.unit)}</small></button>)}
    </div>
  </section>;
}
