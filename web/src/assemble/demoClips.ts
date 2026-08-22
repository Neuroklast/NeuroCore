import { demoGonioLr, demoLoudness } from "../viz/scopeModel";

/** Vite stand-in for host `clips` rows. Same shape as `appendClipPeaks`. */
export function demoClipRows(
  ids: string[],
  t: number,
): Array<{ id: string; peak: number; peakL: number; peakR: number; rms: number; rmsL: number; rmsR: number }> {
  const { inPeak, outPeak, inRms, outRms } = demoLoudness(t);
  const lr = demoGonioLr(0, 1, t);
  const lAbs = Math.abs(lr.l);
  const rAbs = Math.abs(lr.r);
  const span = Math.max(lAbs, rAbs, 1e-3);
  const n = Math.max(1, ids.length - 1);
  return ids.map((id, i) => {
    const u = i / n;
    const peak = inPeak * (1 - u) + outPeak * u;
    const rms = inRms * (1 - u) + outRms * u;
    return {
      id,
      peak,
      peakL: peak * (lAbs / span),
      peakR: peak * (rAbs / span),
      rms,
      rmsL: rms * (lAbs / span),
      rmsR: rms * (rAbs / span),
    };
  });
}
