import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { barFillPercent, loudTitle, SCOPE_COLOR } from "./scopeModel";

function Bar({
  rms,
  peak,
  tag,
  color,
}: {
  rms: number;
  peak: number;
  tag: string;
  color: string;
}) {
  const fill = barFillPercent(rms);
  const tick = barFillPercent(peak);
  return (
    <div className="relative mx-0.5 flex min-h-0 flex-1 flex-col items-center">
      <div className="relative min-h-0 w-4 flex-1 bg-well">
        <div className="absolute bottom-0 w-full" style={{ height: `${fill}%`, background: color }} />
        <div className="absolute left-0 w-full bg-white/80" style={{ bottom: `${tick}%`, height: 2 }} />
      </div>
      <span className="text-[8px] text-muted">{tag}</span>
    </div>
  );
}

export function LoudnessMeter() {
  const inPeak = useTelemetryStore((s) => s.inPeak);
  const outPeak = useTelemetryStore((s) => s.outPeak);
  const inRms = useTelemetryStore((s) => s.inRms);
  const outRms = useTelemetryStore((s) => s.outRms);
  const source = useHostStore((s) => s.scopeSource);
  const showIn = source === "in" || source === "both";
  const showOut = source === "out" || source === "both";
  return (
    <div className="flex h-full w-[72px] shrink-0 flex-col border border-accent/55 px-1 py-1">
      <span className="text-center text-[9px] text-accent">{loudTitle(source)}</span>
      <div className="flex min-h-0 flex-1">
        {showIn ? <Bar rms={inRms} peak={inPeak} tag="IN" color={SCOPE_COLOR.in} /> : null}
        {showOut ? <Bar rms={outRms} peak={outPeak} tag="OUT" color={SCOPE_COLOR.out} /> : null}
      </div>
    </div>
  );
}
