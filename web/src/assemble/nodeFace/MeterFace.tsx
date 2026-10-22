import type { CSSProperties } from "react";
import { useHostStore } from "../../store/hostStore";
import type { FaceProps } from "./faceKind";

/** A level display from this tap. No reconstructed waveform or fake loudness. */
export function MeterFace({ id = "", args }: FaceProps) {
  const peak = useHostStore(s => s.clips[id]);
  const rms = useHostStore(s => s.clipsRms[id]);
  return <MeterScreen peak={peak} rms={rms} mode={args.mode} />;
}

export function MeterScreen({peak, rms, mode}: {peak?: number; rms?: number; mode?: string}) {
  const db = (v: number | undefined) => v == null || !Number.isFinite(v) ? null : Math.max(-60, 20 * Math.log10(Math.max(0.001, v)));
  const peakDb = db(peak), rmsDb = db(rms);
  const text = (v: number | null) => v == null ? "—" : v.toFixed(1);
  return (
    <div className="nk-node-face nk-live-face" data-glyph="meter" aria-label="Measured peak and RMS">
      <div className="nk-meter-screen" data-meter-screen="">
        <div className="nk-meter-scale"><span>−60</span><span>−30</span><span>0 dBFS</span></div>
        {([["PK", peakDb], ["RMS", rmsDb]] as const).map(([label, value]) => (
          <div className="nk-meter-row" key={label}>
            <span>{label}</span>
            <div className="nk-meter-track"><i data-meter-bar={label}
              data-clip={value != null && value >= 0 ? "on" : "off"}
              style={{transform: `scaleX(${value == null ? 0 : Math.min(1, (value + 60) / 60)})`} as CSSProperties} /></div>
            <b>{text(value)}</b>
          </div>
        ))}
        <div className="nk-meter-mode">{mode === "loudness" ? "PEAK / RMS · dBFS" : `${(mode ?? "peak").toUpperCase()} · dBFS`}</div>
      </div>
    </div>
  );
}
