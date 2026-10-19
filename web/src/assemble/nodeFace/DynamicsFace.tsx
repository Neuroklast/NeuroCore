import type { FaceProps } from "./faceKind";
import { dynamicsCurvePath, dynamicsDetector, dynamicsFaceModel } from "./dynamicsCurve";

export function DynamicsFace({ type, args, host }: FaceProps) {
  const model = dynamicsFaceModel(type, args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="curve"
      data-axis="io"
      data-mode={model.mode}
      data-thr={String(model.threshold)}
      data-detector={dynamicsDetector(args)}
      aria-label="calculated dynamics curve"
    >
      <div className="nk-face-plot" data-axis="io">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="curve" d={dynamicsCurvePath(type, args, host)} />
        </svg>
      </div>
    </div>
  );
}
