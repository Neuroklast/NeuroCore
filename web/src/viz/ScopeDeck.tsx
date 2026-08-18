import { useEffect, useRef, useState } from "react";
import { createTelemetryViews, decodeTelemetry, SCOPE_N } from "../bridge/telemetry";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { menuPos } from "../theme/fit";
import { OsContextMenu } from "../overlays/OsContextMenu";
import { LoudnessMeter } from "./LoudnessMeter";
import { ScopeCanvas, ScopeMenu } from "./ScopeCanvas";
import { StereoField } from "./StereoField";
import { demoLoudness, type ScopeSource, type ScopeXScale, type ScopeYScale } from "./scopeModel";

function fillDemo(views: ReturnType<typeof createTelemetryViews>, t: number) {
  const n = SCOPE_N;
  for (let i = 0; i < n; i += 1) {
    const ph = (i / n) * Math.PI * 2 + t * 0.08;
    views.scopeIn[i] = Math.sin(ph) * 0.55;
    views.scopeOut[i] = Math.tanh(Math.sin(ph) * 1.4) * 0.7;
  }
  for (let i = 0; i < views.gonioN; i += 1) {
    const ph = (i / views.gonioN) * Math.PI * 2 + t * 0.08;
    views.gonioX[i] = Math.sin(ph) * 0.55;
    views.gonioY[i] = Math.sin(ph + 0.35) * 0.48;
  }
  const lu = demoLoudness(t);
  views.inPeak = lu.inPeak;
  views.outPeak = lu.outPeak;
  views.inRms = lu.inRms;
  views.outRms = lu.outRms;
}

export function ScopeDeck({ telemetryPath }: { telemetryPath: string }) {
  const views = useRef(createTelemetryViews()).current;
  const [menu, setMenu] = useState<{ left: number; top: number } | null>(null);
  const host = useRef<HTMLDivElement>(null);
  const sr = useHostStore((s) => s.sr);

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
        await new Promise((r) => window.setTimeout(r, 16));
      }
    };
    void tick();
    return () => {
      live = false;
    };
  }, [telemetryPath, views]);

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
      className="relative flex h-[128px] shrink-0 items-stretch gap-2 border-t border-accent/40 bg-black px-2 py-1"
      onContextMenu={(e) => {
        e.preventDefault();
        const pane = host.current;
        if (! pane) {
          return;
        }
        setMenu(menuPos(e.clientX, e.clientY, pane, 200, 320));
      }}
    >
      <div className="min-w-0 flex-1 border border-accent/55">
        <ScopeCanvas scopeIn={views.scopeIn} scopeOut={views.scopeOut} count={views.scopeN} sr={sr} />
      </div>
      <div className="h-full w-[148px] shrink-0 border border-accent/55">
        <StereoField gonioL={views.gonioX} gonioR={views.gonioY} scopeIn={views.scopeIn} count={views.gonioN} />
      </div>
      <LoudnessMeter />
      {menu ? (
        <OsContextMenu left={menu.left} top={menu.top} title="METERS" onDismiss={() => setMenu(null)}>
          <ScopeMenu onPick={onMenu} />
        </OsContextMenu>
      ) : null}
    </div>
  );
}
