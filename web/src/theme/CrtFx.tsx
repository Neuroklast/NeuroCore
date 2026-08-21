import { useEffect, useRef } from "react";
import { useHostStore } from "../store/hostStore";
import { fitCanvas } from "../viz/canvasFit";
import { paintTechNoise } from "../viz/scopeModel";
import { liveTheme } from "./theme";
import { bindPeakCss } from "./fx";
import { motionAllows } from "./motionPolicy";

export type CrtLayer = "scan" | "sweep" | "chroma" | "vignette" | "bloom" | "techNoise";

/** Scan/chroma/bloom sit on the OS chrome. Vignette + speckle stay on Unit/Circuit/Terminal. */
export function crtHost(layer: CrtLayer): "os" | "pane" {
  return layer === "vignette" || layer === "techNoise" ? "pane" : "os";
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
  return (
    <>
      <div className="nk-frame-bloom" aria-hidden />
      <div className="nk-crt-vignette" aria-hidden />
    </>
  );
}

/** Spectrograph speckle on the workspace glass. Full motion only. */
export function PaneTechNoise() {
  const motion = useHostStore((s) => s.motion);
  const on = motionAllows("techNoise", motion, prefersReducedMotion());
  const ref = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (! on) {
      return;
    }
    const canvas = ref.current;
    if (! canvas) {
      return;
    }
    const ctx = canvas.getContext("2d");
    if (! ctx) {
      return;
    }
    let raf = 0;
    let frame = 0;
    const draw = () => {
      const { w, h, scale } = fitCanvas(canvas);
      ctx.setTransform(scale, 0, 0, scale, 0, 0);
      ctx.clearRect(0, 0, w, h);
      paintTechNoise(ctx, w, h, frame, liveTheme().cyan);
      frame += 1;
      raf = requestAnimationFrame(draw);
    };
    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, [on]);

  if (! on || crtHost("techNoise") !== "pane") {
    return null;
  }
  return <canvas ref={ref} className="nk-tech-noise" aria-hidden />;
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
