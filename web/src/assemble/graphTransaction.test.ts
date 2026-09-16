import { afterEach,expect,it,vi } from 'vitest';
import { applyCanvasScript } from './addBlock';
import { useAstStore } from '../store/astStore';
import { useBoardStore } from './boardStore';
import { commitBoardConnect,commitBoardCut } from './boardCommit';
import { portId } from './boardModel';
vi.mock('../bridge/juce',()=>({hasJuceBridge:()=>true,getNativeFunction:()=>vi.fn(async()=>({ok:false,error:'Invalid connection'}))}));
afterEach(()=>vi.restoreAllMocks());
it('rejected native connection leaves the script and visual graph unchanged',async()=>{
 applyCanvasScript('stage1: y = x\nfilter1: cutoff = 900\n');
 useBoardStore.getState().hydrate(useAstStore.getState().ast);
 const g=useBoardStore.getState(); const ast=useAstStore.getState().ast;
 await commitBoardConnect(g.ports[portId('IN','out',true)]!,g.ports[portId('filter1','in',false)]!);
 expect(useBoardStore.getState().edges).toEqual(g.edges);
 expect(useAstStore.getState().ast).toBe(ast);
 expect(useAstStore.getState().diagnostics[0]?.message).toContain('Invalid connection');
});
it('disconnect submits one native transaction instead of compiling an unrelated park operation',async()=>{
 applyCanvasScript('stage1: y = x\nfilter1: cutoff = 900\n');
 useBoardStore.getState().hydrate(useAstStore.getState().ast);
 const g=useBoardStore.getState();const script=useAstStore.getState().lastValidScript;
 await commitBoardCut(g.ports[portId('filter1','in',false)]!);
 expect(useBoardStore.getState().edges).toEqual(g.edges);
 expect(useAstStore.getState().lastValidScript).toBe(script);
});
