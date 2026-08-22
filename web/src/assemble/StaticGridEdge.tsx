import { BaseEdge, getStraightPath, type EdgeProps } from "@xyflow/react";
import { useHostStore } from "../store/hostStore";
import { motionAllows } from "../theme/motionPolicy";
import { TUBE } from "./tubeModel";
import { CableTraffic } from "./CableTraffic";
import { lfoChaseMs, lfoDash, lfoDotGlow, resolveAmp, svgPathLength, LFO_DOT, LFO_WIRE } from "./cableMotion";
import { resolveLfoHz } from "./lfoLamp";
import { isEnvNode, isLfoNode } from "./flowFromAst";
import { cableAccent, normalizeCableKind } from "./validateLink";

export function staticRoute(data: unknown): string {
  const r = (data as { route?: string } | undefined)?.route;
  return typeof r === "string" ? r : "";
}

export function StaticGridEdge({
  id,
  source,
  sourceX,
  sourceY,
  targetX,
  targetY,
  markerEnd,
  data,
}: EdgeProps) {
  const stored = staticRoute(data);
  const temp = Boolean((data as { temp?: boolean } | undefined)?.temp);
  const path = stored || (temp
    ? getStraightPath({ sourceX, sourceY, targetX, targetY })[0]
    : "");
  const cables = useHostStore((s) => s.cables);
  const motion = useHostStore((s) => s.motion);
  const knobs = useHostStore((s) => s.knobs);
  const bpm = useHostStore((s) => s.bpm);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
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
  const len = svgPathLength(path);
  const lfo = lfoDash(Math.max(len, 1), LFO_DOT);
  const showPlasma = ! temp && kind === "audio" && cables === "wave";
  const showDots = ! temp && kind === "audio" && cables === "dots";
  const animateCables = motionAllows("pipeWave", motion, reduced);
  const chase = ! temp && kind === "mod" && lfoSrc && motionAllows("lfoChase", motion, reduced);
  const accent = cableAccent(kind);
  const tubeClass = kind === "mod" ? "nk-tube-mod" : kind === "param" ? "nk-tube-param" : "";
  const focus = String((data as { focus?: string } | undefined)?.focus ?? "off");
  if (! path) {
    return null;
  }
  if (kind === "mod") {
    return (
      <g className="nk-dof-edge" data-focus={focus}>
        <BaseEdge id={id} path={path} style={{ stroke: "transparent", strokeWidth: 10, fill: "none" }} markerEnd={markerEnd} />
        <path
          d={path}
          fill="none"
          stroke="var(--nk-cyan)"
          strokeWidth={LFO_WIRE}
          strokeLinecap="butt"
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
            style={{ opacity: lfoDotGlow(amp) }}
          />
        ) : chase ? (
          <path
            d={path}
            fill="none"
            stroke="var(--nk-cyan)"
            strokeWidth={LFO_DOT}
            strokeLinecap="round"
            strokeDasharray={lfo.dash}
            className="nk-lfo-dot"
            style={{ animationDuration: `${lfoMs}ms`, ["--nk-lfo-cycle" as string]: String(-lfo.trip), opacity: lfoDotGlow(amp) }}
          />
        ) : null}
      </g>
    );
  }
  return (
    <g className="nk-dof-edge" data-focus={focus}>
      <BaseEdge id={id} path={path} style={{ stroke: "transparent", strokeWidth: outer + 4, fill: "none" }} markerEnd={markerEnd} />
      <path d={path} fill="none" stroke={accent} strokeWidth={outer + 2} strokeLinecap="butt" className={`nk-tube-bloom ${tubeClass}`} />
      <path d={path} fill="none" stroke={kind === "param" ? "#2a2a10" : "#220505"} strokeWidth={outer} opacity={0.96} />
      <path d={path} fill="none" stroke="#050505" strokeWidth={glass} opacity={0.94} />
      <path d={path} fill="none" stroke={kind === "audio" ? "var(--nk-black)" : accent} strokeWidth={bore} data-src={srcKey} className={`nk-tube-bore ${tubeClass}`} />
      {showPlasma || showDots ? (
        <CableTraffic sourceId={sourceId || source} sourceType={sourceType}>
          {showPlasma ? (
            <>
              <path d={path} fill="none" stroke="var(--nk-accent)" strokeWidth="2.4" className={`nk-plasma-glow${animateCables ? "" : " nk-cable-still"}`} />
              <path d={path} fill="none" stroke="var(--nk-white)" strokeWidth="1.2" className={`nk-plasma${animateCables ? "" : " nk-cable-still"}`} />
            </>
          ) : null}
          {showDots ? <path d={path} fill="none" stroke="var(--nk-white)" strokeWidth="2.2" strokeLinecap="round" className={`nk-dots${animateCables ? "" : " nk-cable-still"}`} /> : null}
        </CableTraffic>
      ) : null}
    </g>
  );
}
