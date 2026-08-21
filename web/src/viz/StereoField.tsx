import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { liveTheme, themeRgba } from "../theme/theme";
import { subscribeVizClock } from "../theme/vizClock";
import { fitCanvas } from "./canvasFit";
import { gonioPoint, SCOPE_COLOR } from "./scopeModel";

/** Output L/R as a vector-scope cloud. Never a filled IN scribble. */
export function StereoField({
  gonioL,
  gonioR,
  count,
}: {
  gonioL: Float32Array;
  gonioR: Float32Array;
  count: number;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const theme = useHostStore((s) => s.theme);
  const lRef = useRef(gonioL);
  const rRef = useRef(gonioR);
  lRef.current = gonioL;
  rRef.current = gonioR;

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    const draw = () => {
      const t = liveTheme();
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.globalAlpha = 1;
      ctx.fillStyle = t.black;
      ctx.fillRect(0, 0, w, h);
      const pad = 8;
      const side = Math.max(8, Math.min(w, h) - pad * 2);
      const x0 = (w - side) / 2;
      const y0 = (h - side) / 2;
      const cx = x0 + side / 2;
      const cy = y0 + side / 2;
      const rad = side * 0.38;
      ctx.strokeStyle = themeRgba("cyan", 0.35, t);
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(cx, y0);
      ctx.lineTo(cx, y0 + side);
      ctx.moveTo(x0, cy);
      ctx.lineTo(x0 + side, cy);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(cx, cy, rad, 0, Math.PI * 2);
      ctx.moveTo(x0 + 6, y0 + 6);
      ctx.lineTo(x0 + side - 6, y0 + side - 6);
      ctx.moveTo(x0 + side - 6, y0 + 6);
      ctx.lineTo(x0 + 6, y0 + side - 6);
      ctx.stroke();

      const L = lRef.current;
      const R = rRef.current;
      const n = Math.max(0, Math.min(count, L.length, R.length));
      const ink = SCOPE_COLOR.out;
      ctx.fillStyle = ink;
      ctx.shadowColor = ink;
      ctx.shadowBlur = 5;
      for (let i = 0; i < n; i += 1) {
        const p = gonioPoint(L[i] ?? 0, R[i] ?? 0);
        const x = cx + p.x * rad;
        const y = cy + p.y * rad;
        ctx.fillRect(x - 0.8, y - 0.8, 1.6, 1.6);
      }
      ctx.shadowBlur = 0;
    };
    return subscribeVizClock(draw);
  }, [count, theme]);

  return <canvas ref={ref} className="block h-full w-full" />;
}
