/** Calculated decay-envelope well. Not a measured impulse response. */
export function ReverbFace() {
  return (
    <div
      className="nk-node-face"
      data-glyph="decay"
      data-axis="time"
      aria-label="calculated decay envelope"
    >
      <div className="nk-face-plot" data-axis="time" />
    </div>
  );
}
