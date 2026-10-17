import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import {
  ANALYZE_N,
  analyzeFrame,
  createAnalyze,
  feedAnalyze,
  resetAnalyze,
  SPEC_BINS,
} from "./audioAnalyze";
import { specMag01 } from "./scopeModel";

function tone(hz: number, sr: number, n: number, amp = 0.2): Float32Array {
  const out = new Float32Array(n);
  for (let i = 0; i < n; i += 1) {
    out[i] = amp * Math.sin((2 * Math.PI * hz * i) / sr);
  }
  return out;
}

function noise(n: number, amp = 0.2, seed = 1): Float32Array {
  const out = new Float32Array(n);
  let s = seed >>> 0;
  for (let i = 0; i < n; i += 1) {
    s = (s * 1664525 + 1013904223) >>> 0;
    out[i] = amp * ((s / 4294967296) * 2 - 1);
  }
  return out;
}

const SR = 44100;

describe("unit audio analyze", () => {
  it("80 Hz, 1 kHz and 8 kHz land in low / mid / high at 44.1 kHz", () => {
    const bass = analyzeFrame(tone(80, SR, ANALYZE_N), SR);
    const body = analyzeFrame(tone(1000, SR, ANALYZE_N), SR);
    const air = analyzeFrame(tone(8000, SR, ANALYZE_N), SR);
    expect(bass.spec).toHaveLength(SPEC_BINS);
    expect(bass.bands.low).toBeGreaterThan(bass.bands.mid);
    expect(bass.bands.low).toBeGreaterThan(bass.bands.high);
    expect(body.bands.mid).toBeGreaterThan(body.bands.low);
    expect(body.bands.mid).toBeGreaterThan(body.bands.high);
    expect(air.bands.high).toBeGreaterThan(air.bands.low);
    expect(air.bands.high).toBeGreaterThan(air.bands.mid);
    expect(Math.max(...bass.spec)).toBeGreaterThan(0.35);
  });

  it("maps amplitude with the −60 dBFS display floor, not a linear DFT peak", () => {
    const quiet = analyzeFrame(tone(1000, SR, ANALYZE_N, 0.12), SR);
    expect(Math.max(...quiet.spec)).toBeGreaterThan(specMag01(0.05));
    expect(Math.max(...quiet.spec)).toBeGreaterThan(0.4);
  });

  it("an impulse is broadband; noise lights every band; silence is zero", () => {
    const click = new Float32Array(ANALYZE_N);
    click[ANALYZE_N >> 1] = 1;
    const hit = analyzeFrame(click, SR);
    expect(hit.bands.low + hit.bands.mid + hit.bands.high).toBeGreaterThan(0.2);
    const hiss = analyzeFrame(noise(ANALYZE_N), SR);
    expect(hiss.bands.low).toBeGreaterThan(0.05);
    expect(hiss.bands.mid).toBeGreaterThan(0.05);
    expect(hiss.bands.high).toBeGreaterThan(0.05);
    const z = analyzeFrame(new Float32Array(ANALYZE_N), SR);
    expect(z.bands.low + z.bands.mid + z.bands.high).toBe(0);
    expect(z.spec.every((v) => v === 0)).toBe(true);
  });

  it("a held tone stays up and falls back when the ring goes quiet", () => {
    const s = createAnalyze();
    let last = feedAnalyze(s, tone(80, SR, 256), SR, 1 / 60);
    for (let i = 0; i < 12; i += 1) {
      last = feedAnalyze(s, tone(80, SR, 256, 0.2), SR, 1 / 60);
    }
    expect(last.bands.low).toBeGreaterThan(0.25);
    const held = last.bands.low;
    for (let i = 0; i < 4; i += 1) {
      last = feedAnalyze(s, tone(80, SR, 256, 0.2), SR, 1 / 60);
    }
    expect(Math.abs(last.bands.low - held)).toBeLessThan(0.15);
    resetAnalyze(s);
    const quiet = feedAnalyze(s, new Float32Array(256), SR, 1 / 60);
    expect(quiet.bands.low + quiet.bands.mid + quiet.bands.high).toBe(0);
  });

  it("a second hop on the same samples matches the first", () => {
    const x = tone(1000, SR, ANALYZE_N);
    const a = analyzeFrame(x, SR);
    const b = analyzeFrame(x, SR);
    expect(b.spec).toEqual(a.spec);
    expect(b.bands).toEqual(a.bands);
    expect(b.rms).toBe(a.rms);
  });

  it("analyzeFrame and feedAnalyze do not allocate Float32Array per hop", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "audioAnalyze.ts"), "utf8");
    const frame = src.slice(src.indexOf("export function analyzeFrame"), src.indexOf("export function feedAnalyze"));
    const feed = src.slice(src.indexOf("export function feedAnalyze"), src.indexOf("export function idleAnalyze"));
    expect(frame).not.toContain("new Float32Array");
    expect(feed).not.toContain("new Float32Array");
  });
});
