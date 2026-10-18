import type { FaceProps } from "./faceKind";
import { driveChannel, driveFaceModel, driveToneHz, driveTransferPath } from "./driveTransfer";

export function DriveFace({ type, args, host }: FaceProps) {
  const model = driveFaceModel(type, args, host);
  return (
    <div
      className="nk-node-face"
      data-glyph="transfer"
      data-axis="io"
      data-shaper={model.shaper}
      data-drive={String(model.drive)}
      data-mix={String(model.mix)}
      data-bits={String(model.bits)}
      data-tone={String(driveToneHz(args, host))}
      data-channel={driveChannel(args)}
      aria-label="calculated drive transfer"
    >
      <div className="nk-face-plot" data-axis="io">
        <svg viewBox="0 0 100 40" preserveAspectRatio="none" aria-hidden>
          <path data-curve="transfer" d={driveTransferPath(type, args, host)} />
        </svg>
      </div>
    </div>
  );
}
