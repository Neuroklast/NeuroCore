import type { FaceProps } from "./faceKind";
import { lfoFaceModel, lfoWaveformPath } from "./lfoWaveform";

export function LfoFace({ args }: FaceProps) {
  const model = lfoFaceModel(args);
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
          <path data-curve="waveform" d={lfoWaveformPath(args)} />
        </svg>
      </div>
    </div>
  );
}
