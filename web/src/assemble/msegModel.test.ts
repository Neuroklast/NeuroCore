import {expect,it} from 'vitest';
import {readMsegPoints,formatMsegPoints,msegAt,moveMsegPoint,insertMsegPoint,deleteMsegPoint} from './msegModel';
it('round-trips valid bounded point curves with the native interpolation',()=>{
 const points=readMsegPoints('0:0:2,0.5:1:0,1:0:0');
 expect(msegAt(points,0.25)).toBeCloseTo(0.0625);
 expect(readMsegPoints(formatMsegPoints(points))).toEqual(points);
 expect(readMsegPoints('0:0,0:1')).toHaveLength(3); // display default; native compile rejects invalid text
});
it('point edits preserve fixed endpoints, strict time order and the 64 point limit',()=>{
 let points=readMsegPoints('0:0,0.25:1,1:0');
 points=moveMsegPoint(points,1,2,-2);
 expect(points[1]!.time).toBeLessThan(1);expect(points[1]!.value).toBe(-1);
 expect(moveMsegPoint(points,0,0.5,0.3)[0]!.time).toBe(0);
 points=insertMsegPoint(points);expect(points).toHaveLength(4);
 expect(deleteMsegPoint(points,0)).toHaveLength(4);
 expect(deleteMsegPoint(points,1)).toHaveLength(3);
 for(let i=0;i<70;i++)points=insertMsegPoint(points);
 expect(points).toHaveLength(64);
});
