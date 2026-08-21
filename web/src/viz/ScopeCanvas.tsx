import { useEffect, useRef } from "react";
import { spectrumBins } from "../face/faceModel";
import { useHostStore } from "../store/hostStore";
import { liveTheme, themeRgba } from "../theme/theme";
import { subscribeVizClock } from "../theme/vizClock";
import { fitCanvas } from "./canvasFit";
import {
  SCOPE_MENU,
  SPEC_BINS,
  SPEC_DEPTH,
  sampleAtPx,
  scopeSpectra,
  specMag01,
  spectrogramProject,
  spectrogramPush,
  specRowFade,
  logFreqMarks,
  specDbMarks,
  specInner,
  SPEC_PAD,
  paintTechNoise,
  tracesFor,
  waveXMarks,
} from "./scopeModel";

function paintAxisLabel(
  ctx: CanvasRenderingContext2D,
  text: string,
  x: number,
  y: number,
  fill: string,
): void {
  ctx.font = "9px Apex, 'JetBrains Mono', ui-monospace, monospace";
  ctx.lineWidth = 3;
  ctx.strokeStyle = "#000000";
  ctx.lineJoin = "round";
  ctx.strokeText(text, x, y);
  ctx.fillStyle = fill;
  ctx.fillText(text, x, y);
}

function paintSpectrogramAxes(
  ctx: CanvasRenderingContext2D,
  w: number,
  h: number,
  sr: number,
  ink: string,
): void {
  const freq = logFreqMarks(sr);
  const db = specDbMarks();
  ctx.strokeStyle = "rgba(244,241,234,0.45)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (const mark of freq) {
    const a = spectrogramProject(mark.bin, SPEC_DEPTH - 1, 0, w, h);
    const c = spectrogramProject(mark.bin, 0, 0, w, h);
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(c.x, c.y);
  }
  for (const mark of db) {
    const p = spectrogramProject(0, 0, mark.mag, w, h);
    ctx.moveTo(SPEC_PAD.l - 5, p.y);
    ctx.lineTo(p.x, p.y);
  }
  ctx.stroke();
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  for (const mark of freq) {
    const p = spectrogramProject(mark.bin, 0, 0, w, h);
    paintAxisLabel(ctx, mark.label, p.x, Math.min(h - 14, p.y + 6), ink);
  }
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  for (const mark of db) {
    const p = spectrogramProject(0, 0, mark.mag, w, h);
    paintAxisLabel(ctx, `${mark.label} dB`, SPEC_PAD.l - 8, p.y, ink);
  }
}

function waveY(
  v: number,
  innerY: number,
  innerH: number,
  yScale: "linear" | "db",
  invertY: boolean,
): number {
  if (yScale === "db") {
    const mag = specMag01(Math.abs(v));
    const ny = invertY ? 1 - mag : mag;
    return innerY + innerH * (1 - ny);
  }
  const a = invertY ? -v : v;
  return innerY + innerH * 0.5 - a * innerH * 0.42;
}

