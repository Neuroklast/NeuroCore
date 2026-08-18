import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { fitCanvas } from "./canvasFit";
import { fieldTitle, gonioPoint, SCOPE_COLOR } from "./scopeModel";

export function StereoField({
  gonioL,
  gonioR,
  scopeIn,
  count,
}: {
  gonioL: Float32Array;
  gonioR: Float32Array;
  scopeIn: Float32Array;
  count: number;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const source = useHostStore((s) => s.scopeSource);

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    let raf = 0;
    const drawTrace = (color: string, get: (i: number) => { x: number; y: number }, n: number, cx: number, cy: number, hx: number, hy: number) => {
      ctx.strokeStyle = color;
      ctx.globalAlpha = 0.22;
      ctx.lineWidth = 3;
      ctx.beginPath();
      for (let i = 0; i < n; i += 1) {
        const p = get(i);
        const x = cx + p.x * hx;
        const y = cy + p.y * hy;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.globalAlpha = 0.95;
      ctx.lineWidth = 1.1;
      ctx.stroke();
    };
    const draw = () => {
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.globalAlpha = 1;
      ctx.fillStyle = "#000";
      ctx.fillRect(0, 0, w, h);
      const side = Math.min(w - 12, h - 28);
      const x0 = (w - side) / 2;
      const y0 = 16;
      ctx.strokeStyle = "rgba(255,26,26,0.55)";
      ctx.strokeRect(x0 + 0.5, y0 + 0.5, side - 1, side - 1);
      const cx = x0 + side / 2;
      const cy = y0 + side / 2;
      ctx.strokeStyle = "rgba(255,26,26,0.22)";
      ctx.beginPath();
      ctx.moveTo(cx, y0);
      ctx.lineTo(cx, y0 + side);
      ctx.moveTo(x0, cy);
      ctx.lineTo(x0 + side, cy);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(cx, cy, side * 0.42, 0, Math.PI * 2);
      ctx.moveTo(x0, y0);
      ctx.lineTo(x0 + side, y0 + side);
      ctx.moveTo(x0 + side, y0);
      ctx.lineTo(x0, y0 + side);
      ctx.stroke();

      const n = Math.max(2, Math.min(count, gonioL.length, gonioR.length));
      const hx = side * 0.46;
      const hy = side * 0.46;
      if (source === "in" || source === "both") {
        const inn = Math.min(n, scopeIn.length);
        drawTrace(SCOPE_COLOR.in, (i) => {
          const s = scopeIn[i] ?? 0;
          return gonioPoint(s, s);
        }, inn, cx, cy, hx, hy);
      }
      if (source === "out" || source === "both") {
        drawTrace(SCOPE_COLOR.out, (i) => gonioPoint(gonioL[i] ?? 0, gonioR[i] ?? 0), n, cx, cy, hx, hy);
      }

      ctx.globalAlpha = 1;
      ctx.fillStyle = "#ff003c";
      ctx.font = "10px 'JetBrains Mono', monospace";
      ctx.fillText(fieldTitle(source), 6, 12);
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [count, gonioL, gonioR, scopeIn, source]);

  return <canvas ref={ref} className="block h-full w-full" />;
}
