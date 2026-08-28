import { useEffect, useRef, type MutableRefObject } from "react";
import { useHostStore } from "../store/hostStore";
import { subscribeVizClock } from "../theme/vizClock";
import { portGlobal, type BoardCamera, type BoardEdge, type BoardGraph } from "./boardModel";
import { chipDragRef, nodeWithDrag } from "./boardDrag";
import { bezierPreview, connectDragRef } from "./boardConnect";
import { boardFocusEdgesRef } from "./circuitDof";
import {
  buildCableLanes,
  cableGeomStamp,
  cablePaintPass,
  drawBackgroundTraces,
  edgeLanes,
  edgePaintKind,
  PACKET_CORE,
  peakForLane,
  rmsForLane,
  streamAlpha,
  streamBlur,
  streamDash,
  streamGlitch,
  streamSpeed,
  type LaneColor,
} from "./cablePaint";
import type { Pt } from "./layout/types";

const COLOR_VAR: Record<LaneColor, string> = {
  cyan: "var(--nk-cyan)",
  accent: "var(--nk-accent)",
  warn: "var(--nk-warn)",
};

function colorForKind(kind: BoardEdge["kind"]): LaneColor {
  if (kind === "mod") {
    return "cyan";
  }
  if (kind === "sc") {
    return "warn";
  }
  return "accent";
}

function resolveColor(css: string, fallback: string): string {
  if (css.startsWith("var(")) {
    const v = getComputedStyle(document.documentElement).getPropertyValue(css.slice(4, -1)).trim();
    return v || fallback;
  }
  return css;
}

const FALLBACK: Record<LaneColor, string> = {
  cyan: "#3cf6ff",
  accent: "#ff003c",
  warn: "#e8c44a",
};

function tracePath(ctx: CanvasRenderingContext2D, pts: Array<{ x: number; y: number }>): void {
  ctx.beginPath();
  ctx.moveTo(pts[0]!.x, pts[0]!.y);
  for (let i = 1; i < pts.length; i += 1) {
    ctx.lineTo(pts[i]!.x, pts[i]!.y);
  }
}

function strokeStream(
  ctx: CanvasRenderingContext2D,
  pts: Array<{ x: number; y: number }>,
  glow: string,
  core: string,
  width: number,
  dash: number[],
  dashOffset: number,
  peak: number,
  bloom: boolean,
): void {
  ctx.lineCap = "butt";
  ctx.lineJoin = "miter";
  ctx.lineWidth = width;
  ctx.setLineDash([]);
  ctx.shadowBlur = 0;
  ctx.globalCompositeOperation = "source-over";
  ctx.globalAlpha = 0.15;
  ctx.strokeStyle = glow;
  tracePath(ctx, pts);
  ctx.stroke();

  ctx.setLineDash(dash);
  ctx.lineDashOffset = dashOffset;
  ctx.globalCompositeOperation = "screen";
  const glitch = streamGlitch(peak);
  if (glitch > 0) {
    ctx.shadowBlur = 0;
    ctx.globalAlpha = 1;
    ctx.save();
    ctx.translate(-glitch, 0);
    ctx.strokeStyle = "rgba(255, 0, 60, 1)";
    tracePath(ctx, pts);
    ctx.stroke();
    ctx.restore();
    ctx.save();
    ctx.translate(glitch, 0);
    ctx.strokeStyle = "rgba(0, 255, 255, 1)";
    tracePath(ctx, pts);
    ctx.stroke();
    ctx.restore();
    ctx.strokeStyle = "#FFFFFF";
    tracePath(ctx, pts);
    ctx.stroke();
  } else {
    ctx.globalAlpha = streamAlpha(peak);
    ctx.shadowColor = glow;
    ctx.shadowBlur = bloom ? streamBlur(peak) : 0;
    ctx.strokeStyle = core;
    tracePath(ctx, pts);
    ctx.stroke();
  }
  ctx.globalCompositeOperation = "source-over";
  ctx.shadowBlur = 0;
  ctx.setLineDash([]);
}

let drawNow: (() => void) | null = null;

export function paintCablesNow(): void {
  drawNow?.();
}