function paintWaveform(
  ctx: CanvasRenderingContext2D,
  w: number,
  h: number,
  sr: number,
  n: number,
  scopeIn: Float32Array,
  scopeOut: Float32Array,
  source: "in" | "out" | "both",
  delta: boolean,
  xScale: "samples" | "time",
  yScale: "linear" | "db",
  invertY: boolean,
  grid: boolean,
  ink: string,
): void {
  const inner = specInner(w, h);
  const traces = tracesFor(source, delta, scopeIn, scopeOut, n);
  const marks = waveXMarks(xScale, n, sr);

  if (grid) {
    ctx.strokeStyle = "rgba(0, 240, 255, 0.10)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (const mark of marks) {
      const x = inner.x + mark.t * inner.w;
      ctx.moveTo(x, inner.y);
      ctx.lineTo(x, inner.y + inner.h);
    }
    const mids = yScale === "db" ? [0, 0.33, 0.66, 1] : [0, 0.5, 1];
    for (const t of mids) {
      const y = inner.y + t * inner.h;
      ctx.moveTo(inner.x, y);
      ctx.lineTo(inner.x + inner.w, y);
    }
    ctx.stroke();
  }

  for (const tr of traces) {
    ctx.beginPath();
    const span = Math.max(2, inner.w);
    for (let px = 0; px < span; px += 1) {
      const v = sampleAtPx(tr.samples, n, px, span);
      const x = inner.x + px;
      const y = waveY(v, inner.y, inner.h, yScale, invertY);
      if (px === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.strokeStyle = tr.color;
    ctx.shadowColor = tr.color;
    ctx.shadowBlur = 6;
    ctx.lineWidth = 1.6;
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  ctx.strokeStyle = "rgba(244,241,234,0.45)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(inner.x, inner.y);
  ctx.lineTo(inner.x, inner.y + inner.h);
  ctx.lineTo(inner.x + inner.w, inner.y + inner.h);
  ctx.stroke();

  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  for (const mark of marks) {
    const x = inner.x + mark.t * inner.w;
    paintAxisLabel(ctx, mark.label, x, Math.min(h - 14, inner.y + inner.h + 4), ink);
  }
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  if (yScale === "db") {
    for (const mark of specDbMarks()) {
      const y = inner.y + inner.h * (1 - mark.mag);
      paintAxisLabel(ctx, `${mark.label} dB`, SPEC_PAD.l - 8, y, ink);
    }
  } else {
    paintAxisLabel(ctx, "+1", SPEC_PAD.l - 8, waveY(1, inner.y, inner.h, "linear", false), ink);
    paintAxisLabel(ctx, "0", SPEC_PAD.l - 8, waveY(0, inner.y, inner.h, "linear", false), ink);
    paintAxisLabel(ctx, "−1", SPEC_PAD.l - 8, waveY(-1, inner.y, inner.h, "linear", false), ink);
  }
}

function liftBins(raw: number[], yScale: "linear" | "db"): number[] {
  if (yScale === "linear") {
    return raw.map((v) => Math.max(0, Math.min(1, Math.abs(v) * 8)));
  }
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
  const inRef = useRef(scopeIn);
  const outRef = useRef(scopeOut);
  inRef.current = scopeIn;
  outRef.current = scopeOut;

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
      const theme = liveTheme();
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      frame.current += 1;
      ctx.fillStyle = theme.black;
      ctx.fillRect(0, 0, w, h);

      if (xScale !== "freq") {
        paintWaveform(
          ctx, w, h, sr, Math.max(2, inRef.current.length),
          inRef.current, outRef.current, source, delta,
          xScale, yScale, invertY, grid, theme.cyan,
        );
        paintTechNoise(ctx, w, h, frame.current, theme.cyan);
        return;
      }

      const ids = scopeSpectra(source);
      for (const id of ids) {
        const samples = id === "in" ? inRef.current : outRef.current;
        const hist = id === "in" ? histIn : histOut;
        hist.current = spectrogramPush(hist.current, liftBins(spectrumBins(samples, SPEC_BINS), yScale));
      }

      for (const id of ids) {
        const rows = (id === "in" ? histIn : histOut).current;
        const ink = id === "in" ? theme.cyan : theme.accent;
        const token = id === "in" ? "cyan" : "accent";
        for (let r = rows.length - 1; r >= 0; r -= 1) {
          const row = rows[r]!;
          const fade = specRowFade(r);
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
          ctx.fillStyle = themeRgba(token, fade * 0.16, theme);
          ctx.fill();
          ctx.strokeStyle = r === 0 ? ink : themeRgba(token, 0.05 + fade * 0.55, theme);
          ctx.shadowColor = r === 0 ? ink : "transparent";
          ctx.shadowBlur = r === 0 ? 6 : 0;
          ctx.lineWidth = r === 0 ? 1.6 : 0.7;
          ctx.stroke();
          ctx.shadowBlur = 0;
        }
      }

      ctx.fillStyle = themeRgba("cyan", 0.08, theme);
      for (let y = 2; y < h; y += 3) {
        ctx.fillRect(1, y, w - 2, 1);
      }
      paintTechNoise(ctx, w, h, frame.current, theme.cyan);

      if (grid) {
        ctx.strokeStyle = themeRgba("cyan", 0.08, theme);
        ctx.lineWidth = 1;
        ctx.beginPath();
        for (let r = 0; r < SPEC_DEPTH; r += 4) {
          const a = spectrogramProject(0, r, 0, w, h);
          const b = spectrogramProject(SPEC_BINS - 1, r, 0, w, h);
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
        }
        ctx.stroke();
      }

      paintSpectrogramAxes(ctx, w, h, sr, theme.cyan);
    };
    return subscribeVizClock(draw);
  }, [count, delta, grid, invertY, source, sr, themeId, xScale, yScale]);

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
