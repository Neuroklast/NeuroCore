import { useEffect, useLayoutEffect, useRef, useState, type ReactNode } from "react";
import { hasJuceBridge } from "../bridge/juce";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { subscribeVizClock } from "../theme/vizClock";
import { formatHudFixed } from "./faceModel";
import { peakToDb } from "../bridge/telemetry";
import { measureFeatures } from "../viz/audioFeatures";
import {
  coreCallouts,
  coreWave,
  fieldStacks,
  formatViz,
  gonioDots,
  polyline,
  segsLit,
  specLogArea,
  specLogPath,
  specLogX,
  specTickHz,
  coreHud,
  coreViz,
  type CalloutGeom,
  type CoreViz,
} from "./coreDiagModel";
import { BALL_STATIC } from "../viz/ballModel";
import {
  CONTAINMENT_STATIC,
  containmentLayout,
  orbitShards,
  type ContainmentLayout,
  type OrbitShard,
  type TeleBox,
} from "../viz/containmentModel";
import { cachedBands, readViz } from "../viz/vizSnap";

type Frame = {
  viz: CoreViz;
  spec: number[];
  wave: number[];
  waveIn: number[];
  corr: number;
  gonio: Array<{ x: number; y: number }>;
  shards: OrbitShard[];
  cage: ContainmentLayout;
  sr: number;
};

const IDLE: Frame = {
  viz: coreViz(BALL_STATIC, CONTAINMENT_STATIC),
  spec: new Array(32).fill(0),
  wave: new Array(48).fill(0),
  waveIn: new Array(48).fill(0),
  corr: 0,
  gonio: [],
  shards: [],
  cage: containmentLayout(1256, 500),
  sr: 48000,
};

const PANE0 = { w: 1256, h: 500 };
const GONIO_DOTS = 48;
const ORBIT_DOTS = 10;

function writeLive(root: ParentNode | null, key: string, value: string): void {
  const el = root?.querySelector(`[data-nk-live="${key}"]`);
  if (el && el.textContent !== value) {
    el.textContent = value;
  }
}

function writeMode(root: ParentNode | null, mode: string): void {
  root?.querySelectorAll("[data-nk-mode]").forEach((el) => {
    if (el.getAttribute("data-mode") !== mode) {
      el.setAttribute("data-mode", mode);
    }
    if (el.textContent !== mode) {
      el.textContent = mode;
    }
  });
}

