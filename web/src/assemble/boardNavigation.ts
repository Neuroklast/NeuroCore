export function boardGridStep(scale: number): number {
  return (
    32 *
    2 ** Math.max(0, Math.ceil(Math.log2(16 / Math.max(0.001, scale) / 32)))
  );
}
export function cableEnvelope(rms: number, peak: number) {
  const r = Number.isFinite(rms) ? Math.max(0, rms) : 0,
    p = Number.isFinite(peak) ? Math.max(0, peak) : 0;
  return {
    body: r <= 0.001 ? 0 : Math.min(0.65, Math.sqrt(r) * 0.65),
    contact: p <= 0.001 ? 0 : Math.min(1, Math.sqrt(p)),
    clip: p > 1,
  };
}
