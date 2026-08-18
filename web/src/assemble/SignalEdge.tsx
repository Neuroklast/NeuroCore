import { BaseEdge, useStore, type EdgeProps } from "@xyflow/react";
import { useHostStore } from "../store/hostStore";
import { motionAllows } from "../theme/motionPolicy";
import { TUBE } from "./tubeModel";
import { snapJackFace } from "./chipLayout";
import { resolveHz } from "./flowFromAst";
import { tubePath, type Obstacle } from "./tubePath";
import { cableAccent, normalizeCableKind } from "./validateLink";

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
  const norm = normalizeCableKind(String((data as { kind?: string } | undefined)?.kind ?? "audio"));
  const kind = norm === "mod" || norm === "param" ? norm : "audio";
  const freqExpr = String((data as { freqExpr?: string } | undefined)?.freqExpr ?? "");
  const hz = kind === "mod"
    ? resolveHz(freqExpr || String((data as { hz?: number } | undefined)?.hz ?? 1), knobs)
    : Number((data as { hz?: number } | undefined)?.hz ?? 1) || 1;
  const lfoMs = Math.max(80, Math.round(1000 / Math.max(hz, 0.05)));
  const sourceId = String((data as { sourceId?: string } | undefined)?.sourceId ?? "");
  const srcKey = sourceId === "IN" ? "in" : "out";

  const outer = kind === "audio" ? TUBE.audioOuter : TUBE.modOuter;
  const glass = kind === "audio" ? TUBE.audioGlass : TUBE.modGlass;
  const bore = kind === "audio" ? TUBE.audioBore : TUBE.modGlass - 2;
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
  const showPlasma = ! temp && kind === "audio" && cables === "wave" && motionAllows("pipeWave", motion, reduced);
  const showDots = ! temp && kind === "audio" && cables === "dots" && motionAllows("pipeWave", motion, reduced);
  const chase = ! temp && kind === "mod" && motionAllows("lfoChase", motion, reduced);
  const accent = cableAccent(kind);
  const tubeClass = kind === "mod" ? "nk-tube-mod" : kind === "param" ? "nk-tube-param" : "";

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
        className={`nk-tube-bloom ${tubeClass}`}
      />
      <path
        d={path}
        fill="none"
        stroke={kind === "audio" ? "#220505" : kind === "param" ? "#2a2a10" : "#3a1010"}
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
        className={`nk-tube-bore ${tubeClass} ${chase ? "nk-bore-chase" : ""}`}
        style={chase ? { animationDuration: `${lfoMs}ms` } : undefined}
      />
      {showPlasma ? (
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
      {showDots ? (
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