export function CoreDiagnostics({ live }: { live: boolean }) {
  const motion = useHostStore((s) => s.motion);
  const sr = useHostStore((s) => s.sr);
  const numAt = useRef(0);
  const paneRef = useRef(PANE0);
  const probeRef = useRef<HTMLDivElement>(null);
  const diagLRef = useRef<HTMLElement>(null);
  const diagRRef = useRef<HTMLElement>(null);
  const colsRef = useRef<TeleBox[]>([]);
  const frameRef = useRef<Frame>(IDLE);
  const specFillRef = useRef<SVGPathElement>(null);
  const specTraceRef = useRef<SVGPathElement>(null);
  const waveInRef = useRef<SVGPathElement>(null);
  const waveOutRef = useRef<SVGPathElement>(null);
  const [, setTick] = useState(0);

  useLayoutEffect(() => {
    const host = probeRef.current?.parentElement;
    const left = diagLRef.current;
    const right = diagRRef.current;
    if (! host || ! left || ! right) {
      return undefined;
    }
    const measure = () => {
      if (host.clientWidth < 1 || host.clientHeight < 1) return;
      paneRef.current = { w: host.clientWidth, h: host.clientHeight };
      const box = (el: HTMLElement): TeleBox => ({ x: el.offsetLeft, y: el.offsetTop, w: el.offsetWidth, h: el.offsetHeight });
      colsRef.current = [box(left), box(right), ...Array.from(host.querySelectorAll<HTMLElement>(".nk-face-tele"), box)];
      frameRef.current = { ...frameRef.current, cage: containmentLayout(host.clientWidth, host.clientHeight) };
      setTick(n => n + 1);
    };
    const ro = new ResizeObserver(measure);
    ro.observe(host);
    ro.observe(left);
    ro.observe(right);
    measure();
    return () => ro.disconnect();
  }, []);

  useEffect(() => {
    const paint = (now: number) => {
      const tel = useTelemetryStore.getState();
      const rate = sr > 0 ? sr : 48000;
      const snap = readViz();
      const pane = paneRef.current;
      let cage = frameRef.current.cage;
      if (cage.paneW !== pane.w || cage.paneH !== pane.h) {
        cage = containmentLayout(pane.w, pane.h);
      }
      const packed = cachedBands(tel.tick, tel.scopeOut, rate, live);
      const wave = coreWave(tel.scopeOut, live, 48);
      const waveIn = coreWave(tel.scopeIn, live, 48);
      specFillRef.current?.setAttribute("d", specLogArea(packed.spec, rate, 200, 66));
      specTraceRef.current?.setAttribute("d", specLogPath(packed.spec, rate, 200, 66));
      waveInRef.current?.setAttribute("d", polyline(waveIn, 200, 78, true));
      waveOutRef.current?.setAttribute("d", polyline(wave, 200, 78, true));
      if (now - numAt.current < 80 && numAt.current > 0) {
        return;
      }
      numAt.current = now;
      const features = measureFeatures(tel.scopeOut, tel.gonioL, tel.gonioR, rate, {
        validity: live ? (hasJuceBridge() ? "live" : "preview") : "unavailable",
        transient: tel.onsetAmp,
        source: "plugin.out",
        peak: tel.outPeak,
        rms: tel.outRms,
      });
      const viz = coreHud(features);
      const shards = orbitShards(cage, snap.pose.orbitPhase);
      const gonio = gonioDots(tel.gonioL, tel.gonioR, live, GONIO_DOTS);
      frameRef.current = {
        viz,
        spec: packed.spec,
        wave,
        waveIn,
        corr: features.corr,
        gonio,
        shards,
        cage,
        sr: rate,
      };
      const host = probeRef.current?.parentElement ?? null;
      const peak = live ? formatHudFixed(peakToDb(viz.peak), 1, 3) : "—";
      const rms = live ? formatHudFixed(peakToDb(viz.rms), 1, 3) : "—";
      const crest = live && viz.crestDb != null ? formatHudFixed(viz.crestDb, 1, 2) : "—";
      const hit = live ? formatViz(viz.transient) : "—";
      writeLive(host, "peak", peak);
      writeLive(host, "rms", rms);
      writeLive(host, "crest", crest);
      writeLive(host, "hit", hit);
      writeLive(host, "orbit-low", `LOW ${formatViz(viz.low)}`);
      writeLive(host, "orbit-mid", `MID ${formatViz(viz.mid)}`);
      writeLive(host, "orbit-high", `HIGH ${formatViz(viz.high)}`);
      writeLive(host, "corr", live ? formatHudFixed(viz.corr, 2, 1) : "—");
      writeLive(host, "balance", live ? formatHudFixed(viz.balance, 2, 1) : "—");
      writeLive(host, "call-core", viz.mode);
      writeLive(host, "call-core-peak", peak);
      writeLive(host, "call-ring", live ? `CORR ${formatHudFixed(viz.corr, 2, 1)}` : "CORR —");
      writeLive(host, "call-ring-bal", live ? formatHudFixed(viz.balance, 2, 1) : "—");
      writeMode(host, viz.mode);
      const stacks = fieldStacks(viz);
      for (const s of stacks) {
        writeLive(host, `stack-${s.id}`, formatViz(s.v));
        const lit = segsLit(s.v, 12);
        host?.querySelectorAll(`[data-nk-seg="${s.id}"] i`).forEach((el, i) => {
          const on = i < lit ? "1" : "0";
          if (el.getAttribute("data-on") !== on) {
            el.setAttribute("data-on", on);
          }
        });
      }
      const maxR = Math.max(1, cage.orbits[0]?.rx ?? 1);
      const k = 76 / maxR;
      host?.querySelectorAll("[data-nk-shard]").forEach((el, i) => {
        const s = shards[i];
        if (! s) {
          el.setAttribute("visibility", "hidden");
          return;
        }
        el.setAttribute("visibility", "visible");
        el.setAttribute("cx", String(90 + (s.x - cage.cx) * k));
        el.setAttribute("cy", String(36 + (s.y - cage.cy) * k));
        el.setAttribute("r", s.z < 0 ? "1" : "1.8");
        el.setAttribute("class", s.z < 0 ? "nk-face-mod-dot-dim" : "nk-face-mod-dot");
      });
      host?.querySelectorAll("[data-nk-gonio]").forEach((el, i) => {
        const p = gonio[i];
        if (! p) {
          el.setAttribute("visibility", "hidden");
          return;
        }
        el.setAttribute("visibility", "visible");
        el.setAttribute("cx", String(36 + p.x * 28));
        el.setAttribute("cy", String(36 - p.y * 28));
      });
    };
    if (motion === "off") {
      const cage = frameRef.current.cage.paneW === paneRef.current.w
        ? frameRef.current.cage
        : containmentLayout(paneRef.current.w, paneRef.current.h);
      frameRef.current = {
        ...IDLE,
        shards: orbitShards(cage, CONTAINMENT_STATIC.orbitPhase),
        cage,
        sr: sr > 0 ? sr : 48000,
      };
      setTick((n) => n + 1);
      return undefined;
    }
    return subscribeVizClock(paint);
  }, [live, motion, sr]);

  const f = frameRef.current;
  const ticks = specTickHz(f.sr);
  const stacks = fieldStacks(f.viz);
  const calls = coreCallouts(f.cage, colsRef.current);

  return (
    <>
      <div ref={probeRef} className="nk-face-probe" aria-hidden />
      <aside ref={diagLRef} className="nk-face-diag nk-face-diag-l" aria-label="core visualization">
        <Mod title="DYNAMICS" sub="dB">
          <div className="nk-face-mod-gridrow">
            <Row k="PEAK" live="peak" v={live ? formatHudFixed(peakToDb(f.viz.peak), 1, 3) : "—"} />
            <Row k="RMS" live="rms" v={live ? formatHudFixed(peakToDb(f.viz.rms), 1, 3) : "—"} />
            <Row k="CREST" live="crest" v={live && f.viz.crestDb != null ? formatHudFixed(f.viz.crestDb, 1, 2) : "—"} />
            <Row k="HIT" live="hit" v={live ? formatViz(f.viz.transient) : "—"} />
          </div>
          <div className="nk-face-mod-state" data-nk-mode="" data-mode={f.viz.mode}>{f.viz.mode}</div>
        </Mod>
        <Mod title="SPECTRUM">
          <svg className="nk-face-mod-svg nk-face-mod-svg-lg" viewBox="0 0 200 88" aria-hidden>
            <path d="M0 18 H200 M0 36 H200 M0 54 H200" className="nk-face-mod-grid" />
            <path ref={specFillRef} d={specLogArea(f.spec, f.sr, 200, 66)} className="nk-face-mod-fill" />
            <path ref={specTraceRef} d={specLogPath(f.spec, f.sr, 200, 66)} className="nk-face-mod-trace" />
            {ticks.map((hz) => {
              const x = specLogX(hz, f.sr) * 200;
              return (
                <g key={hz}>
                  <path d={`M ${x.toFixed(1)} 0 V 66`} className="nk-face-mod-grid" />
                  <text x={x.toFixed(1)} y="86" className="nk-face-mod-axis" textAnchor="middle">{labelHz(hz)}</text>
                </g>
              );
            })}
          </svg>
        </Mod>
        <Mod title="PARTICLE ORBITS">
          <OrbitMini cage={f.cage} shards={f.shards} />
          <div className="nk-face-orbit-leg">
            <span><i /><span data-nk-live="orbit-low">LOW {formatViz(f.viz.low)}</span></span>
            <span><i className="b" /><span data-nk-live="orbit-mid">MID {formatViz(f.viz.mid)}</span></span>
            <span><i className="c" /><span data-nk-live="orbit-high">HIGH {formatViz(f.viz.high)}</span></span>
          </div>
        </Mod>
      </aside>
      <aside ref={diagRRef} className="nk-face-diag nk-face-diag-r" aria-label="field visualization">
        <Mod title="BAND ENERGY" sub="FFT">
          <div className="nk-face-field-bars">
            {stacks.map((s) => (
              <SegStack key={s.id} id={s.id} v={s.v} kind={s.id === "LOW" ? "accent" : "cyan"} label={s.id} />
            ))}
            <div className="nk-face-seg-scale" aria-hidden>
              <span>1.0</span><span>0.5</span><span>0.0</span>
            </div>
            <div className="nk-face-bars-n">
              {stacks.map((s) => (
                <div key={s.id}><em>{s.id}</em><strong data-nk-live={`stack-${s.id}`}>{formatViz(s.v)}</strong></div>
              ))}
              <div className="nk-face-mod-state" data-nk-mode="" data-mode={f.viz.mode}>{f.viz.mode}</div>
            </div>
          </div>
        </Mod>
        <Mod title="SIGNAL FIELD">
          <svg className="nk-face-mod-svg nk-face-mod-svg-lg" viewBox="0 0 200 78" aria-hidden>
            <path d="M0 20 H200 M0 58 H200 M50 6 V72 M100 6 V72 M150 6 V72" className="nk-face-mod-grid" />
            <path d="M0 39 H200" className="nk-face-mod-zero" />
            <path ref={waveInRef} d={polyline(f.waveIn, 200, 78, true)} className="nk-face-mod-trace-dim" />
            <path ref={waveOutRef} d={polyline(f.wave, 200, 78, true)} className="nk-face-mod-trace" />
          </svg>
        </Mod>
        <Mod title="STEREO FIELD">
          <div className="nk-face-stereo">
            <svg className="nk-face-mod-svg nk-face-mod-round" viewBox="0 0 72 72" aria-hidden>
              <path d="M36 6 V66 M6 36 H66" className="nk-face-mod-grid" />
              <path d="M10 10 L62 62 M62 10 L10 62" className="nk-face-mod-grid" />
              {Array.from({ length: GONIO_DOTS }, (_, i) => {
                const p = f.gonio[i];
                return (
                  <circle
                    key={i}
                    data-nk-gonio={i}
                    cx={p ? 36 + p.x * 28 : 36}
                    cy={p ? 36 - p.y * 28 : 36}
                    r="1"
                    className="nk-face-mod-dot"
                    visibility={p ? "visible" : "hidden"}
                  />
                );
              })}
              <text x="36" y="10" className="nk-face-mod-axis" textAnchor="middle">MID</text>
              <text x="70" y="38" className="nk-face-mod-axis" textAnchor="end">SIDE</text>
            </svg>
            <Row k="CORR" live="corr" v={live ? formatHudFixed(f.corr, 2, 1) : "—"} />
          </div>
        </Mod>
      </aside>
      <svg
        className="nk-face-leads"
        viewBox={`0 0 ${f.cage.paneW} ${f.cage.paneH}`}
        preserveAspectRatio="none"
        aria-hidden
      >
        {calls.core.box.w > 0 && <path d={leadPath(calls.core, "core")} />}
        {calls.ring.box.w > 0 && <path d={leadPath(calls.ring, "ring")} />}
      </svg>
      <div
        className="nk-face-callout"
        hidden={calls.core.box.w === 0}
        style={{ left: calls.core.box.x, top: calls.core.box.y, width: calls.core.box.w, height: calls.core.box.h }}
      >
        <div className="nk-face-callout-box">
          <b>CORE</b>
          <span data-nk-live="call-core">{f.viz.mode}</span>
          <span><em>PEAK</em> <span data-nk-live="call-core-peak">{live ? formatHudFixed(peakToDb(f.viz.peak), 1, 3) : "—"}</span></span>
        </div>
      </div>
      <div
        className="nk-face-callout"
        hidden={calls.ring.box.w === 0}
        style={{ left: calls.ring.box.x, top: calls.ring.box.y, width: calls.ring.box.w, height: calls.ring.box.h }}
      >
        <div className="nk-face-callout-box">
          <b>RING</b>
          <span data-nk-live="call-ring">{live ? `CORR ${formatHudFixed(f.viz.corr, 2, 1)}` : "CORR —"}</span>
          <span><em>BAL</em> <span data-nk-live="call-ring-bal">{live ? formatHudFixed(f.viz.balance, 2, 1) : "—"}</span></span>
        </div>
      </div>
    </>
  );
}

