import { useEffect, useRef } from "react";
import { spectrumBins } from "../face/faceModel";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { liveTheme, themeRgba } from "../theme/theme";
import { subscribeVizClock } from "../theme/vizClock";
import { fitCanvas } from "./canvasFit";
import {
  SCOPE_MENU,
  SPEC_BINS,
  SPEC_DEPTH,
  deltaSamples,
  isWaveScope,
  scopeSpectra,
  specMag01,
  spectrogramProject,
  spectrogramPush,
  standingWaveRow,
  specRowFade,
  logFreqMarks,
  waveSampleMarks,
  waveTimeMarks,
  scopeYMarks,
  shouldPushScopeRow,
  shouldResetScopeHist,
  SPEC_PAD,
  paintTechNoise,
  waveProject,
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
  xScale: "samples" | "time" | "freq",
  yScale: "linear" | "db",
  nSamples = 256,
): void {
  const xMarks = xScale === "freq"
    ? logFreqMarks(sr)
    : xScale === "time"
      ? waveTimeMarks(sr, nSamples)
      : waveSampleMarks(nSamples);
  const yMarks = scopeYMarks(xScale, yScale);
  const project = isWaveScope(xScale) && yScale === "linear" ? waveProject : spectrogramProject;
  ctx.strokeStyle = "rgba(244,241,234,0.45)";
  ctx.lineWidth = 1;
  ctx.beginPath();
  for (const mark of xMarks) {
    const a = project(mark.bin, SPEC_DEPTH - 1, isWaveScope(xScale) && yScale === "linear" ? 0.5 : 0, w, h);
    const c = project(mark.bin, 0, isWaveScope(xScale) && yScale === "linear" ? 0.5 : 0, w, h);
    ctx.moveTo(a.x, a.y);
    ctx.lineTo(c.x, c.y);
  }
  for (const mark of yMarks) {
    const p = project(0, 0, mark.mag, w, h);
    ctx.moveTo(SPEC_PAD.l - 5, p.y);
    ctx.lineTo(p.x, p.y);
  }
  ctx.stroke();
  ctx.textAlign = "center";
  ctx.textBaseline = "top";
  for (const mark of xMarks) {
    const p = project(mark.bin, 0, isWaveScope(xScale) && yScale === "linear" ? 0.5 : 0, w, h);
    paintAxisLabel(ctx, mark.label, p.x, Math.min(h - 14, p.y + 6), ink);
  }
  ctx.textAlign = "right";
  ctx.textBaseline = "middle";
  const yUnit = xScale === "freq" || yScale === "db" ? " dB" : "";
  for (const mark of yMarks) {
    const p = project(0, 0, mark.mag, w, h);
    paintAxisLabel(ctx, `${mark.label}${yUnit}`, SPEC_PAD.l - 8, p.y, ink);
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
  const xScale = useHostStore((s) => s.scopeX);
  const yScale = useHostStore((s) => s.scopeY);
  const grid = useHostStore((s) => s.scopeGrid);
  const invertY = useHostStore((s) => s.scopeInvertY);
  const themeId = useHostStore((s) => s.theme);
  const delta = useHostStore((s) => s.scopeDelta);
  const tick = useTelemetryStore((s) => s.tick);
  const histIn = useRef<number[][]>([]);
  const histOut = useRef<number[][]>([]);
  const histDelta = useRef<number[][]>([]);
  const frame = useRef(0);
  const lastTick = useRef(-1);
  const prevX = useRef(xScale);
  const tickRef = useRef(tick);
  const inRef = useRef(scopeIn);
  const outRef = useRef(scopeOut);
  inRef.current = scopeIn;
  outRef.current = scopeOut;
  tickRef.current = tick;

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
      if (shouldResetScopeHist(prevX.current, xScale)) {
        histIn.current = [];
        histOut.current = [];
        histDelta.current = [];
      }
      prevX.current = xScale;
      const ids = scopeSpectra(source);
      const wave = isWaveScope(xScale) && yScale === "linear";
      const project = wave ? waveProject : spectrogramProject;
      const zeroMag = wave ? 0.5 : 0;
      if (shouldPushScopeRow(tickRef.current, lastTick.current)) {
        lastTick.current = tickRef.current;
        for (const id of ids) {
          const samples = id === "in" ? inRef.current : outRef.current;
          const hist = id === "in" ? histIn : histOut;
          const row = xScale === "freq"
            ? liftBins(spectrumBins(samples, SPEC_BINS), yScale)
            : standingWaveRow(samples, SPEC_BINS, yScale);
          hist.current = spectrogramPush(hist.current, row);
        }
        if (delta) {
          const d = deltaSamples(outRef.current, inRef.current, outRef.current.length);
          const row = xScale === "freq"
            ? liftBins(spectrumBins(d, SPEC_BINS), yScale)
            : standingWaveRow(d, SPEC_BINS, yScale);
          histDelta.current = spectrogramPush(histDelta.current, row);
        } else {
          histDelta.current = [];
        }
      }
      frame.current += 1;
      ctx.fillStyle = theme.black;
      ctx.fillRect(0, 0, w, h);

      const layers: Array<{ rows: number[][]; token: "cyan" | "accent" | "warn"; ink: string }> = [];
      if (ids.includes("in")) {
        layers.push({ rows: histIn.current, token: "cyan", ink: theme.cyan });
      }
      if (ids.includes("out")) {
        layers.push({ rows: histOut.current, token: "accent", ink: theme.accent });
      }
      if (delta && histDelta.current.length > 0) {
        layers.push({ rows: histDelta.current, token: "warn", ink: theme.warn });
      }
      for (const layer of layers) {
        for (let r = layer.rows.length - 1; r >= 0; r -= 1) {
          const row = layer.rows[r]!;
          const fade = specRowFade(r);
          const pts = row.map((mag, b) => project(b, r, invertY ? 1 - mag : mag, w, h));
          ctx.beginPath();
          if (wave) {
            ctx.moveTo(pts[0]!.x, pts[0]!.y);
            for (let i = 1; i < pts.length; i += 1) {
              ctx.lineTo(pts[i]!.x, pts[i]!.y);
            }
          } else {
            const floorL = project(0, r, zeroMag, w, h);
            const floorR = project(SPEC_BINS - 1, r, zeroMag, w, h);
            ctx.moveTo(floorL.x, floorL.y);
            for (const p of pts) {
              ctx.lineTo(p.x, p.y);
            }
            ctx.lineTo(floorR.x, floorR.y);
            ctx.closePath();
            ctx.fillStyle = themeRgba(layer.token, fade * 0.16, theme);
            ctx.fill();
          }
          ctx.strokeStyle = r === 0 ? layer.ink : themeRgba(layer.token, 0.05 + fade * 0.55, theme);
          ctx.shadowColor = r === 0 ? layer.ink : "transparent";
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
          const a = project(0, r, zeroMag, w, h);
          const b = project(SPEC_BINS - 1, r, zeroMag, w, h);
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
        }
        ctx.stroke();
      }

      paintSpectrogramAxes(ctx, w, h, sr, theme.cyan, xScale, yScale, inRef.current.length);
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
