import { BaseEdge, useStore, type EdgeProps } from "@xyflow/react";
import { useHostStore } from "../store/hostStore";
import { motionAllows } from "../theme/motionPolicy";
import { TUBE } from "./tubeModel";
import { snapJackFace } from "./chipLayout";
import { resolveHz } from "./flowFromAst";
import { tubePath, type Obstacle } from "./tubePath";

export function SignalEdge({
  id,
  sourceX,
  sourceY,
  targetX,
  targetY,
  source,
  target,
  sourcePosition,
  targetPosition,
  markerEnd,
  data,
}: EdgeProps) {
  void sourcePosition;
  void targetPosition;
  const obstacles = useStore((s) => {
    const list: Obstacle[] = [];
    s.nodeLookup.forEach((n) => {
      const io = n.type === "io";
      list.push({
        id: n.id,
        x: n.internals.positionAbsolute.x,
        y: n.internals.positionAbsolute.y,
        w: n.measured.width ?? n.width ?? (io ? 96 : 220),
        h: n.measured.height ?? n.height ?? (io ? 56 : 80),
      });
    });
    return list;
  });
  const cables = useHostStore((s) => s.cables);
  const motion = useHostStore((s) => s.motion);
  const knobs = useHostStore((s) => s.knobs);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const temp = Boolean((data as { temp?: boolean } | undefined)?.temp);
  const kind = (data as { kind?: string } | undefined)?.kind === "mod" ? "mod" : "audio";
  const freqExpr = String((data as { freqExpr?: string } | undefined)?.freqExpr ?? "");
  const hz = kind === "mod"
    ? resolveHz(freqExpr || String((data as { hz?: number } | undefined)?.hz ?? 1), knobs)
    : Number((data as { hz?: number } | undefined)?.hz ?? 1) || 1;
  const lfoMs = Math.max(80, Math.round(1000 / Math.max(hz, 0.05)));
  const sourceId = String((data as { sourceId?: string } | undefined)?.sourceId ?? "");
  const srcKey = sourceId === "IN" ? "in" : "out";

  const outer = kind === "mod" ? TUBE.modOuter : TUBE.audioOuter;
  const glass = kind === "mod" ? TUBE.modGlass : TUBE.audioGlass;
  const bore = kind === "mod" ? TUBE.modGlass - 2 : TUBE.audioBore;
  const reserved = useStore((s) => {
    const list: Array<Array<{ x: number; y: number }>> = [];
    s.edges.forEach((e) => {
      const pts = (e.data as { points?: Array<{ x: number; y: number }> } | undefined)?.points;
      if (e.id !== id && pts && pts.length > 1) {
        list.push(pts);
      }
    });
    return list;
  });
  const ready = (data as { route?: string } | undefined)?.route;
  const srcFace = obstacles.find((o) => o.id === source);
  const tgtFace = obstacles.find((o) => o.id === target);
  const from = srcFace ? snapJackFace(sourceX, sourceY, srcFace, true) : { x: sourceX, y: sourceY };
  const to = tgtFace ? snapJackFace(targetX, targetY, tgtFace, false) : { x: targetX, y: targetY };
  const path = ready || tubePath(from.x, from.y, to.x, to.y, {
    obstacles,
    sourceId: source,
    targetId: target,
    reserved,
  }).d;
  const showPlasma = ! temp && cables === "wave" && motionAllows("pipeWave", motion, reduced);
  const showDots = ! temp && cables === "dots" && motionAllows("pipeWave", motion, reduced);
  const chase = ! temp && kind === "mod" && motionAllows("lfoChase", motion, reduced);
  const accent = kind === "mod" ? "#00f0ff" : "#ff003c";

  return (
    <>
      <BaseEdge
        id={id}
        path={path}
        style={{ stroke: "transparent", strokeWidth: outer + 4, fill: "none" }}
        markerEnd={markerEnd}
      />
      <path
        d={path}
        fill="none"
        stroke={accent}
        strokeWidth={outer + 2}
        strokeLinecap="butt"
        strokeLinejoin="miter"
        className={`nk-tube-bloom ${kind === "mod" ? "nk-tube-mod" : ""}`}
      />
      <path
        d={path}
        fill="none"
        stroke={kind === "mod" ? "#3a1010" : "#220505"}
        strokeWidth={outer}
        strokeLinecap="butt"
        strokeLinejoin="miter"
        opacity={0.96}
      />
      <path
        d={path}
        fill="none"
        stroke="#050505"
        strokeWidth={glass}
        strokeLinecap="butt"
        strokeLinejoin="miter"
        opacity={0.94}
      />
      <path
        d={path}
        fill="none"
        stroke={accent}
        strokeWidth={bore}
        strokeLinecap="butt"
        strokeLinejoin="miter"
        data-src={srcKey}
        className={`nk-tube-bore ${kind === "mod" ? "nk-tube-mod" : ""} ${chase ? "nk-bore-chase" : ""}`}
        style={chase ? { animationDuration: `${lfoMs}ms` } : undefined}
      />
      {showPlasma && kind === "audio" ? (
        <path
          d={path}
          fill="none"
          stroke={accent}
          strokeWidth="1.15"
          strokeLinecap="butt"
          strokeLinejoin="miter"
          data-src={srcKey}
          className="nk-plasma"
        />
      ) : null}
      {showDots && kind === "audio" ? (
        <path
          d={path}
          fill="none"
          stroke={accent}
          strokeWidth="2.2"
          strokeLinecap="round"
          strokeLinejoin="miter"
          data-src={srcKey}
          className="nk-dots"
        />
      ) : null}
    </>
  );
}
