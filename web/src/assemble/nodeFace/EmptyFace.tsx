import type { FaceProps } from "./faceKind";

export function EmptyFace({}: FaceProps) {
  return <div className="nk-chip-pads nk-node-face" data-glyph="empty-pad" aria-hidden />;
}
