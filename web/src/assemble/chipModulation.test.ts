import { describe, expect, it } from 'vitest';
import { canLink } from './boardConnect';
import { hydrateBoard, portId } from './boardModel';
import { demoAst } from './demoAst';

describe('chip parameter contacts', () => {
  it('accepts LFO to parameter and rejects audio to parameter after hydration', () => {
    const graph = hydrateBoard(demoAst.doc);
    const mod = graph.ports[portId('lfo1', 'mod', true)]!;
    const audio = graph.ports[portId('IN', 'out', true)]!;
    const cutoff = graph.ports[portId('filter1', 'cutoff', false)]!;
    expect(mod).toBeDefined();
    expect(cutoff.face).toBe("s");
    expect(canLink(mod, cutoff)).toBe(true);
    expect(canLink(audio, cutoff)).toBe(false);
  });
});
