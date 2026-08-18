import { useEffect, useState } from "react";
import { Knob } from "../chrome/Knob";
import { bloomFilter } from "../overlays/overlayMotion";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { logAmp } from "../assemble/tubeModel";
import { motionAllows } from "../theme/motionPolicy";
import type { Workspace } from "../app/workspace";
import {
  faceGlitchStyle,
  logoOverlayMask,
  pickFaceGlitch,
  scheduleFaceGlitch,
  type FaceGlitchKind,
} from "./faceGlitch";

const MARK = "./img/neurokore.png";

export function FaceView({ open }: { open: (w: Workspace) => void }) {
  const knobs = useHostStore((s) => s.knobs);
  const motion = useHostStore((s) => s.motion);
  const peak = useTelemetryStore((s) => s.outPeak);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const glow = motionAllows("faceGlow", motion, reduced) ? logAmp(peak) : 0;
  const bloom = motionAllows("bloom", motion, reduced);
  const scan = motionAllows("crtScan", motion, reduced);
  const jit = motionAllows("jitter", motion, reduced);
  const live = knobs.filter((k) => k.active);
  const [glitch, setGlitch] = useState<{ kind: FaceGlitchKind; seed: number }>({
    kind: "idle",
    seed: 0,
  });

  useEffect(() => {
    if (! jit) {
      setGlitch({ kind: "idle", seed: 0 });
      return;
    }
    let kind: FaceGlitchKind = "idle";
    let handle = 0;
    const tick = () => {
      kind = kind === "idle" ? pickFaceGlitch(Math.random()) : "idle";
      setGlitch({ kind, seed: Math.floor(Math.random() * 1000) });
      handle = window.setTimeout(tick, scheduleFaceGlitch(kind, Math.random()));
    };
    handle = window.setTimeout(tick, scheduleFaceGlitch("idle", Math.random()));
    return () => window.clearTimeout(handle);
  }, [jit]);

  return (
    <section className="nk-face relative flex h-full min-h-0 flex-1 flex-col bg-[#0a0a0c] px-8">
      <div className="relative min-h-0 flex-1">
        <button
          type="button"
          className="nk-face-logo"
          onClick={() => open("assemble")}
          title="Open circuit"
        >
          <span
            className={`nk-face-fx ${scan ? "nk-face-scan" : ""} nk-face-g-${glitch.kind}`}
            style={faceGlitchStyle(glitch.kind, glitch.seed)}
            data-glitch={glitch.kind}
          >
            <img
              src={MARK}
              alt="NEUROKORE"
              className="nk-face-mark"
              style={{ filter: bloomFilter(glow, bloom) }}
            />
            {jit ? (
              <>
                <img src={MARK} alt="" className="nk-face-ghost nk-face-ghost-r" />
                <img src={MARK} alt="" className="nk-face-ghost nk-face-ghost-c" />
                <span className="nk-face-bars" style={logoOverlayMask(MARK)} aria-hidden />
                <span className="nk-face-static" style={logoOverlayMask(MARK)} aria-hidden />
              </>
            ) : null}
          </span>
        </button>
      </div>
      <div className="flex max-w-[920px] flex-wrap items-stretch justify-center gap-3 self-center pb-2">
        {live.map((k) => (
          <div key={k.id} className="h-[168px] w-[128px]">
            <Knob knob={k} bind={false} />
          </div>
        ))}
      </div>
      <div className="mb-6 flex justify-center gap-2">
        <button type="button" className="nk-clip" onClick={() => open("assemble")}>Open Circuit</button>
        <button type="button" className="nk-clip" onClick={() => open("hack")}>Open Terminal</button>
      </div>
    </section>
  );
}
