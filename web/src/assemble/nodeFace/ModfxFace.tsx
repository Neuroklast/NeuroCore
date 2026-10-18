import type { FaceProps } from "./faceKind";
import { modfxFaceModel, modfxWaveformPath } from "./modfxWaveform";

export function ModfxFace({ type, args }: FaceProps) {
  const model = modfxFaceModel(type, args);
  return (
    <div
      className="nk-node-face"
      data-glyph="waveform"
      data-axis="phase"
      data-mode={model.mode}
      data-rate={String(model.rate)}
      data-depth={String(model.depth)}
      data-delay={String(model.delay)}
      data-invert={model.invert ? "on" : "off"}
      aria-label="calculated modfx waveform"
    >
      <div className="nk-face-plot" data-axis="phase">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="waveform" d={modfxWaveformPath(type, args)} />
        </svg>
      </div>
    </div>
  );
}
