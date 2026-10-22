import type { CSSProperties } from "react";
import type { FaceProps } from "./faceKind";
import { readStepMutes, readStepValues, stepCountFromArgs } from "../stepSeqModel";
import { lfoFaceModel, lfoWaveformPath } from "./lfoWaveform";

export function LfoFace({ type, args, host }: FaceProps) {
  if (type === "stepseq") {
    const count = Math.max(1, Math.min(64, Math.round(host?.stepCount ?? stepCountFromArgs(args))));
    const live = host?.step ?? -1;
    const values = readStepValues(args, count);
    const mutes = readStepMutes(args, count);
    return (
      <div className="nk-node-face nk-live-face" data-glyph="steps" data-step-live={live} aria-label="Live sequencer steps">
        <div className="nk-step-leds" style={{ "--nk-step-rows": Math.ceil(count / 8) } as CSSProperties}>
          {values.map((v, i) => (
            <span key={i} data-step-tick={i} data-active={i === live ? "on" : "off"}
              data-muted={mutes[i] ? "on" : "off"} title={`Step ${i + 1}: ${v}`}
              style={{ "--nk-step-level": 0.2 + Math.abs(v) * 0.35 } as CSSProperties} />
          ))}
        </div>
      </div>
    );
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
