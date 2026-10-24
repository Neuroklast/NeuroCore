import {expect,it} from 'vitest';
import {readMsegPoints,formatMsegPoints,msegAt,moveMsegPoint,insertMsegPoint,deleteMsegPoint,msegEditorPosition,msegEditorValueAt,msegEditorPointer,msegEditorPath} from './msegModel';
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

import {bindableJackKeys,chipSpec} from './chipSpec';
it('only native runtime expressions are offered as control-block macro targets',()=>{
 expect(bindableJackKeys(chipSpec('mseg'))).toEqual(['duration']);
 expect(bindableJackKeys(chipSpec('modmix'))).toEqual(['levela','levelb','mix','offset']);
});

it('draws inspector MSEG nodes in matching pixel units at multiple aspect ratios',()=>{
 const points=readMsegPoints('0:0:0,0.25:1:2,0.75:-1:0,1:0:0');
 for(const size of [{width:300,height:180},{width:800,height:180},{width:800,height:260}]){
  for(const point of points){
   const local=msegEditorPosition(point.time,point.value,size);
   const reverse=msegEditorValueAt(local.x,local.y,size);
   expect(reverse.time).toBeCloseTo(point.time,6);
   expect(reverse.value).toBeCloseTo(point.value,6);
   const scaled=msegEditorPointer(130+local.x*1.5,40+local.y*1.5,
     {left:130,top:40,width:size.width*1.5,height:size.height*1.5},size);
   expect(scaled.time).toBeCloseTo(point.time,6);
   expect(scaled.value).toBeCloseTo(point.value,6);
  }
  expect(msegEditorPath(points,size).startsWith('M')).toBe(true);
  expect(msegEditorPath(points,size).includes('NaN')).toBe(false);
  expect(msegEditorPosition(0,1,size).y).toBe(14);
 }
 const narrow=msegEditorPosition(0.5,0,{width:300,height:180});
 const wide=msegEditorPosition(0.5,0,{width:800,height:180});
 expect(wide.y).toBe(narrow.y);
 expect(wide.x).toBeGreaterThan(narrow.x);
});
