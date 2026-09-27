import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { useTheme } from "../theme/themeBind";
import { subscribeVizClock } from "../theme/vizClock";
import { fitCanvas } from "../viz/canvasFit";
import {
  buildTraces,
  PHASE_STEP,
  resolvePlotExpression,
  type PreviewWave,
} from "./plotModel";

export function FunctionPlot({
  name,
  example,
  wave = "sine",
}: {
  name: string;
  example: string;
  wave?: PreviewWave;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const motion = useHostStore((s) => s.motion);
  const theme = useTheme();

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    const expr = resolvePlotExpression(name, example);
    let phase = 0;
    let last = performance.now();
    const drawWave = (w: number, y0: number, h: number, samples: Float32Array, color: string, tag: string) => {
      ctx.fillStyle = theme.surfaceHigh;
      ctx.fillRect(8, y0, w - 16, h);
      ctx.strokeStyle = theme.accentDeep;
      ctx.lineWidth = 1;
      ctx.strokeRect(8.5, y0 + 0.5, w - 17, h - 1);
      const mid = y0 + h * 0.5;
      ctx.strokeStyle = "rgba(200,200,200,0.3)";
      ctx.beginPath();
      ctx.moveTo(12, mid);
      ctx.lineTo(w - 12, mid);
      ctx.stroke();
      ctx.fillStyle = color;
      ctx.font = "13px 'JetBrains Mono', monospace";
      ctx.fillText(tag, 16, y0 + 16);
      ctx.strokeStyle = color;
      ctx.lineWidth = 1;
      ctx.beginPath();
      const amp = h * 0.38;
      for (let i = 0; i < samples.length; i += 1) {
        const x = 12 + (i / (samples.length - 1)) * (w - 24);
        const y = mid - samples[i] * amp;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    };
    const tick = (now: number) => {
      const animate = motion !== "off";
      if (animate) {
        phase += PHASE_STEP * Math.min(2, (now - last) / 33);
        if (phase > Math.PI * 2) phase -= Math.PI * 2;
      }
      last = now;
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.fillStyle = theme.black;
      ctx.fillRect(0, 0, w, h);
      ctx.strokeStyle = `rgba(${theme.accentRgb}, 0.45)`;
      ctx.lineWidth = 1;
      ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
      const { inn, out, ok } = buildTraces(expr, phase, undefined, wave);
      drawWave(w, 8, h * 0.46, inn, theme.cyan, `IN  ${wave}`);
      drawWave(w, h * 0.52, h * 0.46, out, ok ? theme.accent : theme.inkMuted,
        ok ? `OUT  ${name || "f(x)"}` : "OUT  (no demo)");
    };
    return subscribeVizClock(tick);
  }, [example, motion, name, theme, wave]);

  return <canvas ref={ref} className="h-[220px] w-full" />;
}
