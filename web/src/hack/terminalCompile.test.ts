import { expect,it,vi } from 'vitest';
import { commitTerminalDraft } from './terminalCompile';
it('does not compile a draft with diagnostics',async()=>{
 const compile=vi.fn();
 const result=await commitTerminalDraft('broken',[{line:1,column:1,message:'Missing colon'}],compile);
 expect(result.ok).toBe(false); expect(compile).not.toHaveBeenCalled();
});
it('turns a rejected or thrown compile into editor diagnostics',async()=>{
 const rejected=await commitTerminalDraft('stage1: y = x',[],async()=>({ok:false,diagnostics:[{line:1,column:1,message:'bad'}]}));
 expect(rejected.ok).toBe(false);
 const thrown=await commitTerminalDraft('stage1: y = x',[],async()=>{throw new Error('bridge lost')});
 expect(thrown.ok).toBe(false); expect(thrown.diagnostics[0]?.message).toContain('bridge lost');
});
