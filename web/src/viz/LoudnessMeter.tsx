import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { LU_LINES, luLitLines, loudTitle, SCOPE_COLOR } from "./scopeModel";

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
  const lit = luLitLines(rms);
  const peakAt = luLitLines(peak);
  return (
    <div className="mx-1 flex min-h-0 flex-1 flex-col items-center">
      <div className="flex min-h-0 w-14 flex-1 flex-col-reverse">
        {Array.from({ length: LU_LINES }, (_, i) => {
          const on = i < lit;
          const isPeak = i + 1 === peakAt;
          const fade = i / Math.max(1, LU_LINES - 1);
          return (
            <div key={i} className="flex min-h-[2px] flex-1 items-center">
              <div
                className="w-full"
                style={{
                  height: 2,
                  background: isPeak ? "#f4f1ea" : on ? color : "rgba(244,241,234,0.1)",
                  opacity: isPeak ? 0.95 : on ? 0.3 + fade * 0.7 : 0.2,
                }}
              />
            </div>
          );
        })}
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
  useHostStore((s) => s.theme);
  const showIn = source === "in" || source === "both";
  const showOut = source === "out" || source === "both";
  return (
    <div className="flex h-full w-full min-h-0 flex-col px-1 py-1">
      <span className="text-center text-[9px] text-accent">{loudTitle(source)}</span>
      <div className="flex min-h-0 flex-1">
        {showIn ? <Bar rms={inRms} peak={inPeak} tag="IN" color={SCOPE_COLOR.in} /> : null}
        {showOut ? <Bar rms={outRms} peak={outPeak} tag="OUT" color={SCOPE_COLOR.out} /> : null}
      </div>
    </div>
  );
}
