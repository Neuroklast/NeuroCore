import type { FaceProps } from "./faceKind";
import { pitchFaceModel, pitchIntervalsPath, pitchStems } from "./pitchIntervals";

export function PitchFace({ type, args }: FaceProps) {
  const model = pitchFaceModel(type, args);
  return (
    <div
      className="nk-node-face"
      data-glyph="intervals"
      data-axis="interval"
      data-mode={model.mode}
      data-mix={String(model.mix)}
      data-st={String(model.semitones)}
      data-bands={String(model.bands)}
      aria-label="calculated pitch intervals"
    >
      <div className="nk-face-plot" data-axis="interval">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          {pitchStems(type, args).map((stem, i) => (
            <line key={`${stem.x.toFixed(2)}-${i}`} data-tap="interval" x1={stem.x} y1={40} x2={stem.x} y2={stem.y1} />
          ))}
          <path data-curve="intervals" d={pitchIntervalsPath(type, args)} />
        </svg>
      </div>
    </div>
  );
}
