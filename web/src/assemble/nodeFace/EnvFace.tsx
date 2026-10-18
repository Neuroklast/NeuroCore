import type { FaceProps } from "./faceKind";
import { envEnvelopePath, envFaceModel } from "./envEnvelope";

export function EnvFace({ args, host }: FaceProps) {
  const model = envFaceModel(args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="envelope"
      data-axis="time"
      data-attack={String(model.attack)}
      data-hold={String(model.hold)}
      data-release={String(model.release)}
      data-invert={model.invert ? "on" : "off"}
      data-unit={model.unit}
      data-source={model.source}
      aria-label="calculated envelope curve"
    >
      <div className="nk-face-plot" data-axis="time">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="envelope" d={envEnvelopePath(args, host)} />
        </svg>
      </div>
    </div>
  );
}
