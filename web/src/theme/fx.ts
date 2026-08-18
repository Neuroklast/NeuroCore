import { useTelemetryStore } from "../store/telemetryStore";

/** Smooth expo-out. Chrome motion is never `steps()`. */
export const MOTION_EASE = "cubic-bezier(0.22, 1, 0.36, 1)";

export const POST_STACK = ["bloom", "chroma", "sweep", "scan", "vignette"] as const;

export function timingIsStepped(css: string): boolean {
  return /\bsteps\s*\(/.test(css);
}

/** Plasma rides CSS dash offset. Do not rebuild SVG from scope samples. */
export const PLASMA_DRIVER = "css" as const;

export function bindPeakCss(el: HTMLElement): () => void {
  const write = (s: { inPeak: number; outPeak: number }) => {
    el.style.setProperty("--nk-in", s.inPeak.toFixed(3));
    el.style.setProperty("--nk-out", s.outPeak.toFixed(3));
  };
  write(useTelemetryStore.getState());
  return useTelemetryStore.subscribe(write);
}
