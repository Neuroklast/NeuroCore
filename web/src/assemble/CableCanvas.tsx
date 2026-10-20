import { boardRoutePoints } from "./boardBounds";
import { drawBoardSubstrate } from "./boardSubstrate";
import { useEffect, useRef, type MutableRefObject } from "react";
import { useHostStore } from "../store/hostStore";
import { liveTheme } from "../theme/theme";
import { subscribeVizClock } from "../theme/vizClock";
import { perfBegin, perfEnd } from "../perf/marks";
import {
  portGlobal,
  type BoardCamera,
  type BoardEdge,
  type BoardGraph,
} from "./boardModel";
import { chipDragGroupRef, chipDragRef, nodeWithDrag } from "./boardDrag";
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
  pathLength,
  peakForLane,
  pointAlong,
  rmsForLane,
  shouldCoalescePaint,
  shouldSkipClockPaint,
  streamAlpha,
  streamBlur,
  type LaneColor,
} from "./cablePaint";
import { cableEnvelope } from "./boardNavigation";
import {
  advanceCableSignal,
  emptyCableSignal,
  type CableSignal,
} from "./cableSignal";
import type { Pt } from "./layout/types";

const COLOR_VAR: Record<LaneColor, string> = {
  cyan: "var(--nk-cyan)",
  accent: "var(--nk-accent)",
  warn: "var(--nk-sc)",
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
    const v = getComputedStyle(document.documentElement)
      .getPropertyValue(css.slice(4, -1))
      .trim();
    return v || fallback;
  }
  return css;
}

/** Only used when the computed CSS variable is empty (no live theme yet). */
function fallbackColor(k: LaneColor): string {
  const t = liveTheme();
  return k === "cyan" ? t.cyan : k === "accent" ? t.accent : t.sc;
}

function tracePath(
  ctx: CanvasRenderingContext2D,
  pts: Array<{ x: number; y: number }>,
): void {
  ctx.beginPath();
  ctx.moveTo(pts[0]!.x, pts[0]!.y);
  for (let i = 1; i < pts.length; i += 1) {
    ctx.lineTo(pts[i]!.x, pts[i]!.y);
  }
}

