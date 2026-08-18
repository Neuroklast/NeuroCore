import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { fitCanvas } from "./canvasFit";
import {
  sampleAtPx,
  SCOPE_MENU,
  scopeTitle,
  tracesFor,
  type ScopeYScale,
} from "./scopeModel";

function yOf(v: number, h: number, yScale: ScopeYScale, invert: boolean): number {
  let n = v;
  if (yScale === "db") {
    const abs = Math.max(1.0e-6, Math.abs(v));
    n = Math.max(-1, Math.min(1, (20 * Math.log10(abs) + 60) / 60)) * Math.sign(v || 1);
  }
  if (invert) {
    n = -n;
  }
  return h * 0.5 - n * h * 0.38;
}

export function ScopeCanvas({
  scopeIn,
  scopeOut,
  count,
  sr,
}: {
  scopeIn: Float32Array;
  scopeOut: Float32Array;
  count: number;
  sr: number;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const source = useHostStore((s) => s.scopeSource);
  const delta = useHostStore((s) => s.scopeDelta);
  const xScale = useHostStore((s) => s.scopeX);
  const yScale = useHostStore((s) => s.scopeY);
  const grid = useHostStore((s) => s.scopeGrid);
  const invertY = useHostStore((s) => s.scopeInvertY);

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
    const draw = () => {
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      const n = Math.max(2, count);
      ctx.fillStyle = "#000000";
      ctx.fillRect(0, 0, w, h);
      ctx.strokeStyle = "#3a0000";
      ctx.lineWidth = 1;
      ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
      const plotL = 1;
      const plotR = w - 1;
      const plotT = 14;
      const plotB = h - 12;
      const pw = Math.max(2, plotR - plotL);
      const ph = Math.max(2, plotB - plotT);
      if (grid) {
        ctx.strokeStyle = "rgba(255,26,26,0.18)";
        ctx.beginPath();
        for (let i = 0; i <= 4; i += 1) {
          const y = plotT + (ph * i) / 4;
          ctx.moveTo(plotL, y);
          ctx.lineTo(plotR, y);
        }
        for (let i = 0; i <= 4; i += 1) {
          const x = plotL + (pw * i) / 4;
          ctx.moveTo(x, plotT);
          ctx.lineTo(x, plotB);
        }
        ctx.stroke();
      }
      ctx.strokeStyle = "#3a0000";
      ctx.beginPath();
      ctx.moveTo(plotL, plotT + ph / 2);
      ctx.lineTo(plotR, plotT + ph / 2);
      ctx.stroke();

      const traces = tracesFor(source, delta, scopeIn, scopeOut, n);
      for (const t of traces) {
        ctx.beginPath();
        for (let px = 0; px < pw; px += 1) {
          const x = plotL + px;
          const y = plotT + yOf(sampleAtPx(t.samples, n, px, pw), ph, yScale, invertY);
          if (px === 0) {
            ctx.moveTo(x, y);
          } else {
            ctx.lineTo(x, y);
          }
        }
        ctx.save();
        ctx.strokeStyle = t.color;
        ctx.shadowColor = t.color;
        ctx.shadowBlur = t.id === "delta" ? 3 : 5;
        ctx.lineWidth = t.id === "delta" ? 1.4 : 2;
        ctx.stroke();
        ctx.shadowBlur = 0;
        ctx.lineWidth = t.id === "delta" ? 1 : 1.4;
        ctx.stroke();
        ctx.restore();
      }

      ctx.fillStyle = "#ff003c";
      ctx.font = "11px 'JetBrains Mono', monospace";
      ctx.fillText(scopeTitle(source, delta), 8, 12);
      ctx.fillStyle = "#c8c8c8";
      ctx.font = "9px 'JetBrains Mono', monospace";
      const xLab = xScale === "time" && sr > 0
        ? `${((n / sr) * 1000).toFixed(1)} ms`
        : xScale === "freq" && sr > 0
          ? `${(sr * 0.5).toFixed(0)} Hz`
          : `${n}`;
      ctx.fillText("0", plotL + 4, h - 3);
      ctx.fillText(xLab, plotR - 36, h - 3);
      ctx.fillText(yScale === "db" ? "dB" : "1.0", plotL + 4, plotT + 10);
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [count, delta, grid, invertY, scopeIn, scopeOut, source, sr, xScale, yScale]);

  return <canvas ref={ref} className="block h-full w-full" />;
}

export function ScopeMenu({
  onPick,
}: {
  onPick: (kind: "source" | "x" | "y" | "flag", id: string) => void;
}) {
  return (
    <>
      <div className="nk-ctx-id">SOURCE</div>
      {SCOPE_MENU.source.map((s) => (
        <button key={s.id} type="button" className="nk-ctx-item" onClick={() => onPick("source", s.id)}>
          {s.label}
        </button>
      ))}
      <div className="nk-ctx-id">X AXIS</div>
      {SCOPE_MENU.x.map((s) => (
        <button key={s.id} type="button" className="nk-ctx-item" onClick={() => onPick("x", s.id)}>
          {s.label}
        </button>
      ))}
      <div className="nk-ctx-id">Y AXIS</div>
      {SCOPE_MENU.y.map((s) => (
        <button key={s.id} type="button" className="nk-ctx-item" onClick={() => onPick("y", s.id)}>
          {s.label}
        </button>
      ))}
      {SCOPE_MENU.flags.map((s) => (
        <button key={s.id} type="button" className="nk-ctx-item" onClick={() => onPick("flag", s.id)}>
          {s.label}
        </button>
      ))}
    </>
  );
}
