import { useEffect, useRef, useState, type ReactNode } from "react";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { subscribeVizClock } from "../theme/vizClock";
import { formatHudFixed, stereoMetrics } from "./faceModel";
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

  useEffect(() => {
    const host = probeRef.current?.parentElement;
    const left = diagLRef.current;
    const right = diagRRef.current;
    if (! host || ! left || ! right) {
      return undefined;
    }
    const measure = () => {
      const face = host.getBoundingClientRect();
      if (face.width > 8 && face.height > 8) {
        paneRef.current = { w: face.width, h: face.height };
      }
      const box = (el: HTMLElement): TeleBox => {
        const r = el.getBoundingClientRect();
        return { x: r.left - face.left, y: r.top - face.top, w: r.width, h: r.height };
      };
      colsRef.current = [box(left), box(right)];
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
      const cage = containmentLayout(pane.w, pane.h);
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
      frameRef.current = {
        viz: coreViz(snap.drive, snap.pose),
        spec: packed.spec,
        wave,
        waveIn,
        corr: live ? stereoMetrics(tel.gonioL, tel.gonioR).corr : 0,
        gonio: gonioDots(tel.gonioL, tel.gonioR, live, 48),
        shards: orbitShards(cage, snap.pose.orbitPhase),
        cage,
        sr: rate,
      };
      setTick((n) => n + 1);
    };
    if (motion === "off") {
      const cage = containmentLayout(paneRef.current.w, paneRef.current.h);
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
        <Mod title="CORE STATUS" sub="VIZ">
          <div className="nk-face-mod-gridrow">
            <Row k="ACTIVITY" v={formatViz(f.viz.activity)} />
            <Row k="STABILITY" v={formatViz(f.viz.stability)} />
            <Row k="EXCITATION" v={formatViz(f.viz.excitation)} />
            <Row k="SYNC" v={formatViz(f.viz.sync)} />
          </div>
          <div className="nk-face-mod-state" data-mode={f.viz.mode}>{f.viz.mode}</div>
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
            <span><i />ORBIT 1 {formatViz(f.viz.activity)}</span>
            <span><i className="b" />ORBIT 2 {formatViz(f.viz.sync)}</span>
            <span><i className="c" />ORBIT 3 {formatViz(f.viz.excitation)}</span>
          </div>
        </Mod>
      </aside>
      <aside ref={diagRRef} className="nk-face-diag nk-face-diag-r" aria-label="field visualization">
        <Mod title="FIELD ACTIVITY" sub="VIZ">
          <div className="nk-face-bars">
            {stacks.map((s) => (
              <SegStack key={s.id} v={s.v} kind={s.id === "FIELD" ? "accent" : "cyan"} label={s.id} />
            ))}
            <div className="nk-face-seg-scale" aria-hidden>
              <span>1.0</span><span>0.5</span><span>0.0</span>
            </div>
            <div className="nk-face-bars-n">
              {stacks.map((s) => (
                <div key={s.id}><em>{s.id}</em><strong>{formatViz(s.v)}</strong></div>
              ))}
              <div className="nk-face-mod-state" data-mode={f.viz.mode}>{f.viz.mode}</div>
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
              {f.gonio.map((p, i) => (
                <circle key={i} cx={36 + p.x * 28} cy={36 - p.y * 28} r="1" className="nk-face-mod-dot" />
              ))}
              <text x="36" y="10" className="nk-face-mod-axis" textAnchor="middle">MID</text>
              <text x="70" y="38" className="nk-face-mod-axis" textAnchor="end">SIDE</text>
            </svg>
            <Row k="CORR" v={live ? formatHudFixed(f.corr, 2, 1) : "—"} />
          </div>
        </Mod>
      </aside>
      <svg
        className="nk-face-leads"
        viewBox={`0 0 ${f.cage.paneW} ${f.cage.paneH}`}
        preserveAspectRatio="none"
        aria-hidden
      >
        <path d={leadPath(calls.core, "core")} />
        <path d={leadPath(calls.ring, "ring")} />
      </svg>
      <div
        className="nk-face-callout"
        style={{ left: calls.core.box.x, top: calls.core.box.y, width: calls.core.box.w }}
      >
        <div className="nk-face-callout-box">
          <b>CORE</b>
          <span>{f.viz.mode}</span>
          <span><em>VIZ</em> {formatViz(f.viz.activity)}</span>
        </div>
      </div>
      <div
        className="nk-face-callout"
        style={{ left: calls.ring.box.x, top: calls.ring.box.y, width: calls.ring.box.w }}
      >
        <div className="nk-face-callout-box">
          <b>RING</b>
          <span>SYNC {formatViz(f.viz.sync)}</span>
          <span><em>VIZ</em> {formatViz(f.viz.stability)}</span>
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
      {shards.slice(0, 10).map((s, i) => (
        <circle
          key={i}
          cx={90 + (s.x - cage.cx) * k}
          cy={36 + (s.y - cage.cy) * k}
          r={s.z < 0 ? 1 : 1.8}
          className={s.z < 0 ? "nk-face-mod-dot-dim" : "nk-face-mod-dot"}
        />
      ))}
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

function Row({ k, v }: { k: string; v: string }) {
  return (
    <div className="nk-face-mod-row">
      <span>{k}</span>
      <span>{v}</span>
    </div>
  );
}

function SegStack({ v, kind, label }: { v: number; kind: "accent" | "cyan"; label: string }) {
  const n = 12;
  const lit = segsLit(v, n);
  return (
    <span className="nk-face-seg-col">
      <span className={`nk-face-seg nk-face-seg-${kind}`}>
        {Array.from({ length: n }, (_, i) => (
          <i key={i} data-on={i < lit ? "1" : "0"} />
        ))}
      </span>
      <span>{label}</span>
    </span>
  );
}
