import { expect, it } from 'vitest';
import { parseLfoShape, lfoWave } from './lfoLamp';
it('distinguishes noise and soft saw from sine and rounded square',()=>{
 expect(parseLfoShape('noise')).toBe('noise');
 expect(parseLfoShape('soft_saw')).toBe('softsaw');
 expect(parseLfoShape('ramp')).toBe('saw');
});
it('shows continuously rounded square edges instead of a square step',()=>{
 expect(lfoWave(0,'softsquare')).toBeCloseTo(.5);
 expect(lfoWave(.01,'softsquare')).toBeLessThan(.65);
 expect(lfoWave(.25,'softsquare')).toBeCloseTo(1);
});
