import { useEffect, useMemo, useRef, useState } from "react";
import { Knob } from "../chrome/Knob";
import { bloomFilter } from "../overlays/overlayMotion";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { motionAllows } from "../theme/motionPolicy";
import type { Workspace } from "../app/workspace";
import {
  estimateBands,
  faceGlitchStyle,
  logoMotion,
  logoMotionStyle,
  logoOverlayMask,
  pickFaceGlitch,
  scheduleFaceGlitch,
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
  formatLufs,
  logoPulsePeriodMs,
  logoReactiveStyle,
  logoRgbSplit,
  minimapGraph,
  osModeLabel,
  rainHot,
  rainSpeed,
  spectrumBins,
  stereoMetrics,
  transientHit,
} from "./faceModel";

const MARK = "./img/neurokore.png";

export function FaceView({ open }: { open: (w: Workspace) => void }) {
  const knobs = useHostStore((s) => s.knobs);
  const motion = useHostStore((s) => s.motion);
  const cpu = useHostStore((s) => s.cpu);
  const buf = useHostStore((s) => s.buf);
  const sr = useHostStore((s) => s.sr);
  const bpm = useHostStore((s) => s.bpm);
  const osFactor = useHostStore((s) => s.osFactor);
  const mix = useHostStore((s) => s.mix);
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
  const live = knobs.filter((k) => k.active);
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
  const bins = spectrumBins(scopeOut, 48);
  const mini = useMemo(() => minimapGraph(ast, mix <= 1e-5), [ast, mix]);

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
  };

  return (
    <section
      className="nk-face relative flex h-full min-h-0 flex-1 flex-col overflow-hidden bg-[#0a0a0c] px-8"
      onMouseMove={(e) => {
        const r = e.currentTarget.getBoundingClientRect();
        setCursor(cursorReadout(e.clientX - r.left, e.clientY - r.top));
      }}
      onMouseLeave={() => setCursor(null)}
    >
      <FaceFft bins={bins} />

      <aside className="nk-face-tele nk-face-tele-l" aria-label="signal metrics">
        <FaceRow k="PEAK_L" v={`${formatDbfs(stereo.peakL)} dBFS`} />
        <FaceRow k="PEAK_R" v={`${formatDbfs(stereo.peakR)} dBFS`} />
        <FaceRow k="RMS" v={`${formatLufs(outRms)} LUFS`} />
        <FaceRow k="PHASE_CORRELATION" v={stereo.corr.toFixed(2)} />
      </aside>

      <aside className="nk-face-tele nk-face-tele-r" aria-label="engine status">
        <FaceRow k="AST_CHECKSUM" v={checksum} />
        <FaceRow k="DSP_EVAL_TIME" v={`${evalMs.toFixed(2)} ms`} />
        <FaceRow k="BUFFER_SIZE" v={`${buf > 0 ? buf : "—"} SMP`} />
        <FaceRow
          k="CORE_TEMP"
          v={`${temp.temp}°C${temp.warn ? " [WARN]" : ""}`}
          warn={temp.warn}
        />
        <FaceRow k="OVERSAMPLING_MODE" v={osModeLabel(osFactor)} />
      </aside>

      <FaceMinimap nodes={mini.nodes} edges={mini.edges} />

      {cursor ? <div className="nk-face-cursor">{cursor}</div> : null}

      <div className="relative z-[1] min-h-0 flex-1">
        <button
          type="button"
          className="nk-face-logo"
          onClick={() => open("assemble")}
          title="Open circuit"
        >
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
              src={MARK}
              alt="NEUROKORE"
              className="nk-face-mark"
              style={{ filter: bloomFilter(drive.glow, bloom) }}
            />
            {jit || rgb.redX > 0 || rgb.cyanY > 0 || drive.chromaPx > 0 ? (
              <>
                <img src={MARK} alt="" className="nk-face-ghost nk-face-ghost-r" />
                <img src={MARK} alt="" className="nk-face-ghost nk-face-ghost-c" />
              </>
            ) : null}
            <span className="nk-face-pulse" style={logoOverlayMask(MARK)} aria-hidden />
            {jit ? (
              <>
                <span className="nk-face-bars" style={logoOverlayMask(MARK)} aria-hidden />
                <span className="nk-face-static" style={logoOverlayMask(MARK)} aria-hidden />
              </>
            ) : null}
          </span>
        </button>
      </div>

      <div className="relative z-[1] flex max-w-[960px] flex-wrap items-stretch justify-center gap-3 self-center pb-2">
        {live.map((k) => (
          <FaceKnob key={k.id} knob={k} />
        ))}
      </div>
      <div className="relative z-[1] mb-6 flex justify-center gap-2">
        <button type="button" className="nk-clip" onClick={() => open("assemble")}>Open Circuit</button>
        <button type="button" className="nk-clip" onClick={() => open("hack")}>Open Terminal</button>
      </div>
    </section>
  );
}

