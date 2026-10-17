import { test, expect } from '@playwright/test';
import { STIR_FS } from '../src/viz/particleShader';

test('the production physics shader compiles and kicks the bowl up with a stereo fan', async ({ page }) => {
  await page.setContent('<!doctype html><html><body></body></html>');
  const result = await page.evaluate((fragment) => {
    const gl = document.createElement('canvas').getContext('webgl2');
    if (!gl || !gl.getExtension('EXT_color_buffer_float')) throw new Error('GPU contract requires WebGL2 float MRT');
    const shader = (kind: number, source: string) => {
      const s=gl.createShader(kind)!; gl.shaderSource(s,source); gl.compileShader(s);
      if(!gl.getShaderParameter(s,gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s) || 'shader failed');
      return s;
    };
    const program=gl.createProgram()!;
    gl.attachShader(program,shader(gl.VERTEX_SHADER,`#version 300 es
      void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}`));
    gl.attachShader(program,shader(gl.FRAGMENT_SHADER,fragment)); gl.linkProgram(program);
    if(!gl.getProgramParameter(program,gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program) || 'link failed');
    gl.useProgram(program);
    const texture = (unit: number, values: number[]) => {
      gl.activeTexture(gl.TEXTURE0+unit);
      const t=gl.createTexture()!;gl.bindTexture(gl.TEXTURE_2D,t);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MIN_FILTER,gl.NEAREST);
      gl.texParameteri(gl.TEXTURE_2D,gl.TEXTURE_MAG_FILTER,gl.NEAREST);
      gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA32F,3,1,0,gl.RGBA,gl.FLOAT,new Float32Array(values));
      return t;
    };
    texture(0,[-.45,-.6,0,0, 0,-.65,0,0, .45,-.6,0,0]);
    texture(1,[1,0,4,0, 1,0,4,0, 1,0,4,0]);
    texture(2,new Array(12).fill(0));texture(3,new Array(12).fill(0));
    for(const [name,unit] of [['tHome',0],['tSeed',1],['tOffset',2],['tVelocity',3]] as const) gl.uniform1i(gl.getUniformLocation(program,name),unit);
    for(const [name,value] of Object.entries({uDt:1/60,uReset:0,uLiftForce:0,uImpulse:1,uSwirl:0,uDustTime:0,uMotion:0,uFill:.5})) gl.uniform1f(gl.getUniformLocation(program,name),value);
    gl.uniform2f(gl.getUniformLocation(program,'uTurn'),0,0);
    const fb=gl.createFramebuffer();gl.bindFramebuffer(gl.FRAMEBUFFER,fb);
    const output0=texture(4,new Array(12).fill(0));
    const output1=texture(5,new Array(12).fill(0));
    gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.TEXTURE_2D,output0,0);
    gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT1,gl.TEXTURE_2D,output1,0);
    gl.drawBuffers([gl.COLOR_ATTACHMENT0,gl.COLOR_ATTACHMENT1]);
    if(gl.checkFramebufferStatus(gl.FRAMEBUFFER)!==gl.FRAMEBUFFER_COMPLETE) throw new Error('MRT incomplete');
    gl.viewport(0,0,3,1);
    const run=(spread:number) => {
      gl.uniform1f(gl.getUniformLocation(program,'uSpread'),spread);gl.drawArrays(gl.TRIANGLES,0,3);
      gl.readBuffer(gl.COLOR_ATTACHMENT1);const pixels=new Float32Array(12);
      gl.readPixels(0,0,3,1,gl.RGBA,gl.FLOAT,pixels);
      if(gl.getError()!==gl.NO_ERROR) throw new Error('GPU readback failed');
      return [...pixels];
    };
    const mono=run(0), wide=run(1);
    texture(0,[-.0001,-.55,0,0, 0,-.55,0,0, .0001,-.55,0,0]);
    const centre=run(1);
    gl.getExtension('WEBGL_lose_context')?.loseContext();
    return {mono,wide,centre};
  }, STIR_FS);
  for(const at of [0,4,8]) {
    expect(result.mono[at+1]).toBeGreaterThan(.8);
    expect(Math.abs(result.mono[at]!)).toBeLessThan(.01);
    expect(result.wide[at+1]).toBeGreaterThan(.8);
  }
  expect(result.wide[0]).toBeLessThan(-.15);
  expect(result.wide[8]).toBeGreaterThan(.15);
  expect(Math.max(...[0,4,8].map(at=>Math.abs(result.centre[at]!)))).toBeLessThan(.001);
});
