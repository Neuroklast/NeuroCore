import { useEffect, useRef } from "react";
import type { TelemetryViews } from "../bridge/telemetry";
import { subscribeVizClock } from "../theme/vizClock";

const VS = `attribute vec2 a; void main(){ gl_Position = vec4(a,0.0,1.0); gl_PointSize = 2.0; }`;
const FS = `precision mediump float; void main(){ gl_FragColor = vec4(1.0,0.102,0.102,0.85); }`;

export function GonioGl({ views, title }: { views: TelemetryViews; title: string }) {
  const ref = useRef<HTMLCanvasElement>(null);
  const viewsRef = useRef(views);
  viewsRef.current = views;

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const gl = canvas.getContext("webgl2") || canvas.getContext("webgl");
    if (! gl) {
      return;
    }
    const compile = (type: number, src: string) => {
      const s = gl.createShader(type);
      if (! s) {
        return null;
      }
      gl.shaderSource(s, src);
      gl.compileShader(s);
      return s;
    };
    const vs = compile(gl.VERTEX_SHADER, VS);
    const fs = compile(gl.FRAGMENT_SHADER, FS);
    const prog = gl.createProgram();
    if (! vs || ! fs || ! prog) {
      return;
    }
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);
    const buf = gl.createBuffer();
    const packed = new Float32Array(256);
    const draw = () => {
      const v = viewsRef.current;
      const n = Math.min(v.gonioN, 128);
      for (let i = 0; i < n; i += 1) {
        packed[i * 2] = v.gonioX[i];
        packed[i * 2 + 1] = v.gonioY[i];
      }
      gl.viewport(0, 0, canvas.width, canvas.height);
      gl.clearColor(0, 0, 0, 1);
      gl.clear(gl.COLOR_BUFFER_BIT);
      gl.useProgram(prog);
      gl.bindBuffer(gl.ARRAY_BUFFER, buf);
      gl.bufferData(gl.ARRAY_BUFFER, packed, gl.DYNAMIC_DRAW);
      const loc = gl.getAttribLocation(prog, "a");
      gl.enableVertexAttribArray(loc);
      gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
      gl.drawArrays(gl.POINTS, 0, n);
    };
    return subscribeVizClock(draw);
  }, []);

  return (
    <div className="relative h-full w-[148px] shrink-0 border border-accent/55">
      <canvas ref={ref} width={148} height={148} className="h-full w-full" />
      <span className="pointer-events-none absolute left-1 top-1 text-[10px] text-accent">{title}</span>
    </div>
  );
}
