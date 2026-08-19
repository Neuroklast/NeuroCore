import { BaseEdge, useStore, type EdgeProps } from "@xyflow/react";
import { useHostStore } from "../store/hostStore";
import { motionAllows } from "../theme/motionPolicy";
import { TUBE } from "./tubeModel";
import { lfoChaseMs, lfoDash, lfoDotGlow, resolveAmp, LFO_DOT, LFO_WIRE } from "./cableMotion";
import { audioStepPath, pathLength, type Obstacle } from "./audioStep";
import { resolveLfoHz } from "./lfoLamp";
import { isEnvNode, isLfoNode } from "./flowFromAst";
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
  const cables = useHostStore((s) => s.cables);
  const motion = useHostStore((s) => s.motion);
  const knobs = useHostStore((s) => s.knobs);
  const bpm = useHostStore((s) => s.bpm);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const temp = Boolean((data as { temp?: boolean } | undefined)?.temp);
  const norm = normalizeCableKind(String((data as { kind?: string } | undefined)?.kind ?? "audio"));
  const kind = norm === "mod" || norm === "param" ? norm : "audio";
  const freqExpr = String((data as { freqExpr?: string } | undefined)?.freqExpr ?? "");
  const syncExpr = String((data as { syncExpr?: string } | undefined)?.syncExpr ?? "off");
  const sourceType = String((data as { sourceType?: string } | undefined)?.sourceType ?? "");
  const sourceId = String((data as { sourceId?: string } | undefined)?.sourceId ?? "");
  const lfoSrc = isLfoNode({ type: sourceType, id: sourceId });
  const envSrc = isEnvNode({ type: sourceType, id: sourceId });
  const envAmp = useHostStore((s) => (envSrc ? (s.mods[sourceId] ?? 0) : 0));
  const hz = kind === "mod" && lfoSrc
    ? resolveLfoHz({ freq: freqExpr || "1", sync: syncExpr }, knobs, bpm)
    : Number((data as { hz?: number } | undefined)?.hz ?? 1) || 1;
  const depthExpr = String((data as { depthExpr?: string } | undefined)?.depthExpr ?? "1");
  const amp = kind === "mod" ? (envSrc ? envAmp : resolveAmp(depthExpr, knobs)) : 1;
  const lfoMs = lfoChaseMs(hz);
  const srcKey = sourceId === "IN" ? "in" : "out";

  const outer = kind === "audio" ? TUBE.audioOuter : TUBE.modOuter;
  const glass = kind === "audio" ? TUBE.audioGlass : TUBE.modGlass;
  const bore = kind === "audio" ? TUBE.audioBore : TUBE.modGlass - 2;
  const reservedXs = useStore((s) => {
    const xs: number[] = [];
    s.edges.forEach((e) => {
      const cx = (e.data as { centerX?: number } | undefined)?.centerX;
      if (e.id !== id && typeof cx === "number") {
        xs.push(cx);
      }
    });
    return xs;
  });
  const obstacles = useStore((s) => {
    const list: Obstacle[] = [];
    s.nodeLookup.forEach((n) => {
      list.push({
        id: n.id,
        x: n.internals.positionAbsolute.x,
        y: n.internals.positionAbsolute.y,
        w: n.measured.width ?? n.width ?? 220,
        h: n.measured.height ?? n.height ?? 80,
      });
    });
    return list;
  });
  const stored = data as { route?: string; centerX?: number; points?: Array<{ x: number; y: number }> } | undefined;
  const step = stored?.route && stored.points && stored.points.length >= 2
    ? { d: stored.route, points: stored.points, centerX: stored.centerX ?? 0 }
    : audioStepPath(sourceX, sourceY, targetX, targetY, {
      sourcePosition,
      targetPosition,
      centerX: stored?.centerX,
      reservedXs,
      obstacles,
      sourceId: source,
      targetId: target,
    });
  const path = step.d;
  const len = pathLength(step.points);
  const lfo = lfoDash(len, LFO_DOT);
  const showPlasma = ! temp && kind === "audio" && cables === "wave" && motionAllows("pipeWave", motion, reduced);
  const showDots = ! temp && kind === "audio" && cables === "dots" && motionAllows("pipeWave", motion, reduced);
  const chase = ! temp && kind === "mod" && lfoSrc && motionAllows("lfoChase", motion, reduced);
  const accent = cableAccent(kind);
  const tubeClass = kind === "mod" ? "nk-tube-mod" : kind === "param" ? "nk-tube-param" : "";
  const focus = String((data as { focus?: string } | undefined)?.focus ?? "off");

  if (kind === "mod") {
    const glow = lfoDotGlow(amp);
    return (
      <g className="nk-dof-edge" data-focus={focus}>
        <BaseEdge
          id={id}
          path={path}
          style={{ stroke: "transparent", strokeWidth: 10, fill: "none" }}
          markerEnd={markerEnd}
        />
        <path
          d={path}
          fill="none"
          stroke="var(--nk-cyan)"
          strokeWidth={LFO_WIRE}
          strokeLinecap="butt"
          strokeLinejoin="miter"
          className="nk-lfo-wire"
          style={envSrc ? { opacity: 0.18 + 0.82 * amp } : undefined}
        />
        {envSrc && amp > 0.04 ? (
          <path
            d={path}
            fill="none"
            stroke="var(--nk-cyan)"
            strokeWidth={LFO_DOT * (0.35 + 0.65 * amp)}
            strokeLinecap="round"
            className="nk-lfo-dot"
            style={{ opacity: glow }}
          />
        ) : chase ? (
          <path
            d={path}
            fill="none"
            stroke="var(--nk-cyan)"
            strokeWidth={LFO_DOT}
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeDasharray={lfo.dash}
            className="nk-lfo-dot"
            style={{
              animationDuration: `${lfoMs}ms`,
              ["--nk-lfo-cycle" as string]: String(-lfo.cycle),
              opacity: glow,
            }}
          />
        ) : null}
      </g>
    );
  }

  return (
    <g className="nk-dof-edge" data-focus={focus}>
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
        stroke={kind === "param" ? "#2a2a10" : "#220505"}
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
        stroke={kind === "audio" ? "var(--nk-black)" : accent}
        strokeWidth={bore}
        strokeLinecap="butt"
        strokeLinejoin="miter"
        data-src={srcKey}
        className={`nk-tube-bore ${tubeClass}`}
      />
      {showPlasma ? (
        <>
          <path
            d={path}
            fill="none"
            stroke="var(--nk-accent)"
            strokeWidth="2.4"
            strokeLinecap="butt"
            strokeLinejoin="miter"
            data-src={srcKey}
            className="nk-plasma-glow"
          />
          <path
            d={path}
            fill="none"
            stroke="var(--nk-white)"
            strokeWidth="1.2"
            strokeLinecap="butt"
            strokeLinejoin="miter"
            data-src={srcKey}
            className="nk-plasma"
          />
        </>
      ) : null}
      {showDots ? (
        <path
          d={path}
          fill="none"
          stroke="var(--nk-white)"
          strokeWidth="2.2"
          strokeLinecap="round"
          strokeLinejoin="miter"
          data-src={srcKey}
          className="nk-dots"
        />
      ) : null}
    </g>
  );
}