function strokePackets(
  ctx: CanvasRenderingContext2D,
  pts: Pt[],
  glow: string,
  core: string,
  width: number,
  distances: number[],
  peak: number,
  rms: number,
  bloom: boolean,
  animate: boolean,
  dim: boolean,
  clipColor: string,
  clipAllowed: boolean,
): void {
  if (pts.length < 2) return;
  const envelope = cableEnvelope(rms, peak);
  const length = pathLength(pts);
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  ctx.lineWidth = width;
  ctx.setLineDash([]);
  ctx.shadowBlur = 0;
  ctx.globalCompositeOperation = "source-over";
  ctx.globalAlpha = dim ? 0.24 : 0.48;
  ctx.strokeStyle = glow;
  tracePath(ctx, pts);
  ctx.stroke();
  // Sustained source energy is a steady thin core, independent of transient density.
  ctx.globalAlpha = envelope.body * (dim ? 0.25 : 1);
  ctx.strokeStyle = core;
  ctx.lineWidth = Math.max(0.8, width * 0.45);
  ctx.shadowColor = glow;
  ctx.shadowBlur = bloom ? Math.min(5, streamBlur(peak) * 0.35) : 0;
  tracePath(ctx, pts);
  ctx.stroke();
  if (animate && envelope.body > 0) {
    for (let tail = 3; tail >= 0; tail--) {
      ctx.globalAlpha = streamAlpha(peak) * (1 - tail / 4) * (dim ? 0.25 : 1);
      ctx.strokeStyle = tail === 0 ? core : glow;
      ctx.lineWidth = width * (tail === 0 ? 1.05 : 0.8);
      ctx.beginPath();
      for (const head of distances) {
        const a = pointAlong(pts, Math.max(0, head - tail * 7 - 6)),
          b = pointAlong(pts, Math.min(length, Math.max(0, head - tail * 7)));
        if (a && b) {
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
        }
      }
      ctx.stroke();
    }
  }
  // Both contacts refer to this source tap; the far contact is not a destination meter.
  ctx.globalAlpha = envelope.contact * (dim ? 0.2 : 0.8);
  ctx.fillStyle = glow;
  ctx.shadowBlur = bloom ? 4 : 0;
  for (const p of [pts[0]!, pts[pts.length - 1]!]) {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 1.8 + envelope.contact, 0, Math.PI * 2);
    ctx.fill();
  }
  if (envelope.clip && clipAllowed) {
    const p = pts[0]!;
    ctx.globalAlpha = 1;
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = clipColor;
    ctx.beginPath();
    ctx.arc(p.x, p.y, 7, 0, Math.PI * 2);
    ctx.stroke();
  }
  ctx.globalCompositeOperation = "source-over";
  ctx.shadowBlur = 0;
  ctx.globalAlpha = 1;
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
  const sizeRef = useRef({ width, height });
  sizeRef.current = { width, height };

  useEffect(() => {
    const canvas = ref.current;
    if (!canvas || !active) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (!ctx) {
      return;
    }
    const palette: Record<LaneColor, string> = {
      cyan: fallbackColor("cyan"),
      accent: fallbackColor("accent"),
      warn: fallbackColor("warn"),
    };
    const geom = new Map<
      string,
      { stamp: string; lanes: Pt[][]; lengths: number[] }
    >();
    const signals = new Map<string, CableSignal>();
    let lastNow = 0;
    let gesturePaintAt = 0;
    let paintPending = false;
    let paintRaf = 0;
    let paletteTheme = "";
    const coreInk = () => resolveColor(PACKET_CORE, liveTheme().white);
    let core = fallbackColor("accent");
    let substrate = "rgb(20,24,26)";
    const draw = (now = 0) => {
      const fromClock = now > 0;
      const t = fromClock
        ? now
        : typeof performance !== "undefined"
          ? performance.now()
          : 0;
      if (
        fromClock &&
        shouldSkipClockPaint(t, gesturePaintAt, 8, paintPending)
      ) {
        return;
      }
      perfBegin("cables");
      const dt =
        lastNow > 0 ? Math.min(0.1, Math.max(0, (t - lastNow) / 1000)) : 0;
      lastNow = t;
      if (!fromClock) {
        gesturePaintAt = t;
      }
      const { width: w, height: h } = sizeRef.current;
      const g = graphRef.current;
      const cam = cameraRef.current;
      const dpr = Math.min(
        window.devicePixelRatio || 1,
        Math.sqrt(4000000 / Math.max(1, w * h)),
      );
      if (
        canvas.width !== Math.floor(w * dpr) ||
        canvas.height !== Math.floor(h * dpr)
      ) {
        canvas.width = Math.floor(w * dpr);
        canvas.height = Math.floor(h * dpr);
        canvas.style.width = `${w}px`;
        canvas.style.height = `${h}px`;
      }
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.clearRect(0, 0, w, h);
      ctx.save();
      ctx.setTransform(
        dpr * cam.scale,
        0,
        0,
        dpr * cam.scale,
        dpr * cam.tx,
        dpr * cam.ty,
      );

      const host = useHostStore.getState();
      if (host.theme !== paletteTheme) {
        paletteTheme = host.theme;
        (Object.keys(COLOR_VAR) as LaneColor[]).forEach((k) => {
          palette[k] = resolveColor(COLOR_VAR[k], fallbackColor(k));
        });
        core = coreInk();
        substrate = resolveColor("var(--nk-surface)", "rgb(20,24,26)");
      }
      const pass = cablePaintPass(gestureRef.current, host.motion);

      {
        drawBoardSubstrate(ctx,Object.values(g.nodes),core,substrate,boardRoutePoints(g));
        drawBackgroundTraces(ctx, {
          tx: cam.tx,
          ty: cam.ty,
          scale: cam.scale,
          width: w,
          height: h,
        });
      }

      const clips = host.clips;
      const clipsL = host.clipsL;
      const clipsR = host.clipsR;
      const rms = host.clipsRms;
      const rmsL = host.clipsRmsL;
      const rmsR = host.clipsRmsR;
      const tapPeaks = host.clipsPeaks;
      const live = chipDragRef.current;
      const hot = boardFocusEdgesRef.current;
      for (const id of geom.keys()) if (!g.edges[id]) geom.delete(id);
      for (const key of signals.keys())
        if (!g.edges[key.slice(0, key.lastIndexOf(":"))]) signals.delete(key);
      for (const e of Object.values(g.edges)) {
        const sn0 = g.nodes[e.sourceNodeId];
        const tn0 = g.nodes[e.targetNodeId];
        const sp = g.ports[e.sourcePortId];
        const tp = g.ports[e.targetPortId];
        if (!sn0 || !tn0 || !sp || !tp) {
          continue;
        }
        const sn = nodeWithDrag(
          sn0,
          chipDragGroupRef.current?.[sn0.id] ?? live,
        );
        const tn = nodeWithDrag(
          tn0,
          chipDragGroupRef.current?.[tn0.id] ?? live,
        );
        const from = portGlobal(sn, sp);
        const to = portGlobal(tn, tp);
        const stamp = cableGeomStamp(
          e.id,
          e.route,
          from,
          to,
          e.kind,
          sp.jackId,
        );
        let cached = geom.get(e.id);
        if (!cached || cached.stamp !== stamp) {
          const lanes = buildCableLanes(from, to, e.route, e.kind, sp.jackId);
          cached = {
            stamp,
            lanes,
            lengths: lanes.map((lane) => pathLength(lane)),
          };
          geom.set(e.id, cached);
        }
        if (cached.lanes.length === 0) {
          continue;
        }
        const points = cached.lanes[0]!;
        const x0 = -cam.tx / cam.scale - 40,
          y0 = -cam.ty / cam.scale - 40,
          x1 = x0 + w / cam.scale + 80,
          y1 = y0 + h / cam.scale + 80;
        if (
          points.every((p) => p.x < x0) ||
          points.every((p) => p.x > x1) ||
          points.every((p) => p.y < y0) ||
          points.every((p) => p.y > y1)
        )
          continue;
        const spec = edgeLanes(edgePaintKind(sp, sn), sp.jackId);
        const dim = hot && hot.size > 0 && !hot.has(e.id);
        cached.lanes.forEach((painted, i) => {
          const lane = spec[i];
          if (!lane) {
            return;
          }
          const peak = peakForLane(
            lane.id,
            e.sourceNodeId,
            clips,
            clipsL,
            clipsR,
            sn.type,
            sp.jackId,
            sn.busName,
          );
          const energy = rmsForLane(
            lane.id,
            e.sourceNodeId,
            rms,
            rmsL,
            rmsR,
            sn.type,
            sp.jackId,
            sn.busName,
          );
          const key = `${e.id}:${lane.id}`;
          const modValue = host.mods[e.sourceNodeId];
          const isMod = e.kind === "mod" && Number.isFinite(modValue);
          const measuredPeak = isMod ? Math.abs(modValue!) : peak;
          const measuredEnergy = isMod ? Math.abs(modValue!) : energy;
          const state = signals.get(key) ?? emptyCableSignal();
          signals.set(key, state);
          advanceCableSignal(
            state,
            isMod ? host.mods : tapPeaks,
            isMod ? [0] : (tapPeaks[e.sourceNodeId] ?? tapPeaks[sn.id] ?? []),
            measuredEnergy,
            measuredPeak,
            cached.lengths[i] ?? 0,
            dt,
            pass.animate,
          );
          const beads = pass.animate ? state.packets : [];
          strokePackets(
            ctx,
            painted,
            isMod && modValue! < 0 ? palette.accent : palette[lane.color],
            core,
            lane.width,
            beads,
            measuredPeak * state.freshness,
            measuredEnergy * state.freshness,
            pass.glow,
            pass.animate,
            Boolean(dim),
            palette.warn,
            e.kind !== "mod",
          );
        });
      }
      const drag = connectDragRef.current;
      if (drag) {
        const { c1, c2 } = bezierPreview(
          drag.from,
          drag.to,
          drag.fromPort.east,
        );
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
      perfEnd("cables");
    };
    drawNow = () => {
      if (shouldCoalescePaint(paintPending)) {
        return;
      }
      paintPending = true;
      paintRaf = window.requestAnimationFrame(() => {
        paintPending = false;
        paintRaf = 0;
        draw(0);
      });
    };
    const off = subscribeVizClock(draw);
    draw();
    return () => {
      if (paintRaf) {
        window.cancelAnimationFrame(paintRaf);
        paintRaf = 0;
      }
      paintPending = false;
      if (drawNow) {
        drawNow = null;
      }
      off();
    };
  }, [active, cameraRef, gestureRef]);

  return <canvas ref={ref} className="nk-board-canvas" aria-hidden />;
}
