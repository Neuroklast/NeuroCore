import { describe, expect, it } from "vitest";
import {
  deltaSamples,
  fieldTitle,
  gonioPoint,
  demoGonioLr,
  nextProcessMode,
  processModeIndex,
  sampleAtPx,
  SCOPE_COLOR,
  SCOPE_MENU,
  barFillPercent,
  luLitLines,
  LU_LINES,
  demoLoudness,
  scopeTitle,
  SPEC_BINS,
  SPEC_DEPTH,
  SPEC_PAD,
  specInner,
  specMag01,
  spectrogramProject,
  waveProject,
  specDbMarks,
  specRowFade,
  spectrogramPush,
  standingWaveRow,
  triggerAlign,
  logFreqMarks,
  logSpectrumBins,
  liftScopeMags,
  scopeYMarks,
  shouldPushScopeRow,
  shouldResetScopeHist,
  scopeSpectra,
  scopeRecordIds,
  techNoise,
  paintTechNoise,
  tracesFor,
} from "./scopeModel";

describe("scope deck model", () => {
  it("builds IN, OUT, BOTH and delta traces in distinct colours", () => {
    const inn = [0.2, 0.4];
    const out = [0.5, 0.1];
    expect(tracesFor("in", false, inn, out, 2).map((t) => t.id)).toEqual(["in"]);
    expect(tracesFor("out", false, inn, out, 2).map((t) => t.id)).toEqual(["out"]);
    const both = tracesFor("both", true, inn, out, 2);
    expect(both.map((t) => t.id)).toEqual(["in", "out", "delta"]);
    expect(both[0]?.color).toBe(SCOPE_COLOR.in);
    expect(both[1]?.color).toBe(SCOPE_COLOR.out);
    expect(both[2]?.color).toBe(SCOPE_COLOR.delta);
    expect(deltaSamples(out, inn, 2)[0]).toBeCloseTo(0.3);
    expect(sampleAtPx([0, 1], 2, 0, 100)).toBeCloseTo(0);
    expect(sampleAtPx([0, 1], 2, 99, 100)).toBeCloseTo(1);
    expect(sampleAtPx([0, 1], 2, 49.5, 100)).toBeCloseTo(0.5);
  });

  it("places log frequency marks denser at the low end", () => {
    const marks = logFreqMarks(48000);
    expect(marks.map((m) => m.label)).toEqual(expect.arrayContaining(["50", "100", "1k", "10k"]));
    expect(specDbMarks().map((m) => m.label)).toEqual(["0", "-24", "-48", "-72"]);
    const a = marks.find((m) => m.hz === 100)!;
    const b = marks.find((m) => m.hz === 1000)!;
    const c = marks.find((m) => m.hz === 10000)!;
    expect(Math.abs((b.bin - a.bin) - (c.bin - b.bin))).toBeLessThan(3);
  });

  it("puts a 200 Hz tone under the 200 tick and 4 kHz under 2k–5k, not both on the left wall", () => {
    const sr = 48000;
    const n = 256;
    const tone = (hz: number) => Float32Array.from({ length: n }, (_, i) => Math.sin((2 * Math.PI * hz * i) / sr));
    const peakOf = (row: number[]) => row.reduce((p, v, i) => (v > (row[p] ?? 0) ? i : p), 0);
    const marks = logFreqMarks(sr);
    const mark = (hz: number) => marks.find((m) => m.hz === hz)!;
    const bass = peakOf(logSpectrumBins(tone(187.5), sr, SPEC_BINS));
    const treble = peakOf(logSpectrumBins(tone(4000), sr, SPEC_BINS));
    expect(Math.abs(bass - mark(200).bin)).toBeLessThan(4);
    expect(bass).toBeGreaterThan(6);
    expect(treble).toBeGreaterThanOrEqual(mark(2000).bin - 2);
    expect(treble).toBeLessThanOrEqual(mark(5000).bin + 2);
    expect(treble).toBeGreaterThan(mark(200).bin + 8);
  });

  it("maps frequency height in dB even when Y is set to linear", () => {
    const row = liftScopeMags([0.03, 0.3, 1], "freq", "linear");
    expect(row[0]).toBeCloseTo(specMag01(0.03));
    expect(row[1]).toBeCloseTo(specMag01(0.3));
    expect(row[2]).toBeCloseTo(specMag01(1));
    const wave = liftScopeMags([0.1], "time", "linear");
    expect(wave[0]).toBeCloseTo(Math.min(1, 0.1 * 8));
  });

  it("keeps native titles and the old context-menu surface", () => {
    expect(scopeTitle("in", false)).toBe("IN // PRE");
    expect(scopeTitle("both", true)).toContain("Δ");
    expect(fieldTitle("out")).toBe("OUT FIELD");
    expect(SCOPE_MENU.x.map((x) => x.id)).toEqual(["samples", "time", "freq"]);
    expect(SCOPE_MENU.y.map((y) => y.id)).toEqual(["linear", "db"]);
    expect(SCOPE_MENU.flags.map((f) => f.id)).toEqual(["grid", "invertY", "delta"]);
    expect(SCOPE_MENU.source.map((s) => s.id)).toEqual(["in", "out", "both"]);
  });

  it("maps L/R to a mid/side goniometer, not raw x/y", () => {
    const mono = gonioPoint(0.8, 0.8);
    expect(mono.x).toBeCloseTo(0);
    expect(mono.y).toBeCloseTo(-0.8);
    const side = gonioPoint(0.6, -0.6);
    expect(side.x).toBeCloseTo(0.6);
    expect(side.y).toBeCloseTo(0);
    const demo = demoGonioLr(16, 64, 0);
    const p = gonioPoint(demo.l, demo.r);
    expect(Math.abs(p.x) + Math.abs(p.y)).toBeGreaterThan(0.15);
    expect(Math.abs(p.x)).toBeLessThan(0.8);
    expect(Math.abs(p.y)).toBeLessThan(0.9);
  });

  it("LU bar height follows live rms, not a parked constant", () => {
    expect(barFillPercent(0.6)).toBeGreaterThan(barFillPercent(0.05));
    expect(barFillPercent(0)).toBe(2);
    expect(luLitLines(0.6)).toBeGreaterThan(luLitLines(0.05));
    expect(luLitLines(0)).toBe(0);
    expect(luLitLines(1)).toBe(LU_LINES);
    const a = demoLoudness(0);
    const b = demoLoudness(30);
    expect(Math.abs(a.inPeak - b.inPeak) + Math.abs(a.outRms - b.outRms)).toBeGreaterThan(0.05);
  });

  it("plots IN and OUT as two colours when source is BOTH", () => {
    expect(scopeSpectra("in")).toEqual(["in"]);
    expect(scopeSpectra("out")).toEqual(["out"]);
    expect(scopeSpectra("both")).toEqual(["in", "out"]);
    expect(SCOPE_COLOR.in).not.toBe(SCOPE_COLOR.out);
    expect(scopeRecordIds()).toEqual(["in", "out"]);
  });

  it("does not wrap the trigger so the standing wave has no seam", () => {
    const raw = Float32Array.from({ length: 16 }, (_, i) => (i < 4 ? 0.4 : Math.sin(((i - 4) / 12) * Math.PI)));
    const aligned = triggerAlign(raw);
    expect(aligned[0]!).toBeLessThanOrEqual(0);
    expect(aligned[15]!).toBe(0);
    expect(aligned[0]!).not.toBe(raw[0]);
  });

  it("locks the rising edge and ignores zero chatter so the wave does not hunt", () => {
    const n = 64;
    const noisy = Float32Array.from({ length: n }, (_, i) => {
      const s = Math.sin((i / n) * Math.PI * 2) * 0.5;
      return s + (i < 6 ? (i % 2 === 0 ? -0.02 : 0.02) : 0);
    });
    const aligned = triggerAlign(noisy);
    expect(aligned[0]!).toBeLessThanOrEqual(0);
    expect(aligned[2]!).toBeGreaterThan(aligned[0]!);
    const a = triggerAlign(noisy);
    const b = triggerAlign(noisy);
    expect(a[0]).toBeCloseTo(b[0]);
    expect(a[8]).toBeCloseTo(b[8]);
  });

  it("keeps bipolar sample/time traces on the 3D floor, never below 0", () => {
    const w = 800;
    const h = 400;
    const floor = spectrogramProject(0, 0, 0, w, h);
    const zero = spectrogramProject(0, 0, 0.5, w, h);
    const peak = spectrogramProject(0, 0, 1, w, h);
    expect(floor.y).toBeGreaterThan(zero.y);
    expect(zero.y).toBeGreaterThan(peak.y);
    expect(waveProject(0, 0, 0, w, h).y).toBe(floor.y);
    expect(waveProject(0, 0, 0, w, h).y).toBeLessThanOrEqual(floor.y);
    const row = standingWaveRow(Float32Array.from([-1, 0, 1, 0.4]), SPEC_BINS, "linear");
    expect(Math.min(...row)).toBeGreaterThanOrEqual(0);
    expect(Math.max(...row)).toBeLessThanOrEqual(1);
  });

  it("uses bipolar amplitude marks for time/samples and dB for frequency", () => {
    expect(scopeYMarks("freq", "db").some((m) => m.label.includes("dB") || m.label === "0")).toBe(true);
    const wave = scopeYMarks("time", "linear");
    expect(wave.map((m) => m.label)).toEqual(["+1", "0", "−1"]);
    expect(shouldResetScopeHist("freq", "time")).toBe(true);
    expect(shouldResetScopeHist("time", "samples")).toBe(false);
    expect(shouldResetScopeHist("time", "time", "linear", "db")).toBe(true);
    expect(shouldResetScopeHist("freq", "freq", "db", "db")).toBe(false);
    expect(shouldPushScopeRow(4, 4)).toBe(false);
    expect(shouldPushScopeRow(5, 4)).toBe(true);
  });

  it("builds a sample/time row from the live buffer, not a locked trigger", () => {
    const n = 64;
    const a = Float32Array.from({ length: n }, (_, i) => Math.sin((i / n) * Math.PI * 2));
    const b = Float32Array.from({ length: n }, (_, i) => Math.sin((i / n) * Math.PI * 2 + 0.8));
    const ra = standingWaveRow(a, SPEC_BINS, "linear");
    const rb = standingWaveRow(b, SPEC_BINS, "linear");
    expect(ra.length).toBe(SPEC_BINS);
    expect(Math.max(...ra)).toBeGreaterThan(0.6);
    expect(Math.min(...ra)).toBeGreaterThanOrEqual(0);
    let diff = 0;
    for (let i = 0; i < ra.length; i += 1) {
      diff += Math.abs(ra[i]! - rb[i]!);
    }
    expect(diff).toBeGreaterThan(2);
  });

  it("maps spectrum height in dB so a loud but not-full-scale hit still fills", () => {
    expect(specMag01(1)).toBeGreaterThan(0.95);
    expect(specMag01(0)).toBe(0);
    expect(specMag01(0.03)).toBeGreaterThan(0.35);
    expect(specMag01(0.03)).toBeGreaterThan(specMag01(0.003) + 0.08);
    expect(specMag01(0.03)).toBeLessThan(specMag01(0.3));
  });

  it("rolls a spectrogram history and projects older rows into the distance", () => {
    let hist: number[][] = [];
    hist = spectrogramPush(hist, [1, 0.5, 0.1]);
    hist = spectrogramPush(hist, [0.2, 0.8, 0.4]);
    expect(hist[0]?.[1]).toBeCloseTo(0.8);
    expect(hist[1]?.[0]).toBeCloseTo(1);
    for (let i = 0; i < SPEC_DEPTH + 4; i += 1) {
      hist = spectrogramPush(hist, [0.1]);
    }
    expect(hist.length).toBe(SPEC_DEPTH);
    expect(hist[0]?.length).toBe(SPEC_BINS);
    const w = 800;
    const h = 400;
    const inner = specInner(w, h);
    const nearL = spectrogramProject(0, 0, 0, w, h);
    const nearR = spectrogramProject(SPEC_BINS - 1, 0, 0, w, h);
    const farL = spectrogramProject(0, SPEC_DEPTH - 1, 0, w, h);
    const farR = spectrogramProject(SPEC_BINS - 1, SPEC_DEPTH - 1, 0, w, h);
    expect(farL.y).toBeLessThan(nearL.y);
    expect(nearL.x).toBeGreaterThanOrEqual(SPEC_PAD.l);
    expect(nearL.y).toBeGreaterThan(inner.y + inner.h * 0.86);
    expect(farL.y).toBeGreaterThan(inner.y + inner.h * 0.24);
    expect(farL.y).toBeLessThan(inner.y + inner.h * 0.40);
    expect(nearL.y - farL.y).toBeGreaterThan(inner.h * 0.48);
    expect(nearL.y - farL.y).toBeLessThan(inner.h * 0.66);
    const dRow = spectrogramProject(0, 0, 0, w, h).y - spectrogramProject(0, 1, 0, w, h).y;
    expect(dRow).toBeLessThan(inner.h * 0.014);
    const nearW = nearR.x - nearL.x;
    const farW = farR.x - farL.x;
    expect(farW / nearW).toBeGreaterThan(0.68);
    expect(farW / nearW).toBeLessThan(0.82);
    expect(specRowFade(0)).toBeCloseTo(1);
    expect(specRowFade(SPEC_DEPTH - 1)).toBeLessThan(0.05);
    expect(specRowFade(Math.floor(SPEC_DEPTH / 2))).toBeLessThan(0.3);
    const mid = (SPEC_BINS - 1) / 2;
    const midNear = spectrogramProject(mid, 0, 0, w, h);
    const midFar = spectrogramProject(mid, SPEC_DEPTH - 1, 0, w, h);
    expect(Math.abs(midNear.x - inner.cx)).toBeLessThan(2);
    expect(Math.abs(midFar.x - inner.cx)).toBeLessThan(2);
    const zero = spectrogramProject(0, 0, 1, w, h);
    const floor = spectrogramProject(0, 0, 0, w, h);
    expect(zero.y).toBeLessThan(floor.y - 20);
    expect(techNoise(3, 7, 0)).toBeGreaterThanOrEqual(0);
    expect(techNoise(3, 7, 0)).toBeLessThanOrEqual(1);
    expect(techNoise(1, 1, 2) + techNoise(8, 4, 9)).toBeLessThan(2);
  });

  it("paints spectrograph speckle as sparse 1px cells", () => {
    const cells: Array<{ x: number; y: number; w: number; h: number }> = [];
    const ctx = {
      globalAlpha: 1,
      fillStyle: "",
      fillRect(x: number, y: number, w: number, h: number) {
        cells.push({ x, y, w, h });
      },
    };
    paintTechNoise(ctx as CanvasRenderingContext2D, 80, 40, 3, "#00f0ff");
    const slots = Math.ceil(78 / 7) * Math.ceil(38 / 5);
    expect(cells.length).toBeGreaterThan(0);
    expect(cells.length).toBeLessThan(slots * 0.2);
    expect(cells.every((c) => c.w === 1 && c.h === 1)).toBe(true);
  });

  it("STUDIO chip cycles LIVE, not Settings", () => {
    expect(nextProcessMode("STUDIO")).toBe("LIVE");
    expect(nextProcessMode("LIVE")).toBe("STUDIO");
    expect(nextProcessMode("SAFE")).toBe("LIVE");
    expect(processModeIndex("LIVE")).toBe(1);
    expect(processModeIndex("STUDIO")).toBe(0);
  });
});
