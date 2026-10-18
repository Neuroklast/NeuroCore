import type { FaceProps } from "./faceKind";
import { reverbDecayPath, reverbFaceModel } from "./reverbDecay";

export function ReverbFace({ args }: FaceProps) {
  const model = reverbFaceModel(args);
  return (
    <div
      className="nk-node-face"
      data-glyph="decay"
      data-axis="time"
      data-decay={String(model.decay)}
      data-rt60={model.rt60.toFixed(2)}
      aria-label="calculated decay envelope"
    >
      <div className="nk-face-plot" data-axis="time">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="decay" d={reverbDecayPath(args)} />
        </svg>
      </div>
    </div>
  );
}
