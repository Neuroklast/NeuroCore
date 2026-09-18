import { useEffect } from "react";
import { useHostStore } from "../store/hostStore";
import { bindPeakCss } from "./fx";
import { motionAllows } from "./motionPolicy";

export type CrtLayer = "scan" | "sweep" | "chroma" | "vignette" | "bloom";

/** Scan/chroma/bloom sit on the OS chrome. Vignette stays on the workspace pane. */
export function crtHost(layer: CrtLayer): "os" | "pane" {
  return layer === "vignette" ? "pane" : "os";
}

function prefersReducedMotion(): boolean {
  return typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
}

export function PaneVignette() {
  const motion = useHostStore((s) => s.motion);
  if (motion === "off" || crtHost("vignette") !== "pane") {
    return null;
  }
  return <div className="nk-crt-vignette" aria-hidden />;
}

export function CrtFx() {
  const motion = useHostStore((s) => s.motion);
  const reduced = prefersReducedMotion();
  const scan = motionAllows("crtScan", motion, reduced);
  const bloom = motionAllows("bloom", motion, reduced);
  const jit = motionAllows("jitter", motion, reduced);

  useEffect(() => bindPeakCss(document.documentElement), []);

  if (motion === "off") {
    return null;
  }
  return (
    <div className={`nk-crt pointer-events-none ${jit ? "nk-crt-jit" : ""}`} aria-hidden>
      {bloom ? <div className="nk-crt-bloom" /> : null}
      {scan ? <div className="nk-crt-scan" /> : null}
      {scan ? <div className="nk-crt-sweep" /> : null}
      <div className="nk-crt-chroma" />
    </div>
  );
}
