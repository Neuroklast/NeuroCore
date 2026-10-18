/** Calculated frequency-response well. The curve itself is PR C. */
export function FilterFace() {
  return (
    <div
      className="nk-node-face"
      data-glyph="response"
      data-axis="log-freq"
      aria-label="calculated filter response"
    >
      <div className="nk-face-plot" data-axis="log-freq" />
    </div>
  );
}
