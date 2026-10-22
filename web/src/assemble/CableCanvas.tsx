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
  measureCablePath,
  traceCableSpan,
  CABLE_INK,
  type CablePathMetrics,
  peakForLane,
  rmsForLane,
  shouldCoalescePaint,
  shouldSkipClockPaint,
  type LaneColor,
} from "./cablePaint";
import { cableEnvelope } from "./boardNavigation";
import {
  advanceCableSignal,
  cablePacketAlpha,
  type CablePacket,
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
  ctx: CanvasRenderingContext2D, path: CablePathMetrics, glow: string,
  core: string, width: number, impulses: CablePacket[], peak: number,
  rms: number, bloom: boolean, animate: boolean, dim: boolean,
  clipColor: string, clipAllowed: boolean, scale: number, _dpr: number,
  surface: string, dash: number[] = [], baseOnly = false,
): void {
  const pts = path.points;
  if (pts.length < 2) return;
  const envelope = cableEnvelope(rms, peak);
  const px = 1 / Math.max(0.01, scale);
  const inkWidth = Math.max(width, CABLE_INK.corePx * px);
  const focus = dim ? 0.4 : 1;
  ctx.save();
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  ctx.shadowBlur = 0;
  ctx.setLineDash(dash.map((v) => v * px));
  if (baseOnly) {
    ctx.globalAlpha = 0.9;
    ctx.strokeStyle = surface;
    ctx.lineWidth = Math.max(width + 2, CABLE_INK.outlinePx * px);
    tracePath(ctx, pts); ctx.stroke();
    ctx.globalAlpha = dim ? CABLE_INK.dimAlpha : CABLE_INK.baseAlpha;
    ctx.strokeStyle = glow;
    ctx.lineWidth = inkWidth;
    tracePath(ctx, pts); ctx.stroke();
    ctx.restore();
    return;
  }
  ctx.globalAlpha = envelope.body * focus * 0.65;
  ctx.strokeStyle = core;
  ctx.lineWidth = Math.max(0.8 * px, inkWidth * 0.45);
  tracePath(ctx, pts); ctx.stroke();
  ctx.setLineDash([]);
  if (animate) {
    for (const packet of impulses) {
      const alpha = cablePacketAlpha(packet) * focus;
      const span = (28 + packet.strength * 24) * px;
      // Layered strokes form a soft tail without a per-packet shadow blur.
      // All geometry follows cached route segments, including chamfered corners.
      if (bloom) {
        ctx.beginPath();
        traceCableSpan(ctx, path, packet.distance - span, packet.distance);
        ctx.globalAlpha = alpha * 0.12;
        ctx.strokeStyle = glow;
        ctx.lineWidth = inkWidth + 9 * px;
        ctx.stroke();
        ctx.globalAlpha = alpha * 0.28;
        ctx.lineWidth = inkWidth + 4 * px;
        ctx.stroke();
      }
      ctx.beginPath();
      traceCableSpan(ctx, path, packet.distance - span, packet.distance);
      ctx.globalAlpha = alpha * 0.7;
      ctx.strokeStyle = glow;
      ctx.lineWidth = Math.max(2 * px, inkWidth);
      ctx.stroke();
      ctx.beginPath();
      traceCableSpan(ctx, path, packet.distance - span * 0.36, packet.distance);
      ctx.globalAlpha = alpha;
      ctx.strokeStyle = core;
      ctx.lineWidth = Math.max(1.5 * px, inkWidth * 0.8);
      ctx.stroke();
    }
  }
  ctx.globalAlpha = (0.2 + envelope.contact * 0.6) * focus;
  ctx.fillStyle = glow;
  for (const p of [pts[0]!, pts[pts.length - 1]!]) {
    ctx.beginPath();
    ctx.arc(p.x, p.y, (1.8 + envelope.contact) * px, 0, Math.PI * 2);
    ctx.fill();
  }
  if (envelope.clip && clipAllowed) {
    ctx.globalAlpha = 1;
    ctx.lineWidth = 1.5 * px;
    ctx.strokeStyle = clipColor;
    ctx.beginPath(); ctx.arc(pts[0]!.x, pts[0]!.y, 7 * px, 0, Math.PI * 2); ctx.stroke();
  }
  ctx.restore();
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
      { stamp: string; lanes: Pt[][]; paths: CablePathMetrics[] }
    >();
    const signals = new Map<string, CableSignal>();
    const baseCanvas = document.createElement("canvas");
    const baseCtx = baseCanvas.getContext("2d");
    let baseStamp = "";
    let baseGraph: BoardGraph | null = null;
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
      // `wave` = fliessende Signalimpulse, `dots` = statische Punktestrecke.
      const isDots = host.cables === "dots";
      const dotDash = isDots ? [2, 8] : [];
      const cableAnimate = pass.animate;

      const hot = boardFocusEdgesRef.current;
      const staticStamp = [w, h, dpr, cam.scale, cam.tx, cam.ty, host.theme,
        host.cables, ...(hot ? [...hot].sort() : [])].join("|");
      const dragging = Boolean(chipDragRef.current || chipDragGroupRef.current);
      const repaintBase = !baseCtx || baseGraph !== g || baseStamp !== staticStamp || dragging;
      const base = baseCtx ?? ctx;
      if (repaintBase) {
        if (baseCtx) {
          baseCanvas.width = canvas.width;
          baseCanvas.height = canvas.height;
          baseCtx.setTransform(dpr * cam.scale, 0, 0, dpr * cam.scale, dpr * cam.tx, dpr * cam.ty);
        }
        drawBoardSubstrate(base, Object.values(g.nodes), core, substrate, boardRoutePoints(g));
        drawBackgroundTraces(base, { tx: cam.tx, ty: cam.ty, scale: cam.scale, width: w, height: h });
        baseGraph = dragging ? null : g;
        baseStamp = staticStamp;
      }
      const dynamicPaint: Array<() => void> = [];

      const clips = host.clips;
      const clipsL = host.clipsL;
      const clipsR = host.clipsR;
      const rms = host.clipsRms;
      const rmsL = host.clipsRmsL;
      const rmsR = host.clipsRmsR;
      const tapPeaks = host.clipsPeaks;
      const live = chipDragRef.current;
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
            paths: lanes.map(measureCablePath),
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
        ) {
          // Offscreen packets must not reappear frozen when the camera returns.
          for (const key of signals.keys()) {
            if (key.startsWith(e.id + ":")) signals.delete(key);
          }
          continue;
        }
        const spec = edgeLanes(edgePaintKind(sp, sn), sp.jackId);
        const dim = hot && hot.size > 0 && !hot.has(e.id);
        cached.lanes.forEach((_painted, i) => {
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
            cached.paths[i]?.length ?? 0,
            dt,
            cableAnimate,
            3 / Math.max(0.1, cam.scale),
          );
          const path = cached.paths[i]!;
          const color = isMod && modValue! < 0 ? palette.accent : palette[lane.color];
          if (repaintBase) {
            strokePackets(base, path, palette[lane.color], core, lane.width, [], 0, 0,
              false, false, Boolean(dim), palette.warn, false, cam.scale, dpr, substrate, dotDash, true);
          }
          dynamicPaint.push(() => strokePackets(
            ctx, path, color, core, lane.width,
            cableAnimate ? state.impulses : [],
            measuredPeak * state.freshness, measuredEnergy * state.freshness,
            pass.glow && cam.scale >= 0.45, cableAnimate, Boolean(dim),
            palette.warn, e.kind !== "mod", cam.scale, dpr, substrate, dotDash,
          ));

        });
      }
      if (baseCtx) {
        ctx.save();
        ctx.setTransform(1, 0, 0, 1, 0, 0);
        ctx.drawImage(baseCanvas, 0, 0);
        ctx.restore();
      }
      for (const paint of dynamicPaint) paint();
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
