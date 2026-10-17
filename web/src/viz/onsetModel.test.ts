import { describe, expect, it } from "vitest";
import { onsetScan, packOnset, unpackOnset, ONSET_STATIC } from "./onsetModel";

function sine(n: number, amp: number, cycles: number): Float32Array {
  return Float32Array.from({ length: n }, (_, i) => amp * Math.sin((i / n) * Math.PI * 2 * cycles));
}

describe("onset scan", () => {
  it("silence never fires", () => {
    let s = ONSET_STATIC;
    let total = 0;
    for (let i = 0; i < 200; i += 1) {
      const hit = onsetScan(s, new Float32Array(256), 48000);
      s = hit.state;
      total += hit.count;
    }
    expect(total).toBe(0);
  });

  it("a single click is one event", () => {
    const click = new Float32Array(256);
    click[32] = 1;
    const hit = onsetScan(ONSET_STATIC, click, 48000);
    expect(hit.count).toBe(1);
    expect(hit.intensity).toBeGreaterThan(0.2);
    const again = onsetScan(hit.state, new Float32Array(256), 48000);
    expect(again.count).toBe(0);
  });

  it("regular kicks at 120 BPM stay separated", () => {
    const sr = 48000;
    const hop = Math.round(sr * 0.5);
    const block = 256;
    let s = ONSET_STATIC;
    let count = 0;
    const samples = new Float32Array(block);
    for (let t = 0; t < sr * 4; t += block) {
      samples.fill(0);
      const phase = t % hop;
      if (phase < block) {
        samples[0] = 1;
      }
      const hit = onsetScan(s, samples, sr);
      s = hit.state;
      count += hit.count;
    }
    expect(count).toBeGreaterThanOrEqual(7);
    expect(count).toBeLessThanOrEqual(9);
  });

  it("compressed high RMS still sees a click", () => {
    let s = ONSET_STATIC;
    const tone = sine(256, 0.55, 8);
    for (let i = 0; i < 24; i += 1) {
      s = onsetScan(s, tone, 48000).state;
    }
    const body = sine(256, 0.55, 8);
    body[40] = 1;
    const hit = onsetScan(s, body, 48000);
    expect(hit.count).toBe(1);
  });
});

describe("onset pack", () => {
  it("round-trips count and intensity in 16 bits", () => {
    const u = unpackOnset(packOnset(3, 0.5));
    expect(u.count).toBe(3);
    expect(u.intensity).toBeCloseTo(0.5, 1);
    expect(unpackOnset(0)).toEqual({ count: 0, intensity: 0 });
  });
});
