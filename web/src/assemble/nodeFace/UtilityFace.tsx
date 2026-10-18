import type { FaceProps } from "./faceKind";
import { utilityFaceModel, utilityMarks, utilityParamsPath } from "./utilityParams";

export function UtilityFace({ args }: FaceProps) {
  const model = utilityFaceModel(args);
  const marks = utilityMarks(args);
  return (
    <div
      className="nk-node-face"
      data-glyph="params"
      data-axis="params"
      data-gain={String(model.gain)}
      data-pan={String(model.pan)}
      data-polarity={model.polarity ? "on" : "off"}
      aria-label="calculated utility params"
    >
      <div className="nk-face-plot" data-axis="params">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="params" d={utilityParamsPath(args)} />
          <circle data-tap="gain" cx={marks.gain.x} cy={marks.gain.y} r="2.2" />
          <circle data-tap="pan" cx={marks.pan.x} cy={marks.pan.y} r="2.2" />
        </svg>
      </div>
    </div>
  );
}
