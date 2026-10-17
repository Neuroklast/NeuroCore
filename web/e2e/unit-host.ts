import type { Page } from '@playwright/test';

/** Exercises the production bridge and binary telemetry decoder, without test hooks in the app. */
export async function installUnitHost(page: Page, theme='signal', motion='off') {
  const n=2048, g=128;
  const frame=Buffer.alloc(32+(n*2+g*2)*4);
  frame.writeUInt32LE(0x4e4b544d,0);frame.writeUInt16LE(1,4);
  for(const [at,value] of [[8,.7],[12,.7],[16,.4],[20,.4],[24,.2]]) frame.writeFloatLE(value!,at!);
  frame.writeUInt16LE(n,28);frame.writeUInt16LE(g,30);
  for(let i=0;i<n;i++) {
    const value=.6*Math.sin(2*Math.PI*55*i/48000);
    frame.writeFloatLE(value,32+i*4);frame.writeFloatLE(value,32+(n+i)*4);
  }
  for(let i=0;i<g;i++) {
    frame.writeFloatLE(.1*Math.sin(i*.2),32+(n*2+i)*4);
    frame.writeFloatLE(.6*Math.sin(i*.2),32+(n*2+g+i)*4);
  }
  await page.route('**/unit-contract-telemetry', route=>route.fulfill({status:200,contentType:'application/octet-stream',body:frame}));
  await page.addInitScript(({ theme, motion })=>{
    const listeners=new Map<string,((p:unknown)=>void)[]>();
    const emit=(id:string,p:unknown)=>listeners.get(id)?.forEach(fn=>fn(p));
    window.__JUCE__={backend:{
      addEventListener(id,fn){ const rows=listeners.get(id)??[];rows.push(fn);listeners.set(id,rows); },
      emitEvent(id,payload){
        if(id!=='__juce__invoke') return;
        const call=payload as {name:string;resultId:number};
        queueMicrotask(()=>{
          if(call.name==='UI_READY') {
            emit('host',{sr:48000,buf:256,lat:32,cpu:20,bpm:120,mode:'STUDIO',os:4,theme,motion,frameRate:30});
            emit('hello',{telemetryPath:'/unit-contract-telemetry'});
          }
          emit('__juce__complete',{promiseId:call.resultId,result:{ok:true}});
        });
      },
    }};
  },{theme,motion});
}