function FaceRow({ k, v, warn = false }: { k: string; v: string; warn?: boolean }) {
  return (
    <div className={`nk-face-row${warn ? " is-warn" : ""}`}>
      <span>{k}</span>
      <span>{v}</span>
    </div>
  );
}

function FaceFft({ bins }: { bins: number[] }) {
  const hold = useRef<number[]>([]);
  if (hold.current.length !== bins.length) {
    hold.current = bins.slice();
  } else {
    for (let i = 0; i < bins.length; i += 1) {
      hold.current[i] = Math.max(bins[i] ?? 0, (hold.current[i] ?? 0) * 0.82);
    }
  }
  const raw = hold.current;
  const shown = raw.map((v, i) => (
    (raw[i - 1] ?? 0) * 0.22 + v * 0.56 + (raw[i + 1] ?? 0) * 0.22
  ));
  const w = 960;
  const h = 200;
  const gap = 1.15;
  const barW = w / shown.length;
  return (
    <svg className="nk-face-fft" viewBox={`0 0 ${w} ${h}`} preserveAspectRatio="none" aria-hidden>
      {shown.map((v, i) => {
        const bh = Math.max(1.2, Math.min(h * 0.42, v * h * 9));
        return (
          <rect
            key={i}
            x={i * barW + gap * 0.35}
            y={h - bh}
            width={Math.max(0.7, barW - gap)}
            height={bh}
          />
        );
      })}
    </svg>
  );
}

function FaceMinimap({
  nodes,
  edges,
}: {
  nodes: ReturnType<typeof minimapGraph>["nodes"];
  edges: ReturnType<typeof minimapGraph>["edges"];
}) {
  const W = 196;
  const H = 64;
  const px = (x: number) => 10 + x * (W - 20);
  const py = (y: number) => 12 + y * (H - 24);
  return (
    <div className="nk-face-mini" aria-label="signal path">
      <div className="nk-face-mini-tag">SIGNAL_PATH</div>
      <svg viewBox={`0 0 ${W} ${H}`} width={W} height={H}>
        {edges.map((e, i) => {
          const a = nodes.find((n) => n.id === e.from);
          const b = nodes.find((n) => n.id === e.to);
          if (! a || ! b) {
            return null;
          }
          return (
            <line
              key={`${e.from}-${e.to}-${i}`}
              x1={px(a.x)}
              y1={py(a.y)}
              x2={px(b.x)}
              y2={py(b.y)}
              className={e.live ? "is-live" : "is-dead"}
            />
          );
        })}
        {nodes.map((n) => (
          <circle
            key={n.id}
            cx={px(n.x)}
            cy={py(n.y)}
            r={2.4}
            className={n.live ? "is-live" : "is-dead"}
          />
        ))}
      </svg>
    </div>
  );
}

function FaceKnob({
  knob,
}: {
  knob: { id: string; name: string; value: number; active: boolean; min: number; max: number; isNote: boolean; unit?: string; enums?: string[] };
}) {
  return (
    <div className="nk-face-knob h-[168px] w-[128px]">
      <span className="nk-face-aim nk-face-aim-tl" aria-hidden>+</span>
      <span className="nk-face-aim nk-face-aim-tr" aria-hidden>+</span>
      <span className="nk-face-aim nk-face-aim-bl" aria-hidden>+</span>
      <span className="nk-face-aim nk-face-aim-br" aria-hidden>+</span>
      <Knob knob={knob} bind={false} />
    </div>
  );
}
