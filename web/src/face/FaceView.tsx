import { hasJuceBridge } from "../bridge/juce";
import { bloomFilter } from "../overlays/overlayMotion";
import { UnitAnalyzer } from "../viz/ScopeDeck";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { motionAllows } from "../theme/motionPolicy";
import { unitMarkCssVars, unitMarkSrc } from "./faceGlitch";
import {
  bandRms,
  formatDbfs,
  formatHudFixed,
  logoReactiveStyle,
  logoRgbSplit,
  osModeLabel,
  rmsReadout,
  stereoMetrics,
} from "./faceModel";

export function FaceView() {
  const theme = useHostStore((s) => s.theme);
  const motion = useHostStore((s) => s.motion);
  const cpu = useHostStore((s) => s.cpu);
  const buf = useHostStore((s) => s.buf);
  const sr = useHostStore((s) => s.sr);
  const latency = useHostStore((s) => s.lat);
  const osFactor = useHostStore((s) => s.osFactor);
  const telemetry = useTelemetryStore();
  const preview = !hasJuceBridge();
  const live = preview || telemetry.available;
  const mark = unitMarkSrc(theme);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;

  const bloom = motionAllows("bloom", motion, reduced);
  const scan = motionAllows("crtScan", motion, reduced);
  const splitBands = bandRms(telemetry.scopeOut, sr > 0 ? sr : 48000);
  const rgb = logoRgbSplit(splitBands, { motion, prefersReduced: reduced });
  const chroma = rgb.redX > 0.05 || rgb.cyanY > 0.05;
  const amp = Math.max(telemetry.outRms, telemetry.outPeak * 0.6);
  const glow = amp > 0 ? Math.min(1, Math.log10(1 + amp * 9)) : 0;

  const stereo = stereoMetrics(telemetry.gonioL, telemetry.gonioR);
  const rms = rmsReadout(telemetry.outRms);
  const level = (value: number) => live ? formatHudFixed(Number(formatDbfs(value)), 1, 3) : "—";

  const fxStyle = {
    ...logoReactiveStyle(rgb),
    ...unitMarkCssVars(mark, theme),
  };

  return (
    <section className="nk-face relative flex h-full min-h-0 flex-1 flex-col overflow-hidden bg-[var(--nk-bg)]">
      <aside className="nk-face-tele nk-face-tele-l" aria-label="engine status">
        <FaceRow k="SAMPLE RATE" v={sr > 0 ? `${(sr / 1000).toFixed(1)} kHz` : "—"} />
        <FaceRow k="BUFFER" v={buf > 0 ? `${buf} samples` : "—"} />
        <FaceRow k="LATENCY" v={sr > 0 ? `${(latency / sr * 1000).toFixed(2)} ms` : "—"} />
        <FaceRow k="OVERSAMPLING" v={osModeLabel(osFactor)} />
        <FaceRow k="CPU" v={`${Math.round(cpu)}%`} />
        {preview ? <FaceRow k="PREVIEW" v="Synthetic audio" /> : !live ? <FaceRow k="METERS" v="Waiting for audio data" /> : null}
      </aside>
      <aside className="nk-face-tele nk-face-tele-r" aria-label="signal metrics">
        <FaceRow k="INPUT PEAK" num={level(telemetry.inPeak)} unit="dBFS" />
        <FaceRow k="OUTPUT PEAK" num={level(telemetry.outPeak)} unit="dBFS" />
        <FaceRow k="OUTPUT RMS" num={live ? formatHudFixed(Number(rms.value), 1, 3) : "—"} unit={rms.unit} />
        <FaceRow k="STEREO CORRELATION" num={live ? formatHudFixed(stereo.corr, 2, 2) : "—"} />
      </aside>
      <div className="nk-face-logo">
        <span
          className={[
            "nk-face-fx",
            scan ? "nk-face-scan" : "",
            chroma ? "nk-face-chroma" : "",
          ].filter(Boolean).join(" ")}
          style={fxStyle}
        >
          <img
            src={mark}
            alt={theme === "digicide" ? "DIGICIDE" : "NEUROKORE"}
            className="nk-face-mark"
            style={{ filter: bloomFilter(glow, bloom) }}
          />
          {chroma ? (
            <>
              <img src={mark} alt="" className="nk-face-ghost nk-face-ghost-r" />
              <img src={mark} alt="" className="nk-face-ghost nk-face-ghost-c" />
            </>
          ) : null}
        </span>
      </div>
      <div className="flex min-h-0 flex-1"><UnitAnalyzer /></div>
    </section>
  );
}

function FaceRow({ k, v, num, unit }: { k: string; v?: string; num?: string; unit?: string }) {
  return (
    <div className="nk-face-row">
      <span>{k}</span>
      {num != null ? (
        <span className="nk-face-val"><span className="nk-face-num">{num}</span>
          {unit ? <span className="nk-face-unit">{unit}</span> : null}</span>
      ) : <span>{v}</span>}
    </div>
  );
}
