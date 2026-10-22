import type { FaceProps } from "./faceKind";
import { adsrEnvelopePath, adsrFaceModel } from "./adsrEnvelope";
import { envEnvelopePath, envFaceModel } from "./envEnvelope";
import { parseFaceNumber } from "./faceArgs";

export function EnvFace({ type, args, host }: FaceProps) {
  if (type === "envtrigger") {
    const threshold = parseFaceNumber(args.thresholddb ?? args.threshold, -24, -60, 0, host);
    const y = 40 * (1 - (threshold + 60) / 60);
    const live = (host?.trigger ?? 0) > 0;
    return (
      <div
        className="nk-node-face"
        data-glyph="envelope"
        data-axis="time"
        data-trigger-live={live ? "on" : "off"}
        aria-label="envelope trigger threshold and pulse"
      >
        <div className="nk-face-plot" data-axis="time">
          <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
            <line x1="0" y1={y} x2="100" y2={y} stroke="var(--nk-ink-soft)" strokeDasharray="3 3" strokeWidth="1" />
            <rect x="0" y="0" width="100" height="40" fill="var(--nk-cyan)" opacity={live ? 0.14 : 0} />
            <path d="M0 40 L20 40 L20 4 L80 4 L80 40 L100 40" stroke="var(--nk-cyan)" fill="none" strokeWidth="1.2" />
          </svg>
        </div>
      </div>
    );
  }

  if (type === "adsr") {
    const adsr = adsrFaceModel(args, host);
    return (
      <div
        className="nk-node-face"
        data-glyph="envelope"
        data-axis="time"
        data-attack={String(adsr.attack)}
        data-decay={String(adsr.decay)}
        data-sustain={String(adsr.sustain)}
        data-release={String(adsr.release)}
        aria-label="calculated ADSR curve"
      >
        <div className="nk-face-plot" data-axis="time">
          <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
            <path data-curve="envelope" d={adsrEnvelopePath(args, host)} />
          </svg>
        </div>
      </div>
    );
  }

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
