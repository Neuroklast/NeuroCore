import type { FaceProps } from "./faceKind";
import { filterCurvePath, filterFaceModel } from "./filterResponse";

export function FilterFace({ type, args, host }: FaceProps) {
  const model = filterFaceModel(type, args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="response"
      data-axis="log-freq"
      data-mode={model.mode}
      data-fc={String(model.fc)}
      data-q={String(model.q)}
      aria-label="calculated filter response"
    >
      <div className="nk-face-plot" data-axis="log-freq">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="response" d={filterCurvePath(type, args, host)} />
        </svg>
      </div>
    </div>
  );
}