export function CableCanvas({
  graph,
  cameraRef,
  gestureRef,
  width,
  height,
  active = true,
}: {
  graph: BoardGraph;
  cameraRef: MutableRefObject<BoardCamera>;
  gestureRef: MutableRefObject<boolean>;
  width: number;
  height: number;
  active?: boolean;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const graphRef = useRef(graph);
  graphRef.current = graph;

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas || ! active) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    const offsets = new Map<string, number>();
    const palette: Record<LaneColor, string> = { ...FALLBACK };
    const geom = new Map<string, { stamp: string; lanes: Pt[][] }>();
    let paletteTheme = "";
    const coreInk = () => resolveColor(PACKET_CORE, "#ffffff");
    let core = FALLBACK.accent;
    const draw = () => {
      const g = graphRef.current;
      const cam = cameraRef.current;
      const dpr = window.devicePixelRatio || 1;
      if (canvas.width !== Math.floor(width * dpr) || canvas.height !== Math.floor(height * dpr)) {
        canvas.width = Math.floor(width * dpr);
        canvas.height = Math.floor(height * dpr);
        canvas.style.width = `${width}px`;
        canvas.style.height = `${height}px`;
      }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, width, height);
      ctx.save();
      ctx.setTransform(dpr * cam.scale, 0, 0, dpr * cam.scale, dpr * cam.tx, dpr * cam.ty);

      const host = useHostStore.getState();
      if (host.theme !== paletteTheme) {
        paletteTheme = host.theme;
        (Object.keys(COLOR_VAR) as LaneColor[]).forEach((k) => {
          palette[k] = resolveColor(COLOR_VAR[k], FALLBACK[k]);
        });
        core = coreInk();
      }
      const pass = cablePaintPass(gestureRef.current, host.motion);

      if (pass.traces) {
        drawBackgroundTraces(ctx, {
          tx: cam.tx,
          ty: cam.ty,
          scale: cam.scale,
          width,
          height,
        });
      }

      const clips = host.clips;
      const clipsL = host.clipsL;
      const clipsR = host.clipsR;
      const rms = host.clipsRms;
      const rmsL = host.clipsRmsL;
      const rmsR = host.clipsRmsR;
      const live = chipDragRef.current;
      const hot = boardFocusEdgesRef.current;
      for (const e of Object.values(g.edges)) {
        const sn0 = g.nodes[e.sourceNodeId];
        const tn0 = g.nodes[e.targetNodeId];
        const sp = g.ports[e.sourcePortId];
        const tp = g.ports[e.targetPortId];
        if (! sn0 || ! tn0 || ! sp || ! tp) {
          continue;
        }
        const sn = nodeWithDrag(sn0, live);
        const tn = nodeWithDrag(tn0, live);
        const from = portGlobal(sn, sp);
        const to = portGlobal(tn, tp);
        const stamp = cableGeomStamp(e.id, e.route, from, to, e.kind, sp.jackId);
        let cached = geom.get(e.id);
        if (! cached || cached.stamp !== stamp) {
          cached = { stamp, lanes: buildCableLanes(from, to, e.route, e.kind, sp.jackId) };
          geom.set(e.id, cached);
        }
        if (cached.lanes.length === 0) {
          continue;
        }
        const spec = edgeLanes(edgePaintKind(sp, sn), sp.jackId);
        const dim = hot && hot.size > 0 && ! hot.has(e.id);
        cached.lanes.forEach((painted, i) => {
          const lane = spec[i];
          if (! lane) {
            return;
          }
          const peak = peakForLane(lane.id, e.sourceNodeId, clips, clipsL, clipsR, sn.type);
          const energy = rmsForLane(lane.id, e.sourceNodeId, rms, rmsL, rmsR, sn.type);
          const key = `${e.id}:${lane.id}`;
          let off = offsets.get(key) ?? 0;
          const spd = streamSpeed(energy);
          if (spd > 0) {
            off -= spd;
            offsets.set(key, off);
          }
          ctx.save();
          if (dim) {
            ctx.globalAlpha = 0.12;
          }
          strokeStream(
            ctx,
            painted,
            palette[lane.color],
            core,
            lane.width,
            streamDash(lane.dash, energy),
            off,
            peak,
            pass.glow,
          );
          ctx.restore();
        });
      }
      const drag = connectDragRef.current;
      if (drag) {
        const { c1, c2 } = bezierPreview(drag.from, drag.to, drag.fromPort.east);
        const col = palette[colorForKind(drag.kind)];
        ctx.save();
        ctx.strokeStyle = col;
        ctx.shadowColor = col;
        ctx.shadowBlur = pass.glow ? 4 : 0;
        ctx.lineWidth = 2.2;
        ctx.setLineDash([]);
        ctx.beginPath();
        ctx.moveTo(drag.from.x, drag.from.y);
        ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, drag.to.x, drag.to.y);
        ctx.stroke();
        ctx.restore();
      }
      ctx.restore();
    };
    drawNow = draw;
    const off = subscribeVizClock(draw);
    draw();
    return () => {
      if (drawNow === draw) {
        drawNow = null;
      }
      off();
    };
  }, [width, height, cameraRef, gestureRef, active]);

  return <canvas ref={ref} className="nk-board-canvas" aria-hidden />;
}