function leadPath(c: CalloutGeom, which: "core" | "ring"): string {
  const x0 = which === "core" ? c.box.x + c.box.w : c.box.x;
  const y0 = c.box.y + c.box.h * 0.55;
  const mx = (x0 + c.tip.x) * 0.5;
  return `M ${x0.toFixed(1)} ${y0.toFixed(1)} L ${mx.toFixed(1)} ${y0.toFixed(1)} L ${c.tip.x.toFixed(1)} ${c.tip.y.toFixed(1)}`;
}

function labelHz(hz: number): string {
  return hz >= 1000 ? `${Math.round(hz / 1000)}k` : `${Math.round(hz)}`;
}

function OrbitMini({ cage, shards }: { cage: ContainmentLayout; shards: OrbitShard[] }) {
  const maxR = Math.max(1, cage.orbits[0]?.rx ?? 1);
  const k = 76 / maxR;
  return (
    <svg className="nk-face-mod-svg nk-face-mod-svg-lg" viewBox="0 0 180 72" aria-hidden>
      <circle cx="90" cy="36" r="8" className="nk-face-mod-orbit" />
      {cage.orbits.map((o, i) => (
        <ellipse
          key={i}
          cx="90"
          cy="36"
          rx={o.rx * k}
          ry={Math.max(4, o.ry * Math.cos(o.tilt) * k)}
          className={i === 0 ? "nk-face-mod-orbit" : "nk-face-mod-orbit nk-face-mod-orbit-b"}
        />
      ))}
      {Array.from({ length: ORBIT_DOTS }, (_, i) => {
        const s = shards[i];
        return (
          <circle
            key={i}
            data-nk-shard={i}
            cx={s ? 90 + (s.x - cage.cx) * k : 90}
            cy={s ? 36 + (s.y - cage.cy) * k : 36}
            r={s && s.z < 0 ? 1 : 1.8}
            className={s && s.z < 0 ? "nk-face-mod-dot-dim" : "nk-face-mod-dot"}
            visibility={s ? "visible" : "hidden"}
          />
        );
      })}
    </svg>
  );
}

function Mod({ title, sub, children }: { title: string; sub?: string; children: ReactNode }) {
  return (
    <div className="nk-face-mod">
      <div className="nk-face-mod-head">
        <div className="nk-face-mod-title">{title}</div>
        {sub ? <em>{sub}</em> : null}
      </div>
      {children}
    </div>
  );
}

function Row({ k, v, live }: { k: string; v: string; live?: string }) {
  return (
    <div className="nk-face-mod-row">
      <span>{k}</span>
      <span data-nk-live={live}>{v}</span>
    </div>
  );
}

function SegStack({ id, v, kind, label }: { id: string; v: number; kind: "accent" | "cyan"; label: string }) {
  const n = 12;
  const lit = segsLit(v, n);
  return (
    <span className="nk-face-seg-col" data-nk-seg={id}>
      <span className={`nk-face-seg nk-face-seg-${kind}`}>
        {Array.from({ length: n }, (_, i) => (
          <i key={i} data-on={i < lit ? "1" : "0"} />
        ))}
      </span>
      <span>{label}</span>
    </span>
  );
}
