import type { FaceProps } from "./faceKind";
import { ioFaceModel, ioJackDots, ioJackPath } from "./ioJacks";

export function IoFace({ type, args }: FaceProps) {
  const model = ioFaceModel(type, args);
  return (
    <div
      className="nk-node-face"
      data-glyph="jack"
      data-axis="jack"
      data-ins={model.ins.join(",")}
      data-outs={model.outs.join(",")}
      aria-label="calculated io jacks"
    >
      <div className="nk-face-plot" data-axis="jack">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="jack" d={ioJackPath(type, args)} />
          {ioJackDots(type, args).map((dot) => (
            <circle key={`${dot.id}-${dot.east ? "e" : "w"}`} data-jack={dot.id} cx={dot.x} cy={dot.y} r="2.4" />
          ))}
        </svg>
      </div>
    </div>
  );
}
