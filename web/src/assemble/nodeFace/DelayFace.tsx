import type { FaceProps } from "./faceKind";
import { delayFaceModel, delayStemLines, delayTapsPath } from "./delayTaps";

export function DelayFace({ args, host }: FaceProps) {
  const model = delayFaceModel(args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="taps"
      data-axis="time"
      data-time={String(Math.round(model.timeMs))}
      data-fb={model.feedback.toFixed(2)}
      data-sync={model.synced ? "on" : "off"}
      data-pingpong={model.pingpong ? "on" : "off"}
      aria-label="calculated delay taps"
    >
      <div className="nk-face-plot" data-axis="time">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          {delayStemLines(args, host).map((stem) => (
            <line key={stem.x.toFixed(2)} data-tap="echo" x1={stem.x} y1={40} x2={stem.x} y2={stem.y1} />
          ))}
          <path data-curve="taps" d={delayTapsPath(args, host)} />
        </svg>
      </div>
    </div>
  );
}
