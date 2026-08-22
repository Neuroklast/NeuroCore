import { useEffect, useRef, type CSSProperties, type ReactNode } from "react";
import { useHostStore } from "../store/hostStore";
import { motionAllows } from "../theme/motionPolicy";
import { subscribeVizClock } from "../theme/vizClock";
import { advancePlasmaDash, cableSourcePeak } from "./cableMotion";

/** Beads on this cable follow the source chip’s peak. Still at silence / −60 dBFS. */
export function CableTraffic({
  sourceId,
  sourceType = "",
  children,
}: {
  sourceId: string;
  sourceType?: string;
  children: ReactNode;
}) {
  const gRef = useRef<SVGGElement>(null);
  const motion = useHostStore((s) => s.motion);
  useEffect(() => {
    let dash = 0;
    let last = 0;
    const reduced = typeof window !== "undefined"
      && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
    const freeze = reduced || ! motionAllows("pipeWave", motion, reduced);
    return subscribeVizClock((now) => {
      const dt = last > 0 ? Math.min(0.05, (now - last) / 1000) : 0;
      last = now;
      const peak = cableSourcePeak(sourceId, sourceType, useHostStore.getState().clips);
      dash = advancePlasmaDash(dash, peak, freeze ? 0 : dt);
      gRef.current?.style.setProperty("--nk-dash", dash.toFixed(2));
    });
  }, [motion, sourceId, sourceType]);
  return (
    <g ref={gRef} className="nk-cable-traffic" style={{ ["--nk-dash" as string]: "0" } as CSSProperties}>
      {children}
    </g>
  );
}
