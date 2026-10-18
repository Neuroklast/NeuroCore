import { useEffect, useState } from "react";
import { perfEnabled, syncPerfFlag } from "./marks";
import { takePerfCounters } from "./counters";
import { snapshotPerfStats } from "./stats";

/** Opt-in overlay. Mounted only when nkperf is on; does not change product chrome. */
export function PerfHud() {
  const [on, setOn] = useState(false);
  const [text, setText] = useState("nkperf");
  useEffect(() => {
    syncPerfFlag();
    setOn(perfEnabled());
    if (! perfEnabled()) {
      return undefined;
    }
    const id = window.setInterval(() => {
      const s = snapshotPerfStats();
      const c = takePerfCounters();
      setText(
        `work ${s.work.p50.toFixed(2)}/${s.work.p95.toFixed(2)}/${s.work.p99.toFixed(2)} ms  `
        + `present ${s.present.p50.toFixed(1)}/${s.present.p95.toFixed(1)}  `
        + `gpu ${s.gpu.p50.toFixed(2)}/${s.gpu.p95.toFixed(2)}  `
        + `drops ${s.drops} draws ${c.draws} prog ${c.programs} resize ${c.resizes}`,
      );
    }, 500);
    return () => window.clearInterval(id);
  }, []);
  if (! on) {
    return null;
  }
  return (
    <div
      className="pointer-events-none fixed bottom-1 right-1 z-[80] max-w-[min(90vw,640px)] truncate px-2 py-1 font-mono text-[10px] tracking-wide text-[var(--nk-cyan)]"
      data-nk-perf="hud"
      aria-hidden
    >
      {text}
    </div>
  );
}
