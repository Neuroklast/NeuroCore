import { useEffect, useRef, useState } from "react";
import { bloomFilter } from "../overlays/overlayMotion";
import { UnitAnalyzer } from "../viz/ScopeDeck";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { motionAllows } from "../theme/motionPolicy";

import {
  estimateBands,
  faceGlitchStyle,
  logoMotion,
  logoMotionStyle,
  logoOverlayMask,
  pickFaceGlitch,
  scheduleFaceGlitch,
  unitMarkCssVars,
  unitMarkSrc,
  type FaceGlitchKind,
} from "./faceGlitch";
import {
  astChecksum,
  bandRms,
  coreTempC,
  cursorReadout,
  dataRainLines,
  driveAmount,
  dspEvalMs,
  formatDbfs,
  formatHudFixed,
  formatLufs,
  logoPulsePeriodMs,
  logoReactiveStyle,
  logoRgbSplit,
  osModeLabel,
  rainHot,
  rainSpeed,
  stereoMetrics,
  transientHit,
} from "./faceModel";

export function FaceView() {
  const knobs = useHostStore((s) => s.knobs);
  const theme = useHostStore((s) => s.theme);
  const mark = unitMarkSrc(theme);
  const motion = useHostStore((s) => s.motion);
  const cpu = useHostStore((s) => s.cpu);
  const buf = useHostStore((s) => s.buf);
  const sr = useHostStore((s) => s.sr);
  const bpm = useHostStore((s) => s.bpm);
  const osFactor = useHostStore((s) => s.osFactor);
  const peak = useTelemetryStore((s) => s.outPeak);
  const outRms = useTelemetryStore((s) => s.outRms);
  const scopeOut = useTelemetryStore((s) => s.scopeOut);
  const gonioL = useTelemetryStore((s) => s.gonioL);
  const gonioR = useTelemetryStore((s) => s.gonioR);
  const tick = useTelemetryStore((s) => s.tick);
  const ast = useAstStore((s) => s.ast ?? s.lastValidAst);
  const script = useAstStore((s) => s.lastValidScript);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;

  const bands = estimateBands(scopeOut, peak);
  const splitBands = bandRms(scopeOut, sr > 0 ? sr : 48000);
  const drive = logoMotion(bands.loudness, bands.bass, bands.treble, {
    motion,
    prefersReduced: reduced,
  });
  const rgb = logoRgbSplit(splitBands, { motion, prefersReduced: reduced });
  const bloom = motionAllows("bloom", motion, reduced);
  const scan = motionAllows("crtScan", motion, reduced);
  const jit = motionAllows("jitter", motion, reduced);
  const cpu01 = Math.max(0, Math.min(1, cpu / 100));
  const drive01 = driveAmount(knobs);
  const temp = coreTempC(drive01, cpu01);
  const stereo = stereoMetrics(gonioL, gonioR);
  const checksum = astChecksum(script || JSON.stringify(ast ?? ""));
  const evalMs = dspEvalMs(cpu01, buf, sr);
  const pulseMs = logoPulsePeriodMs(bpm);
  const rain = rainSpeed(cpu01, drive01);
  const hot = rainHot(cpu01, drive01);
  const rainLines = dataRainLines(checksum, tick, 22);

  const [glitch, setGlitch] = useState<{ kind: FaceGlitchKind; seed: number }>({
    kind: "idle",
    seed: 0,
  });
  const [cursor, setCursor] = useState<string | null>(null);
  const prevPeak = useRef(0);
  const sliceHold = useRef(0);

  useEffect(() => {
    if (! jit) {
      setGlitch({ kind: "idle", seed: 0 });
      return;
    }
    if (transientHit(peak, prevPeak.current)) {
      window.clearTimeout(sliceHold.current);
      setGlitch({ kind: "slice", seed: Math.floor(Math.random() * 1000) });
      sliceHold.current = window.setTimeout(() => {
        setGlitch({ kind: "idle", seed: 0 });
      }, scheduleFaceGlitch("slice", 0.35));
    }
    prevPeak.current = peak;
  }, [jit, peak, tick]);

  useEffect(() => {
    if (! jit) {
      return;
    }
    let kind: FaceGlitchKind = "idle";
    let handle = 0;
    const tickGlitch = () => {
      kind = kind === "idle" ? pickFaceGlitch(Math.random()) : "idle";
      if (kind !== "slice") {
        setGlitch({ kind, seed: Math.floor(Math.random() * 1000) });
      }
      handle = window.setTimeout(tickGlitch, scheduleFaceGlitch(kind, Math.random()));
    };
    handle = window.setTimeout(tickGlitch, scheduleFaceGlitch("idle", Math.random()));
    return () => window.clearTimeout(handle);
  }, [jit]);

  const fxStyle = {
    ...faceGlitchStyle(glitch.kind, glitch.seed),
    ...logoMotionStyle(drive),
    ...logoReactiveStyle(rgb, pulseMs, rain),
    ...unitMarkCssVars(mark, theme),
  };

  return (
    <section
      className="nk-face relative flex h-full min-h-0 flex-1 flex-col overflow-hidden bg-[var(--nk-bg)]"
      onMouseMove={(e) => {
        const r = e.currentTarget.getBoundingClientRect();
        setCursor(cursorReadout(e.clientX - r.left, e.clientY - r.top));
      }}
      onMouseLeave={() => setCursor(null)}
    >
      <aside className="nk-face-tele nk-face-tele-l" aria-label="engine status">
        <FaceRow k="AST_CHECKSUM" v={checksum} />
        <FaceRow k="DSP_EVAL_TIME" num={formatHudFixed(evalMs, 2, 2)} unit="ms" />
        <FaceRow k="BUFFER_SIZE" num={buf > 0 ? String(buf).padStart(4, "\u2007") : "—"} unit="SMP" />
        <FaceRow k="CORE_TEMP" v={`${temp.temp}°C${temp.warn ? " [WARN]" : ""}`} warn={temp.warn} />
        <FaceRow k="OVERSAMPLING_MODE" v={osModeLabel(osFactor)} />
        <FaceRow k="CPU" v={`${Math.round(cpu)}%`} />
      </aside>
      <aside className="nk-face-tele nk-face-tele-r" aria-label="signal metrics">
        <FaceRow k="PEAK_L" num={formatHudFixed(Number(formatDbfs(stereo.peakL)), 2, 3)} unit="dBFS" />
        <FaceRow k="PEAK_R" num={formatHudFixed(Number(formatDbfs(stereo.peakR)), 2, 3)} unit="dBFS" />
        <FaceRow k="RMS" num={formatHudFixed(Number(formatLufs(outRms)), 1, 3)} unit="LUFS" />
        <FaceRow k="PHASE_CORRELATION" num={formatHudFixed(stereo.corr, 2, 2)} />
      </aside>

      <div className="nk-face-logo">
          <span
            className={[
              "nk-face-fx",
              scan ? "nk-face-scan" : "",
              rgb.redX > 0 || rgb.cyanY > 0 || drive.chromaPx > 0 ? "nk-face-chroma" : "",
              drive.jitterHz > 0 ? "nk-face-jitter" : "",
              jit ? "nk-face-pulse-on" : "",
              hot ? "nk-face-rain-hot" : "",
              `nk-face-g-${glitch.kind}`,
            ].filter(Boolean).join(" ")}
            style={fxStyle}
            data-glitch={glitch.kind}
          >
            <span className="nk-face-rain" aria-hidden>
              {[0, 1, 2].map((col) => (
                <span key={col} className="nk-face-rain-col">
                  {rainLines.map((line, i) => (
                    <span key={`${col}-${i}`}>{col === 1 ? line.slice(4) + line.slice(0, 4) : line}</span>
                  ))}
                </span>
              ))}
            </span>
            <img
              src={mark}
              alt={theme === "digicide" ? "DIGICIDE" : "NEUROKORE"}
              className="nk-face-mark"
              style={{ filter: bloomFilter(drive.glow, bloom) }}
            />
            {jit || rgb.redX > 0 || rgb.cyanY > 0 || drive.chromaPx > 0 ? (
              <>
                <img src={mark} alt="" className="nk-face-ghost nk-face-ghost-r" />
                <img src={mark} alt="" className="nk-face-ghost nk-face-ghost-c" />
              </>
            ) : null}
            <span className="nk-face-pulse" style={logoOverlayMask(mark)} aria-hidden />
            {jit ? (
              <>
                <span className="nk-face-bars" style={logoOverlayMask(mark)} aria-hidden />
                <span className="nk-face-static" style={logoOverlayMask(mark)} aria-hidden />
              </>
            ) : null}
          </span>
      </div>

      <div className="flex min-h-0 flex-1">
        <UnitAnalyzer />
      </div>

      {cursor ? <div className="nk-face-cursor">{cursor}</div> : null}
    </section>
  );
}

function FaceRow({ k, v, num, unit, warn = false }: { k: string; v?: string; num?: string; unit?: string; warn?: boolean }) {
  return (
    <div className={`nk-face-row${warn ? " is-warn" : ""}`}>
      <span>{k}</span>
      {num != null ? (
        <span className="nk-face-val">
          <span className="nk-face-num">{num}</span>
          {unit ? <span className="nk-face-unit">{unit}</span> : null}
        </span>
      ) : (
        <span>{v}</span>
      )}
    </div>
  );
}
