import { useEffect, useRef, type CSSProperties } from "react";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { subscribeVizClock } from "../theme/vizClock";
import { unitMarkCssVars } from "./faceGlitch";
import { LOGO_SLICE_COUNT, logoSliceFrame } from "./logoSlices";

export function LogoMark({ src, theme, filter }: { src: string; theme: string; filter: string }) {
  const motion = useHostStore((s) => s.motion);
  const layers = useRef<Array<HTMLImageElement | null>>([]);
  useEffect(() => {
    if (motion !== "full") return;
    let started: number | null = null;
    let previous = 0;
    let envelope = 0;
    let hit = -Infinity;
    return subscribeVizClock((now) => {
      started ??= now;
      const telemetry = useTelemetryStore.getState();
      const energy = Math.min(1, Math.log10(1 + Math.max(0, telemetry.outRms, telemetry.outPeak * 0.6) * 9));
      const dt = previous ? Math.min(100, now - previous) : 16;
      previous = now;
      if (energy - envelope > 0.16 && now - hit > 360) hit = now;
      envelope += (energy - envelope) * (1 - Math.exp(-dt / 180));
      const impulse = Math.max(0, 1 - (now - hit) / 170) ** 2;
      const frame = logoSliceFrame(now - started, energy, motion, impulse);
      frame.forEach((slice, i) => {
        const image = layers.current[i];
        if (!image) return;
        image.style.transform = `translate3d(${slice.x.toFixed(2)}px, 0, 0)`;
        image.style.opacity = String(slice.opacity);
      });
    });
  }, [motion, src]);

  return <span className="nk-face-fx nk-logo-slices" data-glitch={motion} role="img" aria-label={theme === "digicide" ? "DIGICIDE" : "NEUROKORE"}
    style={{ ...unitMarkCssVars(src, theme), filter } as CSSProperties}>
    <img src={src} alt={theme === "digicide" ? "DIGICIDE" : "NEUROKORE"} className="nk-face-mark" />
    {motion === "full" ? Array.from({ length: LOGO_SLICE_COUNT }, (_, i) => (
      <img key={i} ref={(el) => { layers.current[i] = el; }} src={src} alt="" aria-hidden="true"
        className="nk-logo-slice" draggable={false}
        style={{ clipPath: `inset(${i * 100 / LOGO_SLICE_COUNT}% 0 ${100 - (i + 1) * 100 / LOGO_SLICE_COUNT}% 0)` }} />
    )) : null}
  </span>;
}
