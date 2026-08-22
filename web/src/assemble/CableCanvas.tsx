import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { subscribeVizClock } from "../theme/vizClock";
import { portGlobal, type BoardCamera, type BoardEdge, type BoardGraph } from "./boardModel";
import { bezierPreview, connectDragRef } from "./boardConnect";
import { stubRoute } from "./boardPath";
import {
  edgeLanes,
  edgePaintKind,
  PACKET_CORE,
  parallelOffset,
  drawBackgroundTraces,
  peakForLane,
  rmsForLane,
  sideBreakaway,
  streamAlpha,
  streamBlur,
  streamDash,
  streamGlitch,
  streamSpeed,
  type LaneColor,
} from "./cablePaint";
import { chamferWaypoints } from "./layout/chamfer";

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
    ctx.shadowBlur = streamBlur(peak);
    ctx.strokeStyle = core;
    tracePath(ctx, pts);
    ctx.stroke();
  }
  ctx.globalCompositeOperation = "source-over";
  ctx.shadowBlur = 0;
  ctx.setLineDash([]);
}

export function CableCanvas({
  graph,
  camera,
  width,
  height,
  focusEdgeIds,
}: {
  graph: BoardGraph;
  camera: BoardCamera;
  width: number;
  height: number;
  focusEdgeIds?: Set<string> | null;
}) {
  const ref = useRef<HTMLCanvasElement>(null);
  const graphRef = useRef(graph);
  const camRef = useRef(camera);
  const focusRef = useRef(focusEdgeIds);
  graphRef.current = graph;
  camRef.current = camera;
  focusRef.current = focusEdgeIds;

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    const offsets = new Map<string, number>();
    const palette: Record<LaneColor, string> = { ...FALLBACK };
    const draw = () => {
      const g = graphRef.current;
      const cam = camRef.current;
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

      (Object.keys(COLOR_VAR) as LaneColor[]).forEach((k) => {
        palette[k] = resolveColor(COLOR_VAR[k], FALLBACK[k]);
      });

      drawBackgroundTraces(ctx, {
        tx: cam.tx,
        ty: cam.ty,
        scale: cam.scale,
        width,
        height,
      });

      const host = useHostStore.getState();
      const clips = host.clips;
      const clipsL = host.clipsL;
      const clipsR = host.clipsR;
      const rms = host.clipsRms;
      const rmsL = host.clipsRmsL;
      const rmsR = host.clipsRmsR;
      for (const e of Object.values(g.edges)) {
        const sn = g.nodes[e.sourceNodeId];
        const tn = g.nodes[e.targetNodeId];
        const sp = g.ports[e.sourcePortId];
        const tp = g.ports[e.targetPortId];
        if (! sn || ! tn || ! sp || ! tp) {
          continue;
        }
        const from = portGlobal(sn, sp);
        const to = portGlobal(tn, tp);
        const raw = e.route.length >= 2 ? e.route : stubRoute(from, to);
        let pts = chamferWaypoints(raw);
        if (pts.length < 2) {
          continue;
        }
        const kind = edgePaintKind(sp, sn);
        if (kind === "side") {
          pts = sideBreakaway(pts);
        }
        const lanes = edgeLanes(kind, sp.jackId);
        const hot = focusRef.current;
        const dim = hot && hot.size > 0 && ! hot.has(e.id);
        for (const lane of lanes) {
          const painted = lane.offset !== 0 ? parallelOffset(pts, lane.offset) : pts;
          const peak = peakForLane(lane.id, e.sourceNodeId, clips, clipsL, clipsR);
          const energy = rmsForLane(lane.id, e.sourceNodeId, rms, rmsL, rmsR);
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
            resolveColor(PACKET_CORE, "#ffffff"),
            lane.width,
            streamDash(lane.dash, energy),
            off,
            peak,
          );
          ctx.restore();
        }
      }
      const drag = connectDragRef.current;
      if (drag) {
        const { c1, c2 } = bezierPreview(drag.from, drag.to, drag.fromPort.east);
        const col = palette[colorForKind(drag.kind)];
        ctx.save();
        ctx.strokeStyle = col;
        ctx.shadowColor = col;
        ctx.shadowBlur = 4;
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
    const off = subscribeVizClock(draw);
    draw();
    return off;
  }, [width, height]);

  return <canvas ref={ref} className="nk-board-canvas" aria-hidden />;
}
