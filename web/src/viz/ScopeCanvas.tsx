import { useEffect, useRef } from "react";
import { spectrumBins } from "../face/faceModel";
import { useHostStore } from "../store/hostStore";
import { liveTheme, themeRgba } from "../theme/theme";
import { fitCanvas } from "./canvasFit";
import {
  SCOPE_COLOR,
  SCOPE_MENU,
  SPEC_BINS,
  SPEC_DEPTH,
  scopeSpectra,
  scopeTitle,
  specMag01,
  spectrogramProject,
  spectrogramPush,
  techNoise,
} from "./scopeModel";

function liftBins(raw: number[]): number[] {
  return raw.map((v) => specMag01(v));
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
  const themeId = useHostStore((s) => s.theme);
  const histIn = useRef<number[][]>([]);
  const histOut = useRef<number[][]>([]);
  const frame = useRef(0);

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
      const theme = liveTheme();
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      const ids = scopeSpectra(source);
      for (const id of ids) {
        const samples = id === "in" ? scopeIn : scopeOut;
        const hist = id === "in" ? histIn : histOut;
        hist.current = spectrogramPush(hist.current, liftBins(spectrumBins(samples, SPEC_BINS)));
      }
      frame.current += 1;
      ctx.fillStyle = theme.black;
      ctx.fillRect(0, 0, w, h);

      if (grid) {
        ctx.strokeStyle = themeRgba("accent", 0.14, theme);
        ctx.lineWidth = 1;
        ctx.beginPath();
        for (let r = 0; r < SPEC_DEPTH; r += 4) {
          const a = spectrogramProject(0, r, 0, w, h);
          const b = spectrogramProject(SPEC_BINS - 1, r, 0, w, h);
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
        }
        for (let b = 0; b < SPEC_BINS; b += 8) {
          const a = spectrogramProject(b, SPEC_DEPTH - 1, 0, w, h);
          const c = spectrogramProject(b, 0, 0, w, h);
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(c.x, c.y);
        }
        ctx.stroke();
      }

      for (const id of ids) {
        const rows = (id === "in" ? histIn : histOut).current;
        const ink = id === "in" ? theme.cyan : theme.accent;
        const token = id === "in" ? "cyan" : "accent";
        for (let r = rows.length - 1; r >= 0; r -= 1) {
          const row = rows[r]!;
          const fade = 1 - r / Math.max(1, SPEC_DEPTH);
          const pts = row.map((mag, b) => spectrogramProject(b, r, invertY ? 1 - mag : mag, w, h));
          ctx.beginPath();
          const floorL = spectrogramProject(0, r, 0, w, h);
          const floorR = spectrogramProject(SPEC_BINS - 1, r, 0, w, h);
          ctx.moveTo(floorL.x, floorL.y);
          for (const p of pts) {
            ctx.lineTo(p.x, p.y);
          }
          ctx.lineTo(floorR.x, floorR.y);
          ctx.closePath();
          ctx.fillStyle = themeRgba(token, 0.04 + fade * 0.12, theme);
          ctx.fill();
          ctx.strokeStyle = r === 0 ? ink : themeRgba(token, 0.16 + fade * 0.5, theme);
          ctx.shadowColor = r === 0 ? ink : "transparent";
          ctx.shadowBlur = r === 0 ? 6 : 0;
          ctx.lineWidth = r === 0 ? 1.4 : 0.8;
          ctx.stroke();
          ctx.shadowBlur = 0;
        }
      }

      ctx.fillStyle = themeRgba("cyan", 0.08, theme);
      for (let y = 2; y < h; y += 3) {
        ctx.fillRect(1, y, w - 2, 1);
      }
      const f = frame.current;
      ctx.fillStyle = theme.cyan;
      for (let y = 2; y < h; y += 5) {
        for (let x = 2; x < w; x += 7) {
          const n = techNoise(x, y, f);
          if (n > 0) {
            ctx.globalAlpha = 0.18 + n * 0.45;
            ctx.fillRect(x, y, 1, 1);
          }
        }
      }
      ctx.globalAlpha = 1;

      ctx.strokeStyle = themeRgba("accent", 0.55, theme);
      ctx.strokeRect(0.5, 0.5, w - 1, h - 1);
      ctx.fillStyle = SCOPE_COLOR.out;
      ctx.font = "11px 'JetBrains Mono', monospace";
      ctx.fillText(`${scopeTitle(source, delta)}  SPEC`, 8, 12);
      ctx.fillStyle = theme.inkSoft;
      ctx.font = "9px 'JetBrains Mono', monospace";
      const hz = sr > 0 ? `${(sr * 0.5).toFixed(0)} Hz` : "NYQ";
      ctx.fillText("0", 6, h - 4);
      ctx.fillText(hz, w - 40, h - 4);
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [count, delta, grid, invertY, scopeIn, scopeOut, source, sr, themeId, xScale, yScale]);

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
