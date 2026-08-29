import { advancePlasmaDash } from "../assemble/cableMotion";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { subscribeVizClock } from "./vizClock";

/** Smooth expo-out. Chrome motion is never `steps()`. */
export const MOTION_EASE = "cubic-bezier(0.22, 1, 0.36, 1)";

export const POST_STACK = ["bloom", "chroma", "sweep", "scan", "vignette"] as const;

export function timingIsStepped(css: string): boolean {
  return /\bsteps\s*\(/.test(css);
}

/** Plasma rides CSS dash offset. Do not rebuild SVG from scope samples. */
export const PLASMA_DRIVER = "css" as const;

export function peakCssVars(
  inPeak: number,
  outPeak: number,
  flow: { dashIn: number; dashOut: number; dt: number } = { dashIn: 0, dashOut: 0, dt: 0 },
): Record<string, string> {
  return {
    "--nk-in": inPeak.toFixed(3),
    "--nk-out": outPeak.toFixed(3),
    "--nk-dash-in": advancePlasmaDash(flow.dashIn, inPeak, flow.dt).toFixed(2),
    "--nk-dash-out": advancePlasmaDash(flow.dashOut, outPeak, flow.dt).toFixed(2),
  };
}

export function bindPeakCss(el: HTMLElement): () => void {
  let dashIn = 0;
  let dashOut = 0;
  let last = 0;
  const tick = (now: number) => {
    const dt = last > 0 ? Math.min(0.05, (now - last) / 1000) : 0;
    last = now;
    const motion = useHostStore.getState().motion;
    const osReduce = typeof window !== "undefined"
      && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
    const still = motion === "off" || (osReduce && motion !== "full");
    const s = useTelemetryStore.getState();
    const vars = peakCssVars(s.inPeak, s.outPeak, {
      dashIn,
      dashOut,
      dt: still ? 0 : dt,
    });
    dashIn = Number(vars["--nk-dash-in"]);
    dashOut = Number(vars["--nk-dash-out"]);
    for (const [k, v] of Object.entries(vars)) {
      el.style.setProperty(k, v);
    }
  };
  return subscribeVizClock(tick);
}
