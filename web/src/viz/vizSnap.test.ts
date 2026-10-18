import { afterEach, describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { ANALYZE_N } from "./audioAnalyze";
import { cachedBands } from "./vizSnap";

function tone(hz: number, sr: number, n: number, amp = 0.2): Float32Array {
  const out = new Float32Array(n);
  for (let i = 0; i < n; i += 1) {
    out[i] = amp * Math.sin((2 * Math.PI * hz * i) / sr);
  }
  return out;
}

afterEach(() => {
  cachedBands(-1, new Float32Array(16), 48000, false);
});

describe("cachedBands", () => {
  it("recomputes when sample rate changes on the same tick", () => {
    const low = tone(80, 44100, ANALYZE_N);
    const a = cachedBands(7, low, 44100, true);
    const b = cachedBands(7, low, 44100, true);
    expect(b.spec).toBe(a.spec);
    expect(b.bands.low).toBe(a.bands.low);
    const c = cachedBands(7, low, 96000, true);
    expect(c.bands.low).not.toBe(a.bands.low);
  });

  it("one analyze hop per new tick, not a second spectrumBins pass", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "vizSnap.ts"), "utf8");
    const body = src.slice(src.indexOf("export function cachedBands"), src.indexOf("let inputCache"));
    expect(body).toContain("feedAnalyze");
    expect(body).not.toContain("spectrumBins");
    expect(body).not.toContain("bandRms");
    expect(body).toContain("sr");
  });
});
