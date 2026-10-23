import {expect,it} from "vitest";
import {stepLedMatrix} from "./stepSeqModel";
it.each([8,16,32,64])("shows %i steps in eight columns",steps=>{
 const matrix=stepLedMatrix({steps:String(steps)},steps-1);
 expect(matrix.columns).toBe(8);expect(matrix.rows).toBe(steps/8);
 expect(matrix.cells).toHaveLength(steps);
 expect(matrix.cells.filter(c=>c.state==="current").map(c=>c.index)).toEqual([steps-1]);
});
it("preserves unused sockets, mute and stopped states without advancing a fake playhead",()=>{
 const args={steps:"3",values:"1,0,-1",mutes:"0,1,0"};
 const stopped=stepLedMatrix(args,-1);
 expect(stopped.cells.map(c=>c.state)).toEqual(["stopped","muted","stopped","unused","unused","unused","unused","unused"]);
 expect(stepLedMatrix(args,1).cells[1]!.state).toBe("muted");
 expect(stepLedMatrix(args,99).cells.some(c=>c.state==="current")).toBe(false);
});
