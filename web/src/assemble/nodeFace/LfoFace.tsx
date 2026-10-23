import type { CSSProperties } from "react";
import type { FaceProps } from "./faceKind";
import { stepLedMatrix } from "../stepSeqModel";
import { parseFaceNumber } from "./faceArgs";
import { lfoFaceModel, lfoWaveformPath } from "./lfoWaveform";

export function LfoFace({ type, args, host }: FaceProps) {
  if (type === "stepseq") {
    const matrix=stepLedMatrix(args,host?.step??-1);
    return <div className="nk-node-face nk-live-face" data-glyph="steps" data-step-live={host?.step??-1} aria-label="Live sequencer steps">
      <div className="nk-step-leds" data-step-count={matrix.count} style={{"--nk-step-rows":matrix.rows} as CSSProperties}>
        {matrix.cells.map(cell=><span key={cell.index} data-step-tick={cell.index} data-state={cell.state}
          data-active={cell.active?"on":"off"} data-muted={cell.muted?"on":"off"}
          title={cell.state==="unused"?"Unused step":`Step ${cell.index+1}: ${cell.value}${cell.muted?" (muted)":""}`}
          style={{"--nk-step-level":.55+Math.abs(cell.value)*.3} as CSSProperties}><i /></span>)}
      </div>
    </div>;
  }

  if (type === "random") {
    const min = parseFaceNumber(args.min, -1, -1, 1, host);
    const max = parseFaceNumber(args.max, 1, -1, 1, host);
    return (
      <div className="nk-node-face" data-glyph="random" aria-label="Random modulation">
        <div className="nk-face-plot"><svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path d="M8 20h84" opacity="0.25" />
          <path d={`M8 ${20 - min * 12} H92 M8 ${20 - max * 12} H92`} opacity="0.45" />
          <path d="M14 28 22 12 30 24 38 10 46 26 54 14 62 30 70 11 78 25 86 16" />
        </svg></div>
      </div>
    );
  }

  if (type === "modmix") {
    const weighted = [
      host?.controlA == null ? undefined : host.controlA * parseFaceNumber(args.levela ?? args.levelA, 1, -4, 4, host),
      host?.controlB == null ? undefined : host.controlB * parseFaceNumber(args.levelb ?? args.levelB, 1, -4, 4, host),
      host?.controlOut,
    ];
    return <div className="nk-node-face" data-glyph="control-mix" aria-label="Live weighted A, B and output control">
      <div className="nk-face-plot"><svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
        {weighted.map((v, i) => <g key={i} data-mix-output={i === 2 ? v : undefined}>
          <title>{["A weighted", "B weighted", "Output"][i]}: {v == null ? "no measurement" : v.toFixed(3)}</title>
          <path d={`M50 ${8 + i * 12} H${50 + Math.max(-1, Math.min(1, v ?? 0)) * 46}`} opacity={v == null ? 0.2 : 1} />
          <path d={`M50 ${5 + i * 12} V${11 + i * 12}`} opacity="0.35" />
        </g>)}
      </svg></div>
    </div>;
  }

  const model = lfoFaceModel(args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="waveform"
      data-axis="phase"
      data-shape={model.shape}
      data-depth={String(model.depth)}
      data-hz={model.hz.toFixed(2)}
      aria-label="calculated LFO waveform"
    >
      <div className="nk-face-plot" data-axis="phase">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="waveform" d={lfoWaveformPath(args, host)} />
        </svg>
      </div>
    </div>
  );
}
