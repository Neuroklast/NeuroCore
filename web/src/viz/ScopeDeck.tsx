import { useEffect, useRef, useState } from "react";
import { createTelemetryViews, decodeTelemetry, SCOPE_N } from "../bridge/telemetry";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { menuPos } from "../theme/fit";
import { OsContextMenu } from "../overlays/OsContextMenu";
import { LoudnessMeter } from "./LoudnessMeter";
import { ScopeCanvas, ScopeMenu } from "./ScopeCanvas";
import { StereoField } from "./StereoField";
import { demoGonioLr, demoLoudness, type ScopeSource, type ScopeXScale, type ScopeYScale } from "./scopeModel";

function fillDemo(views: ReturnType<typeof createTelemetryViews>, t: number) {
  const n = SCOPE_N;
  for (let i = 0; i < n; i += 1) {
    const ph = (i / n) * Math.PI * 2 + t * 0.08;
    views.scopeIn[i] = Math.sin(ph) * 0.45 + Math.sin(ph * 3) * 0.18;
    views.scopeOut[i] = Math.tanh(Math.sin(ph) * 1.4) * 0.55
      + Math.sin(ph * 2) * 0.16
      + Math.sin(ph * 5) * 0.08;
  }
  for (let i = 0; i < views.gonioN; i += 1) {
    const lr = demoGonioLr(i, views.gonioN, t);
    views.gonioX[i] = lr.l;
    views.gonioY[i] = lr.r;
  }
  const lu = demoLoudness(t);
  views.inPeak = lu.inPeak;
  views.outPeak = lu.outPeak;
  views.inRms = lu.inRms;
  views.outRms = lu.outRms;
}

/** Keeps telemetryStore live on every workspace. Analyzers read the store. */
export function TelemetryPump({
  telemetryPath,
  intervalMs = 16,
}: {
  telemetryPath: string;
  intervalMs?: number;
}) {
  const views = useRef(createTelemetryViews()).current;
  const ms = Math.max(16, intervalMs);
  useEffect(() => {
    let live = true;
    let t = 0;
    const tick = async () => {
      while (live) {
        let ok = false;
        if (telemetryPath) {
          try {
            const res = await fetch(telemetryPath, { cache: "no-store" });
            if (res.ok) {
              const ab = await res.arrayBuffer();
              ok = decodeTelemetry(ab, views);
            }
          } catch {
            ok = false;
          }
        }
        if (! ok) {
          fillDemo(views, t);
          t += 1;
        }
        useTelemetryStore.getState().applyViews(views);
        await new Promise((r) => window.setTimeout(r, ms));
      }
    };
    void tick();
    return () => {
      live = false;
    };
  }, [telemetryPath, views, ms]);
  return null;
}

export function UnitAnalyzer() {
  const [menu, setMenu] = useState<{ left: number; top: number } | null>(null);
  const host = useRef<HTMLDivElement>(null);
  const sr = useHostStore((s) => s.sr);
  const scopeIn = useTelemetryStore((s) => s.scopeIn);
  const scopeOut = useTelemetryStore((s) => s.scopeOut);
  const gonioL = useTelemetryStore((s) => s.gonioL);
  const gonioR = useTelemetryStore((s) => s.gonioR);
  const tick = useTelemetryStore((s) => s.tick);
  void tick;

  const onMenu = (kind: "source" | "x" | "y" | "flag", id: string) => {
    if (kind === "source") {
      useHostStore.setState({ scopeSource: id as ScopeSource });
    } else if (kind === "x") {
      useHostStore.setState({ scopeX: id as ScopeXScale });
    } else if (kind === "y") {
      useHostStore.setState({ scopeY: id as ScopeYScale });
    } else if (id === "grid") {
      useHostStore.setState((s) => ({ scopeGrid: ! s.scopeGrid }));
    } else if (id === "invertY") {
      useHostStore.setState((s) => ({ scopeInvertY: ! s.scopeInvertY }));
    } else if (id === "delta") {
      useHostStore.setState((s) => ({ scopeDelta: ! s.scopeDelta }));
    }
    setMenu(null);
  };

  return (
    <div
      ref={host}
      className="relative flex h-full min-h-0 min-w-0 flex-1 items-stretch bg-black"
      onContextMenu={(e) => {
        e.preventDefault();
        const pane = host.current;
        if (! pane) {
          return;
        }
        setMenu(menuPos(e.clientX, e.clientY, pane, 200, 320));
      }}
    >
      <div className="flex min-h-0 min-w-0 flex-1 flex-col">
        <div className="min-h-0 flex-1" aria-hidden />
        <div className="nk-spec-fade min-h-0 flex-1">
          <ScopeCanvas scopeIn={scopeIn} scopeOut={scopeOut} count={scopeOut.length} sr={sr} />
        </div>
      </div>
      <div className="nk-unit-rule" aria-hidden />
      <aside className="nk-unit-meters flex h-full w-[168px] shrink-0 flex-col gap-2">
        <div className="aspect-square w-full shrink-0 overflow-hidden">
          <StereoField gonioL={gonioL} gonioR={gonioR} count={gonioL.length} />
        </div>
        <div className="min-h-0 min-w-0 flex-1">
          <LoudnessMeter />
        </div>
      </aside>
      {menu ? (
        <OsContextMenu left={menu.left} top={menu.top} title="METERS" onDismiss={() => setMenu(null)}>
          <ScopeMenu onPick={onMenu} />
        </OsContextMenu>
      ) : null}
    </div>
  );
}
