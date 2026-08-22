import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { subscribeVizClock } from "../theme/vizClock";
import { portGlobal, type BoardCamera, type BoardEdge, type BoardGraph } from "./boardModel";
import { bezierPreview, connectDragRef } from "./boardConnect";
import { stubRoute } from "./boardPath";
import { plasmaSpeedPxPerSec } from "./cableMotion";
import { BOARD_GRID } from "./grid";
import { chamferWaypoints } from "./layout/chamfer";

function colorFor(kind: BoardEdge["kind"]): string {
  if (kind === "mod") {
    return "var(--nk-cyan)";
  }
  if (kind === "sc") {
    return "#e8c44a";
  }
  return "var(--nk-accent)";
}

function resolveColor(css: string, fallback: string): string {
  if (css.startsWith("var(")) {
    const v = getComputedStyle(document.documentElement).getPropertyValue(css.slice(4, -1)).trim();
    return v || fallback;
  }
  return css;
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
    };
    let dash = 0;
    const draw = (now: number) => {
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

      ctx.strokeStyle = "rgba(255,255,255,0.06)";
      ctx.lineWidth = 1 / cam.scale;
      const x0 = Math.floor((-cam.tx) / cam.scale / BOARD_GRID) * BOARD_GRID;
      const y0 = Math.floor((-cam.ty) / cam.scale / BOARD_GRID) * BOARD_GRID;
      const x1 = x0 + width / cam.scale + BOARD_GRID * 2;
      const y1 = y0 + height / cam.scale + BOARD_GRID * 2;
      for (let x = x0; x < x1; x += BOARD_GRID) {
        for (let y = y0; y < y1; y += BOARD_GRID) {
          ctx.beginPath();
          ctx.moveTo(x - 3, y);
          ctx.lineTo(x + 3, y);
          ctx.moveTo(x, y - 3);
          ctx.lineTo(x, y + 3);
          ctx.stroke();
        }
      }

      const clips = useHostStore.getState().clips;
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
        const pts = chamferWaypoints(raw);
        if (pts.length < 2) {
          continue;
        }
        const peak = clips[e.sourceNodeId] ?? 0;
        const speed = plasmaSpeedPxPerSec(peak);
        const col = resolveColor(colorFor(e.kind), e.kind === "mod" ? "#4af" : "#f24");
        ctx.save();
        ctx.strokeStyle = col;
        ctx.shadowColor = col;
        ctx.shadowBlur = 10;
        ctx.lineWidth = 2;
        ctx.lineJoin = "miter";
        ctx.lineCap = "butt";
        ctx.setLineDash([4, 8]);
        ctx.lineDashOffset = speed > 0 ? -(dash * speed) / 60 : 0;
        const hot = focusRef.current;
        const dim = hot && hot.size > 0 && ! hot.has(e.id);
        ctx.globalAlpha = dim ? 0.12 : (speed > 0 ? 0.95 : 0.35);
        ctx.beginPath();
        ctx.moveTo(pts[0]!.x, pts[0]!.y);
        for (let i = 1; i < pts.length; i += 1) {
          ctx.lineTo(pts[i]!.x, pts[i]!.y);
        }
        ctx.stroke();
        ctx.restore();
      }
      const drag = connectDragRef.current;
      if (drag) {
        const { c1, c2 } = bezierPreview(drag.from, drag.to, drag.fromPort.east);
        const col = resolveColor(colorFor(drag.kind), "#f24");
        ctx.save();
        ctx.strokeStyle = col;
        ctx.shadowColor = col;
        ctx.shadowBlur = 12;
        ctx.lineWidth = 2.2;
        ctx.setLineDash([]);
        ctx.beginPath();
        ctx.moveTo(drag.from.x, drag.from.y);
        ctx.bezierCurveTo(c1.x, c1.y, c2.x, c2.y, drag.to.x, drag.to.y);
        ctx.stroke();
        ctx.restore();
      }
      ctx.restore();
      dash = now / 1000;
    };
    const off = subscribeVizClock(draw);
    draw(performance.now());
    return off;
  }, [width, height]);

  return <canvas ref={ref} className="nk-board-canvas" aria-hidden />;
}
