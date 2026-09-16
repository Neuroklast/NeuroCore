import { beforeEach, expect, it } from "vitest";
import { applyCanvasScript } from "./addBlock";
import { commitBind } from "./bindModel";
import { chipSpec, bindableJackKeys } from "./chipSpec";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
beforeEach(()=>{
 useHostStore.setState({knobs:[{id:'a',name:'',value:0,active:false,min:0,max:1,isNote:false}]});
 applyCanvasScript('filter1: type = highpass; cutoff = 1000; resonance = 0.7\n');
});
it('does not expose load-time enums as working realtime modulation',()=>{
 expect(bindableJackKeys(chipSpec('filter'))).not.toContain('type');
 expect(bindableJackKeys(chipSpec('osc'))).not.toContain('shape');
 expect(bindableJackKeys(chipSpec('env'))).not.toContain('unit');
 commitBind('filter1','type','a');
 expect(useAstStore.getState().ast?.nodes.find(n=>n.id==='filter1')?.args.type).toBe('highpass');
});
it('persists the new macro range and binding in the actual script',async()=>{
 await commitBind('filter1','cutoff','a');
 const script=useAstStore.getState().lastValidScript;
 expect(script).toMatch(/param a = .*\[20, 20000\]/);
 expect(script).toContain('cutoff = a');
 expect(useHostStore.getState().knobs[0]!.value).toBeCloseTo(980/19980);
});
it('links an existing macro without overwriting its other destinations',async()=>{
 applyCanvasScript('param a = Drive [1, 10]\nstage1: y = tanh(x*a)\nfilter1: cutoff = 1000\n');
 const knob={id:'a',name:'Drive',value:0.7,active:true,min:1,max:10,isNote:false};
 useHostStore.setState({knobs:[knob]});
 await commitBind('filter1','cutoff','a');
 expect(useHostStore.getState().knobs[0]).toEqual(knob);
 expect(useAstStore.getState().lastValidScript).toContain('param a = Drive [1, 10]');
 expect(useAstStore.getState().lastValidScript).toContain('cutoff = map(a,0,1,20,20000)');
});
it('keeps audio in a formula when adding a gain macro',async()=>{
 applyCanvasScript('custom1: y = tanh(x*3)\n');
 await commitBind('custom1','y','a');
 const expression=useAstStore.getState().ast?.nodes.find(n=>n.id==='custom1')?.args.y;
 expect(expression).toContain('tanh(x*3)');
 expect(expression).toContain('a');
 expect(expression).not.toBe('a');
});
it('labels DSP envelope times in their actual unit',()=>{
 for(const type of ['comp','noisegate','limit']) expect(chipSpec(type).ranges.release?.unit).toBe('s');
});
