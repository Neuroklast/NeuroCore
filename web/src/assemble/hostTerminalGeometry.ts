import { BOARD_GRID, CHIP_AIR_X, snapToGrid } from "./grid";
type Box={x:number;y:number;w:number;h:number};
const HOST_IDS=new Set(["IN","MIDI","SC"]);

/** A fixed left-edge host lane. DSP hull size must not move the connectors. */
export function placeHostTerminals<T extends Box>(nodes:Record<string,T>):void {
  const inn=nodes.IN;if(!inn)return;
  let y=snapToGrid(inn.y+inn.h+BOARD_GRID);
  for(const id of ["MIDI","SC"]) {
    const node=nodes[id];if(!node)continue;
    nodes[id]={...node,x:inn.x,y};y=snapToGrid(y+node.h+BOARD_GRID);
  }
}

/** Only Arrange/Compact reserves space. Reroute never translates user chips. */
export function reserveHostColumn<T extends Box>(nodes:Record<string,T>, outgoingTracks = 1):void {
  const inn=nodes.IN;if(!inn||!nodes.MIDI&&!nodes.SC)return;
  const chips=Object.entries(nodes).filter(([id])=>!HOST_IDS.has(id));
  if(!chips.length)return;
  // One track per distinct conductor, plus clearance at both port faces.
  const shift=Math.max(0,inn.x+inn.w+Math.max(CHIP_AIR_X, BOARD_GRID * (outgoingTracks + 2))-Math.min(...chips.map(([,node])=>node.x)));
  for(const [id,node] of chips)nodes[id]={...node,x:node.x+shift};
}
