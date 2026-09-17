import { expect, it } from 'vitest';
import { scriptAfterSetArg, scriptAfterRemove, scriptAfterInsertAfter, scriptAfterRename } from './addBlock';
it('preserves inline comments and spacing when replacing one value', () => {
 const s='filter1: cutoff = 800; resonance = 0.2 # keep this\n';
 expect(scriptAfterSetArg(s,'filter1','resonance','0.5')).toBe('filter1: cutoff = 800; resonance = 0.5 # keep this\n');
});
it('removes a whole multiline expression and inserts after its closing line', () => {
 const block='stage1: y = tanh(\n  x * 2\n)\n';
 const s=block+'out: main = 1\n';
 expect(scriptAfterRemove(s,'stage1')).toBe('out: main = 1\n');
 expect(scriptAfterInsertAfter(s,'stage1','filter')).toContain('x * 2\n)\nfilter1:');
});
it('renames identifier tokens without rewriting comments', () => {
 const s='# osc1 describes modulation\nosc1: freq = 1\nstage1: y = x * osc1 # osc1\n';
 expect(scriptAfterRename(s,'osc1','osc2')).toBe('# osc1 describes modulation\nosc2: freq = 1\nstage1: y = x * osc2 # osc1\n');
});
it('keeps document comments when building the browser sketch', async () => {
 const {parseDslSketch}=await import('../presets/parseDslSketch');
 expect(parseDslSketch('# My patch\nstage1: y = x\n').doc.leadingComments).toContain('My patch');
});
