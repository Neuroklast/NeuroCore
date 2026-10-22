import type { FaceProps } from "./faceKind";
import { readStepValues, stepCountFromArgs } from "../stepSeqModel";
import { lfoFaceModel, lfoWaveformPath } from "./lfoWaveform";

export function LfoFace({ type, args, host }: FaceProps) {
  if (type === "stepseq") {
    const count = Math.max(1, Math.min(64, Math.round(host?.stepCount ?? stepCountFromArgs(args))));
    const live = host?.step ?? -1;
    const values = readStepValues(args, count);
    return (
      <div className="nk-node-face" data-glyph="steps" data-step-live={live} aria-label="live step position">
        <div className="nk-face-plot flex items-center gap-px px-1" data-axis="step" style={{ height: "100%" }}>
          {values.map((v, i) => (
            <span
              key={i}
              data-step-tick={i}
              data-active={i === live ? "on" : "off"}
              style={{
                flex: 1,
                minWidth: 1,
                height: `${Math.max(6, Math.abs(v) * 100)}%`,
                alignSelf: v >= 0 ? "flex-start" : "flex-end",
                background: i === live ? "var(--nk-cyan)" : "var(--nk-ink-soft)",
                opacity: i === live ? 1 : 0.45,
              }}
            />
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
